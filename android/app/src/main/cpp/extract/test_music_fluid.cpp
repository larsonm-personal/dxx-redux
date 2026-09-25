// Compare production PCM with an explicitly configured real-time reference
#include "music_fluid.h"
#include <array>
#include <cstdio>
#include <cstdlib>

#define CHECK(x)                                                            \
	do {                                                                    \
		if (!(x)) {                                                         \
			std::fprintf(stderr, "Fluid FAIL line %d: %s\n", __LINE__, #x); \
			std::exit(1);                                                   \
		}                                                                   \
	} while (0)

extern "C" void test_music_fluid_interpolation(const char *path)
{
	music_fluid actual, reference;
	CHECK(actual.load(nullptr, path));
	CHECK(reference.load(nullptr, path));
	for (int phase = 0; phase < 3; ++phase) {
		if (phase == 1) {
			actual.reset();
			reference.reset();
		} else if (phase == 2) {
			actual.output(44100, -10);
			reference.output(44100, -10);
		}
		CHECK(fluid_synth_set_interp_method(reference.get(), -1, FLUID_INTERP_4THORDER) == FLUID_OK);
		for (auto *s : { actual.get(), reference.get() }) {
			// Transpose a layered patch so interpolation affects the rendered PCM
			CHECK(fluid_synth_program_change(s, 0, 51) == FLUID_OK);
			CHECK(fluid_synth_pitch_bend(s, 0, 9000) == FLUID_OK);
			CHECK(fluid_synth_noteon(s, 0, 73, 100) == FLUID_OK);
		}
		int peak = 0;
		std::array<short, 512> a{}, b{};
		for (int block = 0; block < 100; ++block) {
			actual.render(a.data(), 256, 0);
			reference.render(b.data(), 256, 0);
			CHECK(a == b);
			for (short sample : a)
				if (std::abs(int(sample)) > peak) peak = std::abs(int(sample));
		}
		CHECK(peak > 100);
	}
	std::puts("FluidSynth interpolation: exact fourth-order PCM after load, reset and rate change");
}
