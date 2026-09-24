// Streaming audio invariants, independent of any game instrument bank
#include "music_fm_resampler.h"
#include <cstdio>
#include <cstdlib>
#include <chrono>

#define CHECK(x)                                                      \
	do {                                                              \
		if (!(x)) {                                                   \
			std::fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #x); \
			std::exit(1);                                             \
		}                                                             \
	} while (0)

static std::vector<float> tone(music_fm_resampler &filter, int rate, double frequency, int chunk, bool reaffirm_rate = false)
{
	std::vector<float> output(size_t(rate) * 2);
	int64_t cursor = 0;
	auto source = [&](float *out, int frames) {
		for (int i = 0; i < frames; ++i, ++cursor) {
			out[i * 2] = float(0.5 * std::sin(6.2831853071795864769 * frequency * double(cursor) / filter.native_rate));
			out[i * 2 + 1] = -out[i * 2];
		}
	};
	for (int i = 0; i < rate; i += chunk) {
		if (reaffirm_rate) filter.configure(rate);
		filter.render(output.data() + i * 2, std::min(chunk, rate - i), source);
	}
	CHECK(cursor == filter.native_rate);
	return output;
}

static double tone_gain(const std::vector<float> &data, int rate)
{
	double energy = 0;
	for (int i = rate / 10; i < rate * 9 / 10; ++i) {
		CHECK(std::isfinite(data[i * 2]));
		CHECK(data[i * 2] == -data[i * 2 + 1]);
		energy += double(data[i * 2]) * data[i * 2];
	}
	return 10 * std::log10(energy / (rate * 8 / 10) / 0.125);
}

int main()
{
	const auto start = std::chrono::steady_clock::now();
	music_fm_resampler filter;
	for (int rate : { 8000, 22050, 44100, 48000, 96000, 192000 }) {
		filter.configure(rate);
		const auto original = tone(filter, rate, 1000, 2048);
		CHECK(std::abs(tone_gain(original, rate)) < 0.01);
		filter.reset();
		CHECK(original == tone(filter, rate, 1000, 1));
		filter.reset();
		CHECK(original == tone(filter, rate, 1000, 317));
		// Updating gain or reasserting the output spec must not reset filter history
		filter.reset();
		CHECK(original == tone(filter, rate, 1000, 97, true));
		std::printf("rate=%d chunk/reset exact, delay=%d frames\n", rate, filter.delay_frames());
	}
	filter.configure(48000);
	for (int frequency : { 20000, 24600 }) {
		filter.reset();
		const double gain = tone_gain(tone(filter, 48000, frequency, 2048), 48000);
		std::printf("tone=%d Hz gain=%.3f dB\n", frequency, gain);
		CHECK(frequency == 20000 ? std::abs(gain) < 0.1 : gain < -65);
	}
	filter.reset();
	float silence[256];
	filter.render(silence, 128, [](float *out, int n) { std::fill(out, out + n * 2, 0.0f); });
	for (float v : silence) CHECK(v == 0);
	const double seconds = std::chrono::duration<double>(std::chrono::steady_clock::now() - start).count();
	std::printf("FM resampler passed: 26 seconds audio in %.3f seconds (including setup and tone generation)\n", seconds);
}
