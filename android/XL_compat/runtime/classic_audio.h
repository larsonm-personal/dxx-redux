#ifndef DXX_XL_CLASSIC_AUDIO_H
#define DXX_XL_CLASSIC_AUDIO_H
#include "../wave.h"
#include <string>
#include <vector>

namespace xl_compat
{
struct ClassicAudioSample {
	int sample;
	std::string name, path, source_hash;
	WaveChannels pcm;
};
struct ClassicAudioResources {
	std::string identity;
	std::vector<ClassicAudioSample> samples;
};
ClassicAudioResources prepare_classic_audio(const char *level, int ordinal, int bank_rate);
int classic_audio_output_rate();
} // namespace xl_compat
#endif
