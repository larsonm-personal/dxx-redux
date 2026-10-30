#ifndef DXX_XL_AUDIO_LOGICAL_H
#define DXX_XL_AUDIO_LOGICAL_H
// Pinned XL SOUNDCLASS values; checked against the actual source constants
enum xl_audio_class {
	XL_SOUND_GENERIC = 0,
	XL_SOUND_AMBIENT = 1,
	XL_SOUND_PLAYER = 2,
	XL_SOUND_ROBOT = 3,
	XL_SOUND_LASER = 4,
	XL_SOUND_MISSILE = 5,
	XL_SOUND_EXPLOSION = 6
};
#ifdef __cplusplus
#include <cstdint>
namespace xl_compat
{
constexpr unsigned audio_logical_sound_count = 254;
int audio_translate_sound(int sound, const std::uint8_t (&sounds)[audio_logical_sound_count], const std::uint8_t (&alternate)[audio_logical_sound_count], bool low_memory);
int audio_forcefield_volume(int requested, int custom_volume);
} // namespace xl_compat
#endif
#endif
