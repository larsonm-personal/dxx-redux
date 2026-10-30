#ifndef DXX_XL_AUDIO_COMBAT_H
#define DXX_XL_AUDIO_COMBAT_H
#include "audio_logical.h"
namespace xl_compat
{
struct WeaponFireAudio {
	bool spatial;
	int sound_class, volume;
};
inline WeaponFireAudio weapon_fire_audio(int weapon, bool parent_exists, bool viewer_parent)
{
	const bool spatial = parent_exists && !viewer_parent;
	return { spatial, spatial ? XL_SOUND_GENERIC : viewer_parent ? XL_SOUND_PLAYER
		                                                         : XL_SOUND_LASER,
		     !spatial && weapon == 11 ? 32768 : 65536 };
}
inline int weapon_explosion_sound(int weapon, bool d1)
{
	// Pinned CObject::InitTables splash classification, independent of damage radius
	if (!d1) switch (weapon) {
			case 18:
			case 28:
			case 40:
			case 54:
			case 58:
			case 59:
			case 61:
			case 62: return 57;
		}
	return 11;
}
inline int actor_explosion_sound(bool d1)
{
	return d1 ? 11 : 58;
}
} // namespace xl_compat
#endif
