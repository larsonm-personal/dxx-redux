#include "door_probe.h"
#include "doors.h"
#include "named_audio.h"
#include "preview.h"
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "segment.h"
#include "gameseg.h"
#include "wall.h"
#include "game.h"
#include "state_android_shared.h"
#include "android_meta_actions.h"
#include "android_log.h"
#include "debug_log_categories.h"
}
#include "door_obstruction_control.h"
namespace
{
std::string failure;
std::vector<active_door> saved;
std::vector<wall> saved_walls;
int first = -1;
void require(bool result, const char *message)
{
	if (!result) throw std::runtime_error(message);
}
void verify()
{
	require(xl_level_extensions_active() && first >= 0 && Num_walls == first + 182 && Num_open_doors == 90 && xl_door_state_valid(ActiveDoors, Num_open_doors), "Full Android door state invalid");
	for (int i = 0; i < 90; ++i) {
		const auto &entry = ActiveDoors[i];
		require(entry.n_parts == (i ? 1 : 2) && entry.front_wallnum[0] == first + 2 * i && entry.back_wallnum[0] == first + 2 * i + 1 && entry.time >= 0 && entry.time <= DOOR_WAIT_TIME, "Android door identity or waiting clock changed");
		require(Walls[first + 2 * i].state == WALL_DOOR_WAITING && Walls[first + 2 * i + 1].state == WALL_DOOR_WAITING, "Android door faces did not retain waiting");
	}
	require((Walls[first].flags & WALL_DOOR_AUTO) && !(Walls[first + 1].flags & WALL_DOOR_AUTO) &&
	            ActiveDoors[0].front_wallnum[1] == first + 1 && ActiveDoors[0].back_wallnum[1] == first,
	        "Android linked door lost mixed automatic flags or second-part ownership");
}
void frame(fix elapsed)
{
	const fix before = FrameTime;
	FrameTime = elapsed;
	wall_frame_process();
	FrameTime = before;
}
void disturb()
{
	verify();
	frame(DOOR_WAIT_TIME + 1);
	frame(WallAnims[Walls[first].clip_num].play_time);
	require(Num_open_doors == 0, "Android simultaneous completion skipped an entry");
}
} // namespace
const char *xl_door_probe(const char *operation)
{
	try {
		require(Game_wind && ConsoleObject && !(Game_mode & GM_MULTI), "Door controls require running singleplayer");
		if (!std::strcmp(operation, "obstruction")) {
			xl_door_obstruction_control::run(require);
			debug_log_force(DLOG_GAME, "XL door obstruction passed 48 actor/effect, side and lifecycle cases");
		} else if (!std::strcmp(operation, "prepare")) {
			require(xl_level_extensions_active() && Num_walls >= 184, "Door controls require the authored 91-door fixture");
			require(game_is_time_paused(), "Door capture controls require paused simulation");
			// Paired checkpoint snapshots must also freeze independent audio callbacks
			// The normal running XL frame resumes playback after the overlay closes
			xl_named_audio_resume();
			first = Num_walls - 182;
			saved.clear();
			saved_walls.clear();
			for (int i = Highest_object_index; i >= 0; --i)
				if (Objects[i].type != OBJ_NONE && i != ConsoleObject - Objects) obj_delete(i);
			obj_relink(ConsoleObject - Objects, 0);
			compute_segment_center(&ConsoleObject->pos, &Segments[0]);
			ConsoleObject->size = F1_0 / 16;
			require(Num_open_doors == 0, "Door fixture already has active doors");
			for (int i = 0; i < 91; ++i) {
				const int index = first + 2 * i;
				Walls[index].flags |= WALL_DOOR_AUTO;
				Walls[index + 1].flags |= WALL_DOOR_AUTO;
				if (!i) {
					Walls[index].linked_wall = index + 1;
					Walls[index + 1].linked_wall = index;
					Walls[index + 1].flags &= ~WALL_DOOR_AUTO;
				}
				wall_open_door(&Segments[Walls[index].segnum], Walls[index].sidenum);
			}
			require(Num_open_doors == 90 && Walls[first + 180].state == WALL_DOOR_CLOSED, "Android 90/91 door allocation boundary differs");
			frame(WallAnims[Walls[first].clip_num].play_time);
			verify();
		} else if (!std::strcmp(operation, "verify")) verify();
		else if (!std::strcmp(operation, "disturb")) disturb();
		else if (!std::strcmp(operation, "disk_save")) {
			verify();
			saved.assign(ActiveDoors, ActiveDoors + Num_open_doors);
			saved_walls.assign(Walls, Walls + Num_walls);
			require(state_android_save_to_slot(ANDROID_SAVE_META_SLOT_QUICK, "XL full door state", ANDROID_SAVE_META_KIND_MANUAL) == 1, "Cannot save Android door state");
		} else if (!std::strcmp(operation, "disk_load")) {
			require(saved.size() == 90 && state_android_restore_slot(ANDROID_SAVE_META_SLOT_QUICK) == 1, "Cannot restore Android door state");
			require(Num_open_doors == 90 && Num_walls == static_cast<int>(saved_walls.size()) && !std::memcmp(saved.data(), ActiveDoors, saved.size() * sizeof(active_door)) && !std::memcmp(saved_walls.data(), Walls, saved_walls.size() * sizeof(wall)), "Android disk restore changed door state before simulation");
			verify();
			debug_log_force(DLOG_GAME, "Full Android door table restored with exact identities, timers and wall state before simulation");
			require(android_open_overlay_pause_if_safe(), "Cannot pause restored door state");
		} else if (!std::strcmp(operation, "reuse")) {
			disturb();
			const int index = first + 180;
			wall_open_door(&Segments[Walls[index].segnum], Walls[index].sidenum);
			require(Num_open_doors == 1 && ActiveDoors[0].front_wallnum[0] == index, "Freed Android door capacity was not reused");
		} else if (!std::strcmp(operation, "stock")) {
			require(!xl_level_extensions_active() && !xl_door_frame_process(), "XL door lifecycle leaked into stock");
		} else require(false, "Unknown door probe operation");
		return nullptr;
	} catch (const std::exception &error) {
		failure = error.what();
		return failure.c_str();
	}
}
