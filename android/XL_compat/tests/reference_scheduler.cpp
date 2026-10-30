// Storage adapters for extracted arming/timing code, without Redux linkage
#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <stdexcept>
using fix = std::int32_t;
#define TF_PERMANENT 8
#define TF_DISABLED  4
#define TT_COUNTDOWN 25
#define TT_MESSAGE   29
#define TT_SOUND     30
#define MSEC2X(ms)   fix(float(ms) * (65536.0f / 1000.0f))
struct {
	struct {
		bool bD2XLevel = true;
	} app;
} gameStates;
struct {
	struct {
		std::int64_t xGame = 65536;
	} timeData;
} gameData;
int token = 0, random_calls = 0;
int Rand(int maximum)
{
	if (maximum != 10) throw std::runtime_error("Unexpected random bound");
	++random_calls;
	return token;
}
struct CTrigger {
	int flags = 8, times[2] = { -1, -1 }, fired = 0, type = 31;
	struct {
		std::int64_t tOperated = -1;
		int nObject = -1, nPlayer = -1, bShot = -1;
	} m_info;
	int Type() const
	{
		return type;
	}
	bool Flagged(int flag) const
	{
		return (flags & flag) != 0;
	}
	void SetFlags(int flag)
	{
		flags |= flag;
	}
	int GetTime(int slot) const
	{
		return times[slot];
	}
	void SetTime(int slot, int value)
	{
		times[slot] = value;
	}
	void Arm(int, int, int);
	bool IsDelayed();
	int32_t Delay();
	void Countdown(bool);
	void Operate(int object, int player, int shot, bool attached)
	{
		if (object != 7 || player != 2 || shot != 1 || attached) throw std::runtime_error("Stored actor differs");
		++fired;
	}
};
#include "xl_reference_scheduler.h"
int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Requires runtime scheduling samples");
		std::ifstream samples(argv[1]);
		int time, chosen, selected, pending, before, after, count = 0;
		while (samples >> time >> chosen >> selected >> pending >> before >> after) {
			CTrigger trigger;
			trigger.times[0] = time;
			token = chosen;
			random_calls = 0;
			gameData.timeData.xGame = 20 * 65536;
			// abs(INT_MIN) is undefined in the reference; test that bound independently
			if (time == INT32_MIN) {
				if (selected != -1 || pending) throw std::runtime_error("Unrepresentable delay admitted");
				++count;
				continue;
			}
			trigger.Arm(7, 2, 1);
			if (trigger.GetTime(1) != selected || (trigger.Delay() > 0) != bool(pending) || random_calls != (pending && time < 0 ? 1 : 0)) throw std::runtime_error("Reference initial delay mismatch");
			if (pending) {
				gameData.timeData.xGame += MSEC2X(selected) - 1;
				if (trigger.Delay() != before) throw std::runtime_error("Reference early boundary mismatch");
				trigger.Countdown(false);
				if (trigger.fired) throw std::runtime_error("Reference fired early");
				++gameData.timeData.xGame;
				if (trigger.Delay() != after) throw std::runtime_error("Reference due boundary mismatch");
				trigger.Countdown(false);
				if (trigger.fired != 1 || trigger.m_info.tOperated != -1 || trigger.flags != 8) throw std::runtime_error("Reference permanent completion mismatch");
			}
			++count;
		}
		if (!samples.eof() || count != 140) throw std::runtime_error("Missing scheduler samples");
		for (int type : { TT_COUNTDOWN, TT_MESSAGE, TT_SOUND })
			for (int time : { 99, 1000, -1000, 900000, INT32_MIN }) {
				CTrigger trigger;
				trigger.type = type;
				trigger.times[0] = time;
				random_calls = 0;
				trigger.Arm(7, 2, 1);
				if (trigger.IsDelayed() || trigger.Delay() != -1 || trigger.GetTime(0) != time || trigger.GetTime(1) != -1 || random_calls)
					throw std::runtime_error("Countdown/message/sound time entered the delayed scheduler");
			}
		std::cout << "Matched 130 runtime arming/timing samples plus 10 INT_MIN negative controls\n";
		std::cout << "Matched 15 countdown/message/sound scheduling exceptions\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
