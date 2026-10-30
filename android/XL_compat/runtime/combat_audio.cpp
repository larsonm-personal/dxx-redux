#include "gameplay_audio.h"
#include "../audio_combat.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "weapon.h"
#include "laser.h"
#include "newdemo.h"
#include "d1_in_d2/d1_in_d2.h"
}
extern "C" int xl_gameplay_weapon_fire(const object *weapon, int make_sound)
{
	if (!xl_level_extensions_active()) return 0;
	if (!weapon || !make_sound || weapon->id >= N_weapon_types) return 1;
	const int sound = Weapon_info[weapon->id].flash_sound;
	if (sound < 0) return 1;
	const int parent = weapon->ctype.laser_info.parent_num;
	// Source OBJECT lookup checks the index range, including an empty parent slot
	const bool exists = parent >= 0 && parent <= Highest_object_index;
	const auto cue = xl_compat::weapon_fire_audio(weapon->id, exists, Viewer && parent == Viewer - Objects);
	if (cue.spatial)
		xl_gameplay_sound_position(sound, weapon->segnum, 0, &weapon->pos, 0, cue.volume, 256 * F1_0);
	else {
		if (Newdemo_state == ND_STATE_RECORDING) newdemo_record_sound_3d(sound, 32767, cue.volume);
		xl_gameplay_sound_play(sound, cue.sound_class, cue.volume, 32767, -1);
	}
	return 1;
}
extern "C" int xl_gameplay_weapon_explosion(int object_index, int water)
{
	if (!xl_level_extensions_active()) return 0;
	if (object_index < 0 || object_index > Highest_object_index || Objects[object_index].type != OBJ_WEAPON) return 1;
	const int sound = xl_compat::weapon_explosion_sound(Objects[object_index].id, !water && d1_in_d2_use_d1_gameplay());
	xl_gameplay_sound_object(sound, XL_SOUND_EXPLOSION, object_index, 0, F1_0, 256 * F1_0, -1, -1, 0);
	return 1;
}
extern "C" int xl_gameplay_actor_explosion(int object_index)
{
	if (!xl_level_extensions_active()) return 0;
	xl_gameplay_sound_object(xl_compat::actor_explosion_sound(d1_in_d2_use_d1_gameplay()), XL_SOUND_EXPLOSION, object_index, 0, F1_0, 256 * F1_0, -1, -1, 0);
	return 1;
}
