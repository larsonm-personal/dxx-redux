namespace shield_pulse_source
{
static uint32_t ticks = 0;
static uint32_t SDL_GetTicks()
{
	return ticks;
}
#define G3_SLEEP(t) throw std::runtime_error("Unexpected source pulse throttle")
#include "xl_reference_shield_pulse.h"
#undef G3_SLEEP
} // namespace shield_pulse_source

static void compare_shield_pulse()
{
	std::array<shield_pulse_source::CPulseData, 4> source;
	std::array<xl_compat::ShieldPulse, 4> actual;
	xl_compat::ShieldPulseClock clock;
	source[1].Setup(.1f, .5f);
	actual[1].setup(.1f, .5f);
	source[2].Setup(.02f, .4f);
	actual[2].setup(.02f, .4f);
	source[3].Setup(.02f, .5f);
	actual[3].setup(.02f, .5f);
	unsigned comparisons = 0, changed = 0, suppressed = 0, upper = 0, lower = 0;
	uint32_t ticks = 9000;
	const auto frame = [&](uint32_t now, unsigned first) {
		shield_pulse_source::ticks = now;
		unsigned frame_changes = 0;
		for (unsigned offset = 0; offset < source.size(); ++offset) {
			const unsigned index = (first + offset) % source.size();
			const float before = actual[index].scale;
			source[index].Update();
			actual[index].update(clock, now);
			require(source[index].Scale() == actual[index].scale && source[index].Valid() == actual[index].valid,
			        "Shield pulse differs from source shared timeout/state");
			if (actual[index].scale != before) {
				++changed;
				++frame_changes;
			} else ++suppressed;
			upper += actual[index].scale == 1.f;
			lower += actual[index].scale == actual[index].minimum;
			++comparisons;
		}
		require(frame_changes <= 1, "Multiple shield pulses consumed one shared timeout");
	};
	frame(ticks, 0);
	require(actual[0].valid == -1 && actual[0].scale == .5f, "Default pulse/first-call delay changed");
	for (unsigned i = 0; i < 3000; ++i) {
		ticks += std::array<uint32_t, 6>{ 0, 1, 24, 25, 26, 500 }[i % 6];
		if (i == 1200) {
			source[1].Setup(.02f, .4f);
			actual[1].setup(.02f, .4f);
		}
		frame(ticks, (i / 150) % 4);
	}
	frame(UINT32_MAX - 40, 0);
	frame(UINT32_MAX - 15, 1);
	const auto before_wrap = actual;
	frame(9, 2);
	frame(1000, 3);
	for (unsigned i = 0; i < actual.size(); ++i)
		require(actual[i].scale == before_wrap[i].scale, "Source signed timeout wrap behavior changed");
	require(changed && suppressed && upper && lower, "Shield pulse missed timing or reversal witnesses");
	std::cout << "Pinned shield pulse: " << comparisons << " shared-clock comparisons, " << changed
	          << " changes, " << suppressed << " suppressed; defaults, resets, bounds and SDL tick wrap passed\n";
}
