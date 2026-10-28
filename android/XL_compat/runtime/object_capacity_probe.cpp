#include "object_capacity_probe.h"
#include "projectile_probe.h"
#include "projectile.h"
#include "preview.h"
#include <algorithm>
#include <cstring>
#include <stdexcept>
#include <string>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "game.h"
#include "ai.h"
#include "digi.h"
#include "sounds.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
#include "android_log.h"
#include "debug_log_categories.h"
}
namespace
{
std::string failure;
object_runtime_state disk_state{};
bool disk_saved = false;
int earned_capacity = 0;
void require(bool result, const char *message)
{
	if (!result) throw std::runtime_error(message);
}
void projectiles(const char *operation)
{
	if (const char *error = xl_projectile_probe(operation)) throw std::runtime_error(error);
}
} // namespace
const char *xl_object_capacity_probe(const char *operation)
{
	try {
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Object pool controls require running singleplayer");
		if (!std::strcmp(operation, "prepare")) {
			earned_capacity = object_capacity();
			require(xl_level_extensions_active() && earned_capacity > DXX_CLASSIC_MAX_OBJECTS && earned_capacity <= DXX_XL_MAX_OBJECTS, "XL object capacity is not active");
			while (num_objects < object_capacity()) {
				const int owner = obj_create(OBJ_LIGHT, 0, ConsoleObject->segnum, &ConsoleObject->pos, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
				require(owner >= 0, "Cannot fill full XL object pool");
			}
			require(obj_allocate() == -1, "Full XL pool allowed excess allocation");
			for (int owner = earned_capacity - 100; owner < earned_capacity; ++owner) obj_delete(owner);
			object_runtime_state sequence{};
			object_get_runtime_state(&sequence);
			sequence.signature_seed = 65535;
			require(object_validate_runtime_state(&sequence), "XL rejected wide lifetime signature seed");
			object_set_runtime_state(&sequence);
			const int audio_owner = obj_create(OBJ_LIGHT, 0, ConsoleObject->segnum, &ConsoleObject->pos, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
			require(audio_owner >= 0 && Objects[audio_owner].signature == 65536, "Wide signature did not advance through allocation");
			digi_link_sound_to_object(SOUND_SHIP_IN_WATER, short(audio_owner), 1, F1_0);
			require(xl_linked_sound_count(audio_owner, SOUND_SHIP_IN_WATER, 0) == 1 && xl_linked_sound_count(audio_owner, SOUND_SHIP_IN_WATER, 1) == 1, "Wide owner lost linked or playing sound");
			digi_sync_sounds();
			require(xl_linked_sound_count(audio_owner, SOUND_SHIP_IN_WATER, 0) == 1, "Sound synchronization truncated wide owner identity");
			obj_delete(audio_owner);
			const int reused = obj_create(OBJ_LIGHT, 0, ConsoleObject->segnum, &ConsoleObject->pos, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
			require(reused == audio_owner && Objects[reused].signature == 65537 && !xl_linked_sound_count(reused, SOUND_SHIP_IN_WATER, 0), "Reused audio owner inherited sound");
			obj_delete(reused);
			projectiles("prepare");
			object_runtime_state earned{};
			object_get_runtime_state(&earned);
			require(object_validate_runtime_state(&earned) && earned.highest_object_index > DXX_CLASSIC_MAX_OBJECTS, "High Android object allocator invalid");
		} else if (!std::strcmp(operation, "verify")) {
			projectiles("verify");
			object_runtime_state current{};
			object_get_runtime_state(&current);
			// Exact allocator identity/order is checked inside checkpoint_cycle before
			// simulation resumes; later frames can legitimately allocate effects
			require(object_capacity() == earned_capacity && object_validate_runtime_state(&current) && current.highest_object_index >= DXX_CLASSIC_MAX_OBJECTS, "High Android allocator is invalid after gameplay resumes");
		} else if (!std::strcmp(operation, "disk_save")) {
			projectiles("verify");
			object_get_runtime_state(&disk_state);
			disk_saved = false;
			require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL high object state", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save high Android object state");
			disk_saved = true;
		} else if (!std::strcmp(operation, "disk_load")) {
			require(disk_saved, "No captured high object disk state");
			require(state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore high Android object state");
			// Observe the production restore before ordinary simulation can move the
			// saved in-flight projectiles into a wall between automation steps
			object_runtime_state restored{};
			object_get_runtime_state(&restored);
			require(object_capacity() == earned_capacity && object_validate_runtime_state(&restored) &&
			            restored.num_objects == disk_state.num_objects && restored.highest_object_index == disk_state.highest_object_index &&
			            restored.signature_seed == disk_state.signature_seed &&
			            std::equal(restored.free_obj_list, restored.free_obj_list + object_capacity(), disk_state.free_obj_list),
			        "High Android disk allocator changed before simulation");
			projectiles("verify");
			debug_log_force(DLOG_GAME, "Object capacity production disk restore preserved allocator and earned projectiles before simulation");
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored high object state");
		} else if (!std::strcmp(operation, "stock")) {
			require(object_capacity() == DXX_CLASSIC_MAX_OBJECTS, "XL object capacity leaked into Redux");
			for (int owner = object_capacity(); owner < MAX_OBJECTS; ++owner) require(Objects[owner].type == OBJ_NONE, "XL inactive objects survived stock transition");
			projectiles("stock");
		} else if (!std::strcmp(operation, "disturb") || !std::strcmp(operation, "reuse")) projectiles(operation);
		else require(false, "Unknown object pool probe operation");
		return nullptr;
	} catch (const std::exception &error) {
		failure = error.what();
		return failure.c_str();
	}
}
