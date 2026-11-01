#ifndef DXX_XL_SHIELD_PULSE_H
#define DXX_XL_SHIELD_PULSE_H
#include <cstdint>

namespace xl_compat
{
// One clock is shared by all shield pulses, as in CPulseData::Update's static timeout
// Keep it for the process lifetime, including across level changes
struct ShieldPulseClock {
	bool initialized = false;
	int64_t start = 0;
	void initialize(uint32_t ticks)
	{
		if (!initialized) {
			start = ticks;
			initialized = true;
		}
	}
	bool expired(uint32_t ticks)
	{
		if (int64_t(ticks) - start < 25) return false;
		start = ticks;
		return true;
	}
};

struct ShieldPulse {
	float scale = .5f, minimum = .5f, direction = .02f, speed = .02f;
	int valid = -1;
	void setup(float new_speed = .02f, float new_minimum = .5f)
	{
		scale = minimum = new_minimum;
		speed = direction = new_speed;
		valid = 1;
	}
	void update(ShieldPulseClock &clock, uint32_t ticks)
	{
		clock.initialize(ticks);
		if (valid && clock.expired(ticks)) {
			scale += direction;
			if (scale > 1.f) {
				scale = 1.f;
				direction = -speed;
			} else if (scale < minimum) {
				scale = minimum;
				direction = speed;
			}
		}
	}
};
} // namespace xl_compat
#endif
