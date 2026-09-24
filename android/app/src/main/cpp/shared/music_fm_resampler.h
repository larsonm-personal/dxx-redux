#ifndef DXX_MUSIC_FM_RESAMPLER_H
#define DXX_MUSIC_FM_RESAMPLER_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <vector>

// Streaming counterpart of the fm_quality audition's windowed-sinc converter
// No allocation, trigonometry or future MIDI events are needed while rendering
class music_fm_resampler
{
  public:
	// Pinned ymfmidi: 14318181 Hz master clock / (8 * 36), integer sample_rate()
	static constexpr int native_rate = 49715;
	static constexpr int block_frames = 128;

	void configure(int rate)
	{
		if (rate == rate_) return;
		rate_ = rate;
		const double ratio = double(native_rate) / rate;
		const double radius = 48 * std::max(1.0, ratio);
		const double cutoff = 0.47 * std::min(1.0, 1 / ratio);
		half_ = int(std::ceil(radius)) + 1;
		taps_ = half_ * 2 + 1;
		delay_ = int(std::ceil(radius / ratio));
		coefficients_.resize(size_t(phases + 1) * taps_);
		const double window_scale = bessel_i0(8.6);
		for (int phase = 0; phase <= phases; ++phase) {
			double sum = 0;
			for (int tap = 0; tap < taps_; ++tap) {
				const double x = tap - half_ - double(phase) / phases;
				const double a = 2 * pi * cutoff * x;
				const double window = std::abs(x) < radius
				                          ? bessel_i0(8.6 * std::sqrt(1 - x * x / (radius * radius))) / window_scale
				                          : 0;
				const double value = (std::abs(a) < 1e-12 ? 2 * cutoff : std::sin(a) / (pi * x)) * window;
				coefficients_[size_t(phase) * taps_ + tap] = value;
				sum += value;
			}
			for (int tap = 0; tap < taps_; ++tap) coefficients_[size_t(phase) * taps_ + tap] /= sum;
		}
		size_t capacity = 1;
		while (capacity < size_t(taps_ + std::ceil(ratio) + 4)) capacity *= 2;
		history_.resize(capacity);
		mask_ = capacity - 1;
		source_.resize(size_t(std::ceil(block_frames * ratio) + 1) * 2);
		reset();
	}

	void reset()
	{
		output_frames_ = input_frames_ = 0;
		std::fill(history_.begin(), history_.end(), std::array<float, 2>{});
	}

	int delay_frames() const
	{
		return delay_;
	}

	// generate supplies native-rate stereo floats at unity gain
	template <typename Generator>
	void render(float *out, int frames, Generator generate)
	{
		while (frames > 0) {
			const int count = std::min(frames, int(block_frames));
			const int native_count = int(input_end(output_frames_ + count) - input_frames_);
			if (native_count) generate(source_.data(), native_count);
			int source_index = 0;
			for (int frame = 0; frame < count; ++frame) {
				const int64_t end = input_end(output_frames_ + 1);
				while (input_frames_ < end) {
					history_[size_t(input_frames_++) & mask_] = { source_[source_index], source_[source_index + 1] };
					source_index += 2;
				}
				const int64_t position = (output_frames_ - delay_) * native_rate;
				int64_t center = position / rate_;
				int64_t remainder = position % rate_;
				if (remainder < 0) {
					--center;
					remainder += rate_;
				}
				const int phase = int(remainder * phases / rate_);
				const double fraction = double(remainder * phases % rate_) / rate_;
				const double *a = coefficients_.data() + size_t(phase) * taps_;
				const double *b = a + taps_;
				double left = 0, right = 0;
				for (int tap = 0; tap < taps_; ++tap) {
					const int64_t index = center + tap - half_;
					if (index < 0 || index >= input_frames_) continue;
					const auto &sample = history_[size_t(index) & mask_];
					const double weight = a[tap] + fraction * (b[tap] - a[tap]);
					left += sample[0] * weight;
					right += sample[1] * weight;
				}
				*out++ = float(left);
				*out++ = float(right);
				++output_frames_;
			}
			frames -= count;
		}
	}

  private:
	static constexpr int phases = 1024;
	static constexpr double pi = 3.14159265358979323846;
	int rate_ = 0, half_ = 0, taps_ = 0, delay_ = 0;
	int64_t output_frames_ = 0, input_frames_ = 0;
	size_t mask_ = 0;
	std::vector<double> coefficients_;
	std::vector<std::array<float, 2>> history_;
	std::vector<float> source_;

	int64_t input_end(int64_t frames) const
	{
		return (frames * native_rate + rate_ - 1) / rate_;
	}

	static double bessel_i0(double x)
	{
		double sum = 1, term = 1;
		for (int k = 1; k < 40; ++k) {
			term *= x * x / (4 * k * k);
			sum += term;
			if (term < sum * 1e-16) break;
		}
		return sum;
	}
};

#endif
