// PatternBreak.cpp : Defines the exported functions for the DLL application.
//

#include "stdafx.h"

#include "../../BaseModules/BaseAlgorithm/BaseAlgorithm.h"
#include "../../BaseModules/CandleMaker/CandleMaker.h"
#include "../../BaseModules/Indicators/SimpleMovingAverage.h"
#include "../../BaseModules/Indicators/ExponentialMovingAverage.h"
#include "../../BaseModules/Indicators/BollingerBands.h"


#include <windows.h>
#include <iostream>

#pragma comment(lib, "../../BaseModules/x64/Debug/BaseAlgorithm.lib")
#pragma comment(lib, "../../BaseModules/x64/Debug/CandleMaker.lib")
#pragma comment(lib, "../../BaseModules/x64/Debug/Indicators.lib")

class PatternBreak : public BaseAlgorithm
{
public:
	PatternBreak(ALGORITHM_ARGS);
	~PatternBreak();

private:
	CandleMaker candleMaker;
	SimpleMovingAverage priceSma;
	SimpleMovingAverage dPriceSmaSma;
	
	std::vector<Candlestick> candles;
	std::vector<double> priceSmaValues;
	std::vector<double> dpriceSmaSmaValues;

	std::vector<Candlestick> supportCandles;
	std::vector<Candlestick> resistanceCandles;

	int supportNum = 0;
	int resistanceNum = 0;

	SimpleMovingAverage volumeSma5;
	std::vector<double> volumeSma5Values;

	std::list<int> positionHistory;
	int posId = -1;

	// stopping conditions: target hit, stoploss hit, time limit hit
	double stopPrice = -1;
	double entryPrice = -1;
	double riskPerShare = -1;
	int numShares = 0;
	int numReduction = 0;
	time_t targetTime = -1;

	double netProfit = 0;
	bool eod = true;
	void tickHandler(const Tick& tick);

	// finds support and resistance based on updated candles member
	void updateSupportAndResistance(const Candlestick& candle);

	// gets the top and bottom of the BODY of a candle. not the high and low.
	// if a candle was green, then the bottom is the open and top is the close.
	// opposite applies for red candles
	double getCandleTop(const Candlestick& candle);
	double getCandleBottom(const Candlestick& candle);
};

PatternBreak::PatternBreak(ALGORITHM_ARGS) :
	BaseAlgorithm(BASEALGORITHM_PASS_ARGS),
	priceSma(5),
	dPriceSmaSma(5),
	candleMaker(60),
	volumeSma5(5)
{
}

PatternBreak::~PatternBreak()
{
}

void PatternBreak::tickHandler(const Tick & tick)
{
	candleMaker.addTick(tick);
	auto closedCandles = candleMaker.getClosedCandles();
	auto currentCandle = candleMaker.getCurrentCandle();
	// supports are updated by candles
	for (auto& candle: closedCandles)
	{
		updateSupportAndResistance(candle);
	}

	// trades are made in ticks time frame
	if (isRth(tick.time))
	{
		eod = false;
		// check if any conditions has been hit for the current position
		// we only deal with one position at a time
		if (posId != -1)
		{
			// keep stop price adjusted to the latest resistance candle
			stopPrice = getCandleTop(resistanceCandles.back());
			
			// reduce half of position for when profit = risk. (selling into profit)
			if (tick.price <= entryPrice - riskPerShare && numReduction == 0)
			{
				reducePosition(posId, numShares/2);
				++numReduction;
			}
			else if (tick.price >= stopPrice)
			{
				positionHistory.push_back(posId);
				closePosition(posId);
				posId = -1;
			}
		}
		// support/resistanceCandles get cleared and repopulated with the latest
		// support/resistance. We use empty to check whether there is one
		else if (supportCandles.size() > 0 && resistanceCandles.size() > 1)
		{
			auto supportCandle = supportCandles.back();
			auto resistanceCandle = resistanceCandles.back();
			
			// check for support breach and lower highs. the most recent resistance is lower than the second to
			// the last most recent resistance
			if ( posId == -1 && tick.price < getCandleBottom(supportCandle) && getCandleTop(resistanceCandle) < getCandleTop(resistanceCandles[resistanceCandles.size() - 2]))
			{
				// only trade if there is an update to support/resistance 
				if(supportNum != supportCandles.size() || resistanceNum != resistanceCandles.size())
				{
					supportNum = supportCandles.size();
					resistanceNum = resistanceCandles.size();

					entryPrice = tick.price;
					stopPrice = getCandleTop(resistanceCandle);
					riskPerShare = stopPrice - tick.price;
					numShares = static_cast<int>(100 / riskPerShare);
					numReduction = 0;
					posId = shortMarket(ticker(), numShares);
				}
			}
		}
	}
	else
	{
		if (posId != -1)
		{
			closePosition(posId);
			positionHistory.push_back(posId);
			posId = -1;

		}
		else if (!eod)
		{
			auto numTradesProfitable = 0;
			double dailyProfit = 0;
			//print stats
			for (auto pastPosId : positionHistory)
			{
				auto pastPosition = getPosition(pastPosId);
				dailyProfit += pastPosition.profit;
				if (pastPosition.profit > 0)
				{
					++numTradesProfitable;
				}
			}
			// print each eod
			std::cout << "Num trades: " << positionHistory.size() << " Profitable: " << numTradesProfitable << " Profit: " << dailyProfit <<std::endl;
			positionHistory.clear();

			eod = true;
		}
	}
}

void PatternBreak::updateSupportAndResistance(const Candlestick& candle)
{
	candles.push_back(candle);
	priceSmaValues.emplace_back(priceSma.computeIndicatorPoint(candle.close)[SimpleMovingAverage::MOVING_AVERAGE]);

	// needs two or more points to start calculating slopes
	if (priceSmaValues.size() >= 2)
	{
		const auto dPrice = priceSmaValues.back() - priceSmaValues[priceSmaValues.size() - 2];
		dpriceSmaSmaValues.emplace_back(dPriceSmaSma.computeIndicatorPoint(dPrice)[SimpleMovingAverage::MOVING_AVERAGE]);
		if (dpriceSmaSmaValues.size() >= 2)
		{
			const auto& currCandle = candles.back();
			const auto& prevCandle = candles[candles.size() - 2];

			const auto& prevdPriceSma = dpriceSmaSmaValues[dpriceSmaSmaValues.size() - 2];
			const auto& currdPriceSma = dpriceSmaSmaValues.back();

			// multiplying values by 5 to increase visibility. the values
			// don't matter anyway since we only care about zero crossings
			auto line = std::make_shared<Annotation::Line>(prevCandle.time, prevdPriceSma * 5, currCandle.time, currdPriceSma * 5, 2);
			line->color_ = { 255, 0, 0 }; //red

			getPlotData()->annotations.push_back(line);
			// zero crossings
			if (prevdPriceSma < 0 && currdPriceSma > 0)
			{
				// this is a local min, look for the support
				decltype(candles.size()) count = 0;
				auto rIt = candles.rbegin();
				auto minCandle = currCandle;
				for (; rIt != candles.rend() && count < 10; ++rIt, ++count)
				{
					if (getCandleBottom(*rIt) < getCandleBottom(minCandle))
					{
						minCandle = *rIt;
					}
				}

				double price = getCandleBottom(minCandle);
				supportCandles.push_back(minCandle);

				auto box = std::make_shared<Annotation::Box>(minCandle.time, minCandle.high, minCandle.time + 60, minCandle.low, 0);
				box->color_ = { 0, 255, 0 }; //green for supports
				getPlotData()->annotations.push_back(box);

			}
			else if (prevdPriceSma > 0 && currdPriceSma < 0)
			{
				// this is a local max, look for the resistance
				decltype(candles.size()) count = 0;
				auto rIt = candles.rbegin();
				auto maxCandle = currCandle;
				for (; rIt != candles.rend() && count < 10; ++rIt, ++count)
				{
					if (getCandleTop(*rIt) > getCandleTop(maxCandle))
					{
						maxCandle = *rIt;
					}
				}

				double price = getCandleTop(maxCandle);
				resistanceCandles.push_back(maxCandle);

				auto box = std::make_shared<Annotation::Box>(maxCandle.time, maxCandle.high, maxCandle.time + 60, maxCandle.low, 0);
				box->color_ = { 255, 0, 0 }; //red for resistances
				getPlotData()->annotations.push_back(box);

			}
		}

	}


}

double PatternBreak::getCandleTop(const Candlestick & candle)
{
	double top = 0;
	candle.close <= candle.open ? top = candle.open : top = candle.close;
	return top;
}

double PatternBreak::getCandleBottom(const Candlestick & candle)
{
	double bottom = 0;
	candle.close <= candle.open ? bottom = candle.close : bottom = candle.open;
	return bottom ;
}


EXPORT_ALGORITHM(PatternBreak)