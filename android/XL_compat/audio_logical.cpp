#include "audio_logical.h"
#include <algorithm>
namespace xl_compat
{
int audio_translate_sound(int sound, const std::uint8_t (&sounds)[audio_logical_sound_count], const std::uint8_t (&alternate)[audio_logical_sound_count], bool low_memory)
{
	if (sound < 0 || unsigned(sound) >= audio_logical_sound_count) return -1;
	if (low_memory) sound = alternate[sound];
	// The source reserves 255; reject 254 too rather than indexing past its table
	if (unsigned(sound) >= audio_logical_sound_count) return -1;
	const int sample = sounds[sound];
	return unsigned(sample) < audio_logical_sound_count ? sample : -1;
}
int audio_forcefield_volume(int requested, int custom_volume)
{
	if (requested < 0 || custom_volume < 0 || custom_volume > 65536) return -1;
	return (std::max) (4096, int(std::int64_t(requested) * custom_volume / 65536));
}
} // namespace xl_compat
