#include <thread>
#include <unordered_map>
#include <iostream>
#include <string>
#include "InteractiveBrokersClient.h"
#include "../InteractiveBrokersApi/InteractiveBrokersApi.h"
#include "../InteractiveBrokersApi/CommonDefs.h"
#include "../InteractiveBrokersApi/Order.h"
#include "../InteractiveBrokersApi/OrderSamples.h"
#include "../InteractiveBrokersApi/Execution.h"
#include "../InteractiveBrokersApi/Contract.h"

class InteractiveBrokersClient::InteractiveBrokersClientImpl
{
public:
	InteractiveBrokersClientImpl();
	~InteractiveBrokersClientImpl();
	InteractiveBrokersClientImpl(const InteractiveBrokersClientImpl& other) = delete;
	InteractiveBrokersClientImpl(InteractiveBrokersClientImpl&& other) = delete;

	int longMarket(std::string ticker, int numShares, std::function<void(double, time_t)> fillNotification);
	int longLimit(std::string ticker, double limitPrice, int numShares, std::function<void(double, time_t)> fillNotification);
	int shortMarket(std::string ticker, int numShares, std::function<void(double, time_t)> fillNotification);
	int shortLimit(std::string ticker, double limitPrice, int numShares, std::function<void(double, time_t)> fillNotification);

	void cancelOrder(int orderId);

	void unregisterFillNotification(int handle);

	int requestRealtimeTicks(std::string ticker, std::function<void(const Tick&)> callback);
	void cancelRealtimeTicks(std::string ticker, int handle);

	// 
	// This function is registered to the ibApi's actual requestRealTimeTicks. This function
	// will be called with an order id as the input and the new tick that came in. tickDispatcher
	// will then look for all the callbacks with the associated orderId and dispatch the tick
	// to all the callbacks with the new tick.
	//
	void tickDispatcher(OrderId oid, const Tick& tick);

	// orderStatus gets called whenever a placed order has been executed. will notify the
	// appropriate caller's callback
	void orderStatus(int reqId, const Contract & contract, const Execution & execution);

	bool isReady(void);

private:
	int callbackHandle; // used to uniquely create a handle each time requestRealTimeTicks is called
	InteractiveBrokersApi ibApi;
	std::thread messageProcessThread;
	std::atomic<bool> threadCancellationToken;
	void messageProcessThreadFn(void);

// members to handle routing data requests to the appropriate callback
private:
	// Tick Data
	// A call back function can be uniquely identified given a ticker and a handle
	std::unordered_map<std::string, OrderId> tickerOrderIds;
	std::unordered_map<OrderId, std::unordered_map<int, std::function<void(const Tick&)>>> tickCallbacks;
	std::mutex tickCallbacksMtx; //synchronizes message processing thread and request and cancel functions

	// Routing the proper order id to the appropriate order status update callback
	std::unordered_map<int, std::function<void(const Contract&, const Execution&)>> orderStatusCallbacks;
	std::mutex orderStatusCallbacksMtx; //synchronizes multiple threads submitting orders

// helpers
private:

	time_t stringToTime(const std::string& timeStr);
	Contract createUsContract(const std::string& ticker);
};

InteractiveBrokersClient::InteractiveBrokersClientImpl::InteractiveBrokersClientImpl():
	threadCancellationToken(false)
{
	callbackHandle = 0;
	std::cout << "Initializing IB Client" << std::endl;

	// if connection is established, then thread becomes the message processing thread.
	messageProcessThread = std::thread([this]()
	{
		int attempts = 0;
		const int MAX_ATTEMPTS = 50;
		while (!threadCancellationToken.load() && !ibApi.isConnected() && attempts < MAX_ATTEMPTS)
		{
			ibApi.connect("", 7496, 0);
			std::cout << "Connect attempt " << attempts << std::endl;
			Sleep(1000);
			attempts++;
		}

		if (attempts < MAX_ATTEMPTS)
		{
			std::cout << "Initialized IB Client. Message processing started..." << std::endl;

			//register our tick data dispatcher
			ibApi.registerOrderStatusCallback([this](int reqId, const Contract& contract, const Execution& execution) {this->orderStatus(reqId, contract, execution); });
			ibApi.registerRealtimeTickCallback([this](OrderId oid, const Tick& tick) {this->tickDispatcher(oid, tick); });
			messageProcessThreadFn();
		}
		else
		{
			std::cout << "Reached maximum number of attempts to connect."<< std::endl;
		}
	});
}

InteractiveBrokersClient::InteractiveBrokersClientImpl::~InteractiveBrokersClientImpl()
{
	threadCancellationToken = true;
	if (messageProcessThread.joinable())
	{
		messageProcessThread.join();
	}
	if (ibApi.isConnected())
	{
		ibApi.disconnect();
	}
}

int InteractiveBrokersClient::InteractiveBrokersClientImpl::longMarket(std::string ticker, int numShares, std::function<void(double, time_t)> fillNotification)
{
	// we want to make sure we place an order inside of the lock to prevent
	// the order from potentially being filled and orderStatus being called
	// before the lambda below is even registered. unlikely but still possible
	std::lock_guard<std::mutex> lockGuard(orderStatusCallbacksMtx);
	
	auto oid = ibApi.placeOrder(createUsContract(ticker), OrderSamples::MarketOrder("BUY", abs(numShares)));

	// this lambda be added to the map of order status update callbacks. when the order gets
	// executed, this callback associated with the order id will be called.
	orderStatusCallbacks[oid] = [=](const Contract& contract, const Execution& execution)
	{
		// we only support all or none orders
		if (execution.cumQty == numShares)
		{
			// return the system time for now. ideally we shouldd be using the server time
			// from execution.time. however, it is in string and we need
			// to account for dls ourselves.
			fillNotification(execution.avgPrice, time(nullptr));
		}
	};

	return oid;
}

int InteractiveBrokersClient::InteractiveBrokersClientImpl::longLimit(std::string ticker, double limitPrice, int numShares, std::function<void(double, time_t)> fillNotification)
{
	std::lock_guard<std::mutex> lockGuard(orderStatusCallbacksMtx);

	auto oid = ibApi.placeOrder(createUsContract(ticker), OrderSamples::LimitOrder("BUY", abs(numShares), limitPrice));

	// this lambda be added to the map of order status update callbacks. when the order gets
	// executed, this callback associated with the order id will be called.
	orderStatusCallbacks[oid] = [=](const Contract& contract, const Execution& execution)
	{
		// we only support all or none orders
		if (execution.cumQty == numShares)
		{
			fillNotification(execution.avgPrice, time(nullptr));
		}
	};

	return oid;
}

int InteractiveBrokersClient::InteractiveBrokersClientImpl::shortMarket(std::string ticker, int numShares, std::function<void(double, time_t)> fillNotification)
{

	std::lock_guard<std::mutex> lockGuard(orderStatusCallbacksMtx);

	auto oid = ibApi.placeOrder(createUsContract(ticker), OrderSamples::MarketOrder("SELL", abs(numShares)));

	// this lambda be added to the map of order status update callbacks. when the order gets
	// executed, this callback associated with the order id will be called.
	orderStatusCallbacks[oid] = [=](const Contract& contract, const Execution& execution)
	{
		// we only support all or none orders
		if (execution.cumQty == numShares)
		{
			fillNotification(execution.avgPrice, time(nullptr));
		}
	};

	return oid;
}

int InteractiveBrokersClient::InteractiveBrokersClientImpl::shortLimit(std::string ticker, double limitPrice, int numShares, std::function<void(double, time_t)> fillNotification)
{
	std::lock_guard<std::mutex> lockGuard(orderStatusCallbacksMtx);

	auto oid = ibApi.placeOrder(createUsContract(ticker), OrderSamples::LimitOrder("SELL", abs(numShares), limitPrice));

	// this lambda be added to the map of order status update callbacks. when the order gets
	// executed, this callback associated with the order id will be called. Refer to function
	// orderStatus
	orderStatusCallbacks[oid] = [=](const Contract& contract, const Execution& execution)
	{
		// we only support all or none orders
		if (execution.cumQty == numShares)
		{
			fillNotification(execution.avgPrice, time(nullptr));
		}
	};

	return oid;
}

void InteractiveBrokersClient::InteractiveBrokersClientImpl::cancelOrder(int orderId)
{
	std::lock_guard<std::mutex> lockGuard(orderStatusCallbacksMtx);

	// if the order exists, delete the order update callback and cancel the order 
	// in ibapi. we don't allow any on going orders if no algorithm is monitoring it
	if (orderStatusCallbacks.find(orderId) != orderStatusCallbacks.end())
	{
		ibApi.cancelOrder(orderId);
	}
}

void InteractiveBrokersClient::InteractiveBrokersClientImpl::unregisterFillNotification(int orderHandle)
{
	std::lock_guard<std::mutex> lockGuard(orderStatusCallbacksMtx);

	// if the order exists, delete the order update callback and cancel the order 
	// in ibapi. we don't allow any on going orders if no algorithm is monitoring it
	if (orderStatusCallbacks.find(orderHandle) != orderStatusCallbacks.end())
	{
		orderStatusCallbacks.erase(orderHandle);
	}
}

// requests from ib api for real time ticks. When real time ticks come in, callback will be called with the Tick data
// as the input. requestRealTimeTicks from ib api will return an order Id. We must keep the mapping of this order id to the
// appropriate ticker name and list of callbacks in order to call the proper callback functions.
int InteractiveBrokersClient::InteractiveBrokersClientImpl::requestRealtimeTicks(std::string ticker, std::function<void(const Tick&)> callback)
{
	if (ibApi.isReady())
	{
		OrderId oid;

		// we place the lock guard here to ensure that tickCallbacks[oid] will be
		// valid before tickdispatcher can call it.
		std::lock_guard<std::mutex> lock(tickCallbacksMtx);
		// if this ticker already has an order id, then a data stream already has been
		// requested before. otherwise, request a new data stream and assign the
		// orderid to the ticker
		if (tickerOrderIds.find(ticker) != tickerOrderIds.end())
		{
			oid = tickerOrderIds[ticker];
		}
		else
		{
			Contract contract;
			contract.symbol = ticker;
			contract.secType = "STK";
			contract.currency = "USD";
			contract.exchange = "SMART";
			// Specify the Primary Exchange attribute to avoid contract ambiguity
			// (there is an ambiguity because there is also a MSFT contract with primary exchange = "AEB")
			contract.primaryExchange = "ISLAND";

			// we should consider using bid ask instead for more realistic simulation prices
			// we put 0 for the number of ticks because we don't need historical ticks.
			oid = ibApi.requestRealtimeTicks(contract, "AllLast", 0, false);
			tickerOrderIds[ticker] = oid;
		}

		tickCallbacks[oid].insert(std::pair<int, std::function<void(const Tick&)>>(callbackHandle, callback));
		
		// return current value of handle and increment after
		return callbackHandle++;
	}

	return -1;
}

void InteractiveBrokersClient::InteractiveBrokersClientImpl::cancelRealtimeTicks(std::string ticker, int handle)
{
	// check if the provided ticker has an associated order id. can't cancel 
	// if it doesn't exist
	if (ibApi.isReady() && tickerOrderIds.find(ticker) != tickerOrderIds.end())
	{
		auto oid = tickerOrderIds[ticker];
		if(tickCallbacks.find(oid) != tickCallbacks.end())
		{		
			// read and modify entries under a lock because 
			// another thread is accessing the functions
			std::lock_guard<std::mutex> lock(tickCallbacksMtx);

			// erase doesn't throw exceptions
			tickCallbacks[oid].erase(handle);

			// if there are no more callbacks associated with the orderId,
			// delete the entry
			if (tickCallbacks[oid].size() == 0)
			{
				ibApi.cancelRealtimeTicks(oid);
				// For now, no success checking of cancelTickbyTickData before removing
				tickCallbacks.erase(oid);
				tickerOrderIds.erase(ticker);
			}
		}
	}
}

void InteractiveBrokersClient::InteractiveBrokersClientImpl::tickDispatcher(OrderId oid, const Tick & tick)
{
	// call all the associated functions under the lock because another thread
	// might be modifying the tickCallbacks 
	std::lock_guard<std::mutex> lockGuard(tickCallbacksMtx);
	for (const auto& fn : tickCallbacks.at(oid))
	{
		fn.second(tick);
	}
	
}

void InteractiveBrokersClient::InteractiveBrokersClientImpl::orderStatus(int reqId, const Contract& contract, const Execution& execution)
{
	// although reqId is unique for every context, std::unordered_map does not guarantee
	// no data race even if different elements accessed. we need synchronization to prevent ub
	std::lock_guard<std::mutex> lockGuard(orderStatusCallbacksMtx);
	//we don't need at here since reqId isn't a value from user.
	orderStatusCallbacks[reqId](contract, execution);
}

bool InteractiveBrokersClient::InteractiveBrokersClientImpl::isReady(void)
{	
	return ibApi.isReady();
}

void InteractiveBrokersClient::InteractiveBrokersClientImpl::messageProcessThreadFn(void)
{
	//shouldnt need to check for nulls here because we can only get here if these are not nullptr
	while (ibApi.isConnected() && !threadCancellationToken)
	{
		ibApi.processMessages();
		Sleep(10);
	}
}

time_t InteractiveBrokersClient::InteractiveBrokersClientImpl::stringToTime(const std::string & timeStr)
{
	// example time format "20181231  10:55:33"
	std::tm t;
	t.tm_sec = std::stoi(timeStr.substr(16, 2));
	t.tm_min = std::stoi(timeStr.substr(13, 2));
	t.tm_hour = std::stoi(timeStr.substr(10, 2));
	t.tm_mday = std::stoi(timeStr.substr(6, 2));
	t.tm_mon = std::stoi(timeStr.substr(4, 2)) - 1;
	t.tm_year = std::stoi(timeStr.substr(0, 4)) - 1900;
	t.tm_isdst = 0;

	return mktime(&t);

}

Contract InteractiveBrokersClient::InteractiveBrokersClientImpl::createUsContract(const std::string & ticker)
{
	Contract contract;
	contract.symbol = ticker;
	contract.secType = "STK";
	contract.currency = "USD";
	contract.exchange = "SMART";
	// Specify the Primary Exchange attribute to avoid contract ambiguity
	// (there is an ambiguity because there is also a MSFT contract with primary exchange = "AEB")
	contract.primaryExchange = "ISLAND";
	return contract;
}

InteractiveBrokersClient::InteractiveBrokersClient() :
	impl_(new InteractiveBrokersClientImpl)
{

}

InteractiveBrokersClient::~InteractiveBrokersClient()
{
	if (impl_ != nullptr)
	{
		delete impl_;
		impl_ = nullptr;
	}
}

int InteractiveBrokersClient::longMarket(std::string ticker, int numShares, std::function<void(double, time_t)> fillNotification)
{
	return impl_->longMarket(ticker, numShares, fillNotification);
}

int InteractiveBrokersClient::longLimit(std::string ticker, double limitPrice, int numShares, std::function<void(double, time_t)> fillNotification)
{
	return impl_->longLimit(ticker, limitPrice, numShares, fillNotification);
}

int InteractiveBrokersClient::shortMarket(std::string ticker, int numShares, std::function<void(double, time_t)> fillNotification)
{
	return impl_->shortMarket(ticker, numShares, fillNotification);
}

int InteractiveBrokersClient::shortLimit(std::string ticker, double limitPrice, int numShares, std::function<void(double, time_t)> fillNotification)
{
	return impl_->shortLimit(ticker, limitPrice, numShares, fillNotification);
}

int InteractiveBrokersClient::requestRealTimeTicks(std::string ticker, std::function<void(const Tick&)> callback)
{
	return impl_->requestRealtimeTicks(ticker, callback);
}

void InteractiveBrokersClient::cancelRealTimeTicks(std::string ticker, int handle)
{
	impl_->cancelRealtimeTicks(ticker, handle);
}

bool InteractiveBrokersClient::isReady(void)
{
	return impl_->isReady();
}

void InteractiveBrokersClient::unregisterFillNotification(int handle)
{
	impl_->unregisterFillNotification(handle);
}

INTERACTIVEBROKERSCLIENTDLL std::shared_ptr<InteractiveBrokersClient> GetInteractiveBrokersClient()
{
	static auto ibInterfaceInst = std::make_shared<InteractiveBrokersClient>();
	return ibInterfaceInst;
}