// Compare production PCM with an explicitly configured real-time reference
#include "music_fluid.h"
#include <array>
#include <cstdio>
#include <cstdlib>
#include <vector>

#define CHECK(x)                                                            \
	do {                                                                    \
		if (!(x)) {                                                         \
			std::fprintf(stderr, "Fluid FAIL line %d: %s\n", __LINE__, #x); \
			std::exit(1);                                                   \
		}                                                                   \
	} while (0)

extern "C" void test_music_fluid_interpolation(const char *path)
{
	// Golden responses computed independently with scipy.signal.sosfreqz
	const double golden[2][3][4] = {
		{ { -5.687751, -0.011135, -3.210000, -2.938751 }, { -5.631162, -0.012480, -2.865379, -2.982504 }, { -5.304031, -0.227987, -2.416718, -3.029879 } },
		{ { -5.687769, -0.011959, -3.216866, -2.965782 }, { -5.631200, -0.013844, -2.871203, -3.021992 }, { -5.304172, -0.231455, -2.424183, -3.051028 } }
	};
	const int rates[] = { 44100, 48000 }, frequencies[] = { 100, 500, 4000, 10000 };
	for (int r = 0; r < 2; ++r) {
		for (int p = 1; p <= 3; ++p) {
			for (int f = 0; f < 4; ++f) {
				std::vector<float> pcm(size_t(rates[r]) * 2, 0);
				for (int n = 0; n < rates[r]; ++n) pcm[size_t(n) * 2] = float(.25 * std::sin(6.283185307179586 * frequencies[f] * n / rates[r]));
				const auto original = pcm;
				music_eq eq;
				eq.configure(0, rates[r]);
				eq.process(pcm.data(), rates[r]);
				CHECK(pcm == original);
				eq.configure(p, rates[r]);
				eq.process(pcm.data(), rates[r]);
				double energy = 0;
				for (int n = rates[r] / 2; n < rates[r]; ++n) {
					CHECK(pcm[size_t(n) * 2 + 1] == 0);
					energy += double(pcm[size_t(n) * 2]) * pcm[size_t(n) * 2];
				}
				const double measured = 10 * std::log10(energy / (rates[r] - rates[r] / 2) / (.25 * .25 / 2));
				CHECK(std::abs(measured - golden[r][p - 1][f]) < .002);
				eq.reset();
				auto chunks = original;
				for (int n = 0; n < rates[r]; n += 137) eq.process(chunks.data() + n * 2, std::min(137, rates[r] - n));
				CHECK(chunks == pcm);
			}
		}
	}
	CHECK(!music_eq::matches_font("different bank", 14));
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
	// Real wet synth reset and seek-style chunk reconstruction retain EQ determinism
	for (int rate : rates) {
		actual.output(rate, -10);
		reference.output(rate, -10);
		actual.eq(2);
		reference.eq(2);
		CHECK(actual.eq_preset() == 2);
		for (int pass = 0; pass < 2; ++pass) {
			actual.reset();
			reference.reset();
			for (auto *s : { actual.get(), reference.get() }) CHECK(fluid_synth_noteon(s, 0, 60, 100) == FLUID_OK);
			std::array<short, 8192> a{}, b{};
			actual.render(a.data(), 4096, 0);
			for (int n = 0; n < 4096; n += 137) reference.render(b.data() + n * 2, std::min(137, 4096 - n), 0);
			CHECK(a == b);
		}
	}
	std::puts("Measured EQ: golden stereo responses, Flat parity, chunk/reset/rate contracts passed");
	std::puts("FluidSynth interpolation: exact fourth-order PCM after load, reset and rate change");
}
