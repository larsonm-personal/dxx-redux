#ifndef DXX_MUSIC_EQ_H
#define DXX_MUSIC_EQ_H
#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include "music_eq_presets.h"

// Stereo biquads after synth effects, before mixing or PCM16 conversion
// Configure only while stopped; the existing preview replacement path enforces this
class music_eq
{

	struct section {
		double b0 = 1, b1 = 0, b2 = 0, a1 = 0, a2 = 0;
		std::array<double, 2> z1{}, z2{};
	};
	std::array<section, 5> filters{};
	int selected = 0;
	double preamp = 1;

  public:
	static bool matches_font(const void *data, size_t size)
	{
		if (size != music_eq_font_bytes) return false;
		uint64_t hash = UINT64_C(14695981039346656037);
		const auto *bytes = static_cast<const unsigned char *>(data);
		for (size_t i = 0; i < size; ++i) hash = (hash ^ bytes[i]) * UINT64_C(1099511628211);
		return hash == music_eq_font_fnv;
	}
	int preset() const
	{
		return selected;
	}
	void reset()
	{
		for (auto &f : filters) {
			f.z1 = {};
			f.z2 = {};
		}
	}
	void configure(int preset, int rate)
	{
		selected = preset >= 1 && preset <= 3 && rate >= 8000 && rate <= 96000 ? preset : 0;
		reset();
		if (!selected) return;
		const auto &profile = music_eq_profiles[selected - 1];
		preamp = std::pow(10., profile.preamp / 20.);
		for (size_t i = 0; i < filters.size(); ++i) {
			const auto &band = profile.bands[i];
			const double a = std::pow(10., band.gain / 40.);
			const double w = 6.283185307179586 * std::min(band.frequency, rate * .45) / rate;
			const double c = std::cos(w), s = std::sin(w);
			double b0, b1, b2, a0, a1, a2;
			if (band.type == 1) {
				const double alpha = s / (2 * band.width);
				b0 = 1 + alpha * a;
				b1 = -2 * c;
				b2 = 1 - alpha * a;
				a0 = 1 + alpha / a;
				a1 = -2 * c;
				a2 = 1 - alpha / a;
			} else {
				const double alpha = s / 2 * std::sqrt((a + 1 / a) * (1 / band.width - 1) + 2);
				const double t = 2 * std::sqrt(a) * alpha;
				if (band.type == 0) {
					b0 = a * ((a + 1) - (a - 1) * c + t);
					b1 = 2 * a * ((a - 1) - (a + 1) * c);
					b2 = a * ((a + 1) - (a - 1) * c - t);
					a0 = (a + 1) + (a - 1) * c + t;
					a1 = -2 * ((a - 1) + (a + 1) * c);
					a2 = (a + 1) + (a - 1) * c - t;
				} else {
					b0 = a * ((a + 1) + (a - 1) * c + t);
					b1 = -2 * a * ((a - 1) + (a + 1) * c);
					b2 = a * ((a + 1) + (a - 1) * c - t);
					a0 = (a + 1) - (a - 1) * c + t;
					a1 = 2 * ((a - 1) - (a + 1) * c);
					a2 = (a + 1) - (a - 1) * c - t;
				}
			}
			filters[i] = { b0 / a0, b1 / a0, b2 / a0, a1 / a0, a2 / a0, {}, {} };
		}
	}
	void process(float *pcm, int frames)
	{
		if (!selected) return;
		for (int i = 0; i < frames; ++i) {
			for (int channel = 0; channel < 2; ++channel) {
				double value = std::isfinite(pcm[i * 2 + channel]) ? pcm[i * 2 + channel] * preamp : 0;
				for (auto &f : filters) {
					const double result = f.b0 * value + f.z1[channel];
					f.z1[channel] = f.b1 * value - f.a1 * result + f.z2[channel];
					f.z2[channel] = f.b2 * value - f.a2 * result;
					value = result;
				}
				pcm[i * 2 + channel] = float(value);
			}
		}
	}
};
#endif
