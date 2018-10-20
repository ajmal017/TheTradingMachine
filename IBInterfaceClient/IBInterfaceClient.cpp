// IBInterfaceClient.cpp : Defines the exported functions for the DLL application.
//

#include <thread>
#include <iostream>
#include "IBInterfaceClient.h"

class IBInterfaceClient::IBInterfaceClientImpl
{
public:
	IBInterfaceClientImpl();
	~IBInterfaceClientImpl();
	IBInterfaceClientImpl(const IBInterfaceClientImpl& other) = delete;
	IBInterfaceClientImpl(IBInterfaceClientImpl&& other) = delete;

	int requestRealTimeTicks(std::string ticker, std::function<void(const Tick&)> callback);
	bool cancelRealTimeTicks(std::string ticker, int handle);
	bool isReady(void);

	// Order functions
	void buyMarketNoStop(std::string ticker);
	void buyMarketStopMarket(std::string ticker, double activationPrice);
	void buyMarketStopLimit(std::string ticker, double activationPrice, double limitPrice);
	void buyLimitStopMarket(std::string ticker, double buyLimit, double activationPrice);
	void buyLimitStopLimit(std::string ticker, double buyLimit, double activationPrice, double limitPrice);

	void sellMarketNoStop(std::string ticker);
	void sellMarketStopMarket(std::string ticker, double activationPrice);
	void sellMarketStopLimit(std::string ticker, double activationPrice, double limitPrice);
	void sellLimitStopMarket(std::string ticker, double buyLimit, double activationPrice);
	void sellLimitStopLimit(std::string ticker, double buyLimit, double activationPrice, double limitPrice);

private:
	IBInterface ibApi;
	std::thread messageProcessThread;
	std::atomic<bool> threadCancellationToken;
	void messageProcessThreadFn(void);
};

IBInterfaceClient::IBInterfaceClientImpl::IBInterfaceClientImpl():
	threadCancellationToken(false)
{
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
			messageProcessThreadFn();
		}
		else
		{
			std::cout << "Reached maximum number of attempts to connect."<< std::endl;
		}
	});
}

IBInterfaceClient::IBInterfaceClientImpl::~IBInterfaceClientImpl()
{
	threadCancellationToken = true;

	std::cout << "impl destruct" << std::endl;
	if (messageProcessThread.joinable())
	{
		messageProcessThread.join();
	}
	if (ibApi.isConnected())
	{
		ibApi.disconnect();
	}
}

int IBInterfaceClient::IBInterfaceClientImpl::requestRealTimeTicks(std::string ticker, std::function<void(const Tick&)> callback)
{
	if (ibApi.isReady())
	{
		return ibApi.requestRealTimeTicks(ticker, callback);
	}
	return 0;
}

bool IBInterfaceClient::IBInterfaceClientImpl::cancelRealTimeTicks(std::string ticker, int handle)
{
	if (ibApi.isReady())
	{
		return ibApi.cancelRealTimeTicks(ticker, handle);
	}

	return false;
}

bool IBInterfaceClient::IBInterfaceClientImpl::isReady(void)
{	
	return ibApi.isReady();
}

void IBInterfaceClient::IBInterfaceClientImpl::buyMarketNoStop(std::string ticker)
{
}

void IBInterfaceClient::IBInterfaceClientImpl::buyMarketStopMarket(std::string ticker, double activationPrice)
{
}

void IBInterfaceClient::IBInterfaceClientImpl::buyMarketStopLimit(std::string ticker, double activationPrice, double limitPrice)
{
}

void IBInterfaceClient::IBInterfaceClientImpl::buyLimitStopMarket(std::string ticker, double buyLimit, double activationPrice)
{
}

void IBInterfaceClient::IBInterfaceClientImpl::buyLimitStopLimit(std::string ticker, double buyLimit, double activationPrice, double limitPrice)
{
}

void IBInterfaceClient::IBInterfaceClientImpl::sellMarketNoStop(std::string ticker)
{
}

void IBInterfaceClient::IBInterfaceClientImpl::sellMarketStopMarket(std::string ticker, double activationPrice)
{
}

void IBInterfaceClient::IBInterfaceClientImpl::sellMarketStopLimit(std::string ticker, double activationPrice, double limitPrice)
{
}

void IBInterfaceClient::IBInterfaceClientImpl::sellLimitStopMarket(std::string ticker, double buyLimit, double activationPrice)
{
}

void IBInterfaceClient::IBInterfaceClientImpl::sellLimitStopLimit(std::string ticker, double buyLimit, double activationPrice, double limitPrice)
{
}

void IBInterfaceClient::IBInterfaceClientImpl::messageProcessThreadFn(void)
{
	//shouldnt need to check for nulls here because we can only get here if these are not nullptr
	while (ibApi.isConnected() && !threadCancellationToken)
	{
		ibApi.processMessages();
		Sleep(10);
	}
}

IBInterfaceClient::IBInterfaceClient() :
	impl_(new IBInterfaceClientImpl)
{

}

IBInterfaceClient::~IBInterfaceClient()
{
	if (impl_ != nullptr)
	{
		delete impl_;
		impl_ = nullptr;
	}
}

int IBInterfaceClient::requestRealTimeTicks(std::string ticker, std::function<void(const Tick&)> callback)
{
	return impl_->requestRealTimeTicks(ticker, callback);
}

bool IBInterfaceClient::cancelRealTimeTicks(std::string ticker, int handle)
{
	return impl_->cancelRealTimeTicks(ticker, handle);
}

bool IBInterfaceClient::isReady(void)
{
	return impl_->isReady();
}

IBINTERFACECLIENTDLL std::shared_ptr<IBInterfaceClient> GetIBInterfaceClient()
{
	std::cout << "GetIBInterfaceClient" << std::endl;
	static auto ibInterfaceInst = std::make_shared<IBInterfaceClient>();
	return ibInterfaceInst;
}