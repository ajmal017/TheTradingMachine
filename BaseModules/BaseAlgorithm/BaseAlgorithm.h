#pragma once

#include <string>
#include "Common.h"
#include "PlotData.h"

#ifdef BASEALGORITHM_EXPORTS
#define BASEALGORITHMDLL __declspec(dllexport)
#else
#define BASEALGORITHMDLL __declspec(dllimport)
#include <unordered_map>
#include <memory>

#define ALGORITHM_ARGS std::string input, std::shared_ptr<InteractiveBrokersClient> ibInst, bool live
#define BASEALGORITHM_PASS_ARGS input, ibInst, live

#define EXPORT_ALGORITHM(CLASSNAME) 																				\
/*boilerplate code for runtime dll linking*/																		\
static std::unordered_map<int, std::unique_ptr<CLASSNAME>> AlgorithmInstances; 										\
extern "C" 																											\
{ 																													\
	/* 																												\
	* int represents a handle to the instantiation of the algorithm corresponding to the input ticker  				\
	* this handle needs to be stored by the caller for destruction and calling algorithm 							\
	* specific functions. This is necessary because multiple tickers can be running on the same 					\
	* algorithm and we only have a single instance of the dll file 													\
	*/ 																												\
	__declspec(dllexport) int PlayAlgorithm(																		\
		std::string dataInput,																						\
		std::shared_ptr<PlotData>* dataOut,																			\
		std::shared_ptr<InteractiveBrokersClient> ibInst,															\
		bool live) 																									\
	{ 																												\
		return PlayAlgorithmT<CLASSNAME>(AlgorithmInstances, dataInput, dataOut, ibInst, live);						\
	} 																												\
																													\
	__declspec(dllexport) bool StopAlgorithm(int instHandle) 														\
	{ 																												\
		return StopAlgorithmT<CLASSNAME>(AlgorithmInstances, instHandle);											\
	} 																												\
}

// not really sure how to define these template functions outside of a macro
template<class Algorithm>
int PlayAlgorithmT(
	std::unordered_map<int, std::unique_ptr<Algorithm>>& algorithmInstances,
	std::string dataInput,
	std::shared_ptr<PlotData>* dataOut,
	std::shared_ptr<InteractiveBrokersClient> ibInst,
	bool live)
{
	static int uniqueInstanceHandles = 0;
	try
	{
		auto newInstance = std::make_unique<Algorithm>(dataInput, ibInst, live);
		*dataOut = newInstance->getPlotData();
		newInstance->run();
		algorithmInstances[uniqueInstanceHandles] = std::move(newInstance);
		return static_cast<int>(uniqueInstanceHandles++);
	}
	catch (const std::runtime_error&)
	{
		return -1;
	}
}

template<class Algorithm>
bool StopAlgorithmT(
	std::unordered_map<int, std::unique_ptr<Algorithm>>& algorithmInstances,
	int instHandle)
{
	if (algorithmInstances.find(instHandle) != algorithmInstances.end())
	{
		algorithmInstances[instHandle]->stop();
		algorithmInstances.erase(instHandle);
	}
	return true;
}

#endif

class BASEALGORITHMDLL BaseAlgorithm
{
public:
	BaseAlgorithm(std::string input, std::shared_ptr<InteractiveBrokersClient> ibApiPtr = std::shared_ptr<InteractiveBrokersClient>(nullptr), bool live = false);

	virtual ~BaseAlgorithm();
	std::shared_ptr<PlotData> getPlotData();

	// not meant to be overridden
	virtual void run() final;
	virtual void stop() final;

private:
	class BaseAlgorithmImpl;
	BaseAlgorithmImpl* impl_;
protected:
	
	//let the impl class call the pure virtual tickHandler
	friend BaseAlgorithmImpl;

	bool isRth(time_t time);

	//ordering api
	PositionId longMarket(std::string ticker, int numShares);
	PositionId longLimit(std::string ticker, double limitPrice, int numShares);
	
	PositionId shortMarket(std::string ticker, int numShares);
	PositionId shortLimit(std::string ticker, double limitPrice, int numShares);
	
	void closePosition(PositionId posId);
	void reducePosition(PositionId posId, int numShares);
	Position getPosition(PositionId posId);

	virtual void tickHandler(const Tick& tick) = 0;

	std::string ticker();
};
