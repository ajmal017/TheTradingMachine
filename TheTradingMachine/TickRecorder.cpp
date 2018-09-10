#include "TickRecorder.h"

std::string TimeToString(time_t time)
{
	char timeStr[256];
	ctime_s(timeStr, 256, &time);
	return std::string(timeStr);
}

TickRecorder::TickRecorder(std::string t) : TheTradingMachine(t), ticker(t)
{
	if (t.find(".tickdat") != std::string::npos)
	{
		throw std::invalid_argument("It is already a file. Aborting...");
		return;
	}

	//remove the newline 
	std::string filename = TimeToString(time(nullptr)).substr(4, 6) + ticker + ".tickdat";
	tickoutput.open(filename, std::ios::trunc | std::ios::out);

	std::cout << "requesting ticks for " << ticker << std::endl;
	requestTicks([this](const Tick& tick) {this->tickHandler(tick); });
}

TickRecorder::~TickRecorder()
{
	tickoutput.close();
}

void TickRecorder::tickHandler(const Tick & tick)
{
	std::string s = TimeToString(time(nullptr));
	tickoutput << tick.tickType << ',';
	tickoutput << tick.time << ",";
	tickoutput << s.substr(0, s.length() - 1) << ",";
	tickoutput << tick.price << ',';
	tickoutput << tick.size << ',';
	tickoutput << (int)tick.attributes.canAutoExecute << ',';
	tickoutput << (int)tick.attributes.pastLimit << ',';
	tickoutput << (int)tick.attributes.preOpen << ',';
	tickoutput << (int)tick.attributes.unreported << ',';
	tickoutput << (int)tick.attributes.bidPastLow << ',';
	tickoutput << (int)tick.attributes.askPastHigh << ',';
	tickoutput << tick.exchange << std::endl;

	std::cout << ticker << "\t" << tick.price << '\t' << tick.size << std::endl;
}