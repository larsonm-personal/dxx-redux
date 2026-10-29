#ifndef DXX_XL_AUDIO_VOICE_H
#define DXX_XL_AUDIO_VOICE_H
#include "wave.h"
#include <array>
#include <cstddef>
#include <cstdint>
#include <vector>

namespace xl_compat
{
constexpr unsigned audio_voice_limit = 128;
struct AudioSample {
	unsigned rate = 0;
	std::vector<std::int16_t> stereo;
	std::size_t frames() const
	{
		return stereo.size() / 2;
	}
};
AudioSample prepare_audio_sample(const WaveChannels &source, unsigned rate);

struct AudioVoice {
	std::uint64_t serial = 0;
	unsigned sample = 0, position = 0;
	int owner = -1, remaining = 0;
};

// The engine owns the immutable sample bank and excludes concurrent control
// operations with SDL's audio lock. mix() performs no allocation or I/O
class AudioVoices
{
  public:
	std::array<AudioVoice, audio_voice_limit> voices{};
	std::uint64_t next_serial = 1, starts = 0, completions = 0, stops = 0;
	bool paused = false;
	int volume = 128;
	int start(unsigned sample, int repetitions, int owner);
	void stop(int slot);
	void clear();
	void mix(unsigned char *stream, std::size_t frames, const std::vector<AudioSample> &samples);
};
} // namespace xl_compat
#endif
