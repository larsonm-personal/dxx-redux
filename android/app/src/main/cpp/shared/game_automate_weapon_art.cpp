#if defined(INTROSPECT_ON) && defined(ANDROID)

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>
#include <nlohmann/json.hpp>

extern "C" {
#include "game_automate_weapon_art.h"
#include "3d.h"
#include "android_save_meta.h"
#include "android_rewind.h"
#include "collide.h"
#include "endlevel.h"
#include "fuelcen.h"
#include "game.h"
#include "gameseg.h"
#include "laser.h"
#include "object.h"
#include "ogl_init.h"
#include "palette.h"
#include "piggy.h"
#include "player.h"
#include "rle.h"
#include "screens.h"
#include "state.h"
#include "weapon.h"
#ifdef DXX_BUILD_DESCENT_II
#include "d1_in_d2/d1_in_d2.h"
#endif
}

namespace
{
using json = nlohmann::ordered_json;
using bytes = std::vector<unsigned char>;

void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}

void write_file(const std::string &name, const void *data, size_t size)
{
	PHYSFS_file *file = PHYSFS_openWrite(("weapon-art/" + name).c_str());
	require(file != nullptr, "cannot open weapon art output");
	const bool written = PHYSFS_writeBytes(file, data, size) == static_cast<PHYSFS_sint64>(size);
	const bool closed = PHYSFS_close(file) != 0;
	require(written && closed, "cannot write weapon art output");
}

json vector(const vms_vector &v)
{
	return { static_cast<int64_t>(v.x), static_cast<int64_t>(v.y), static_cast<int64_t>(v.z) };
}

json capture(const char *name, const std::vector<int> &slots)
{
	json shots = json::array();
	for (int slot : slots) {
		const object &shot = Objects[slot];
		require(shot.type == OBJ_WEAPON && shot.id == 20, "live Spreadfire projectile lost original identity 20");
		shots.push_back({ { "id", static_cast<int>(shot.id) }, { "size", static_cast<int64_t>(shot.size) }, { "damage", static_cast<int64_t>(shot.shields) }, { "life", static_cast<int64_t>(shot.lifeleft) }, { "creation_age", GameTime64 - shot.ctype.laser_info.creation_time }, { "position", vector(shot.pos) }, { "velocity", vector(shot.mtype.phys_info.velocity) } });
	}
	const bitmap_index bitmap = Weapon_info[20].bitmap;
	require(Weapon_info[20].render_type == WEAPON_RENDER_BLOB, "Spreadfire is not a blob");
	PIGGY_PAGE_IN(bitmap);
	grs_bitmap *source = &GameBitmaps[bitmap.index];
	require(!std::strcmp(piggy_game_bitmap_name(source), "sprdblob"), "Spreadfire does not resolve to sprdblob");
	grs_bitmap *decoded = (source->bm_flags & BM_FLAG_RLE) ? rle_expand_texture(source) : source;
	bytes indexed;
	for (int y = 0; y < decoded->bm_h; ++y)
		indexed.insert(indexed.end(), decoded->bm_data + y * decoded->bm_rowsize,
		               decoded->bm_data + y * decoded->bm_rowsize + decoded->bm_w);
	const std::string stem = name;
	write_file(stem + ".indexed", indexed.data(), indexed.size());
	write_file(stem + ".palette", gr_palette, 768);

	// Use the device's real renderer and framebuffer, without level occlusion
	// Fresh test preferences select the same non-MSAA surface in both engines
	require(ogl_msaa_samples == 0, "weapon art comparison requires non-MSAA rendering");
	grs_canvas *saved_canvas = grd_curcanv;
	gr_set_current_canvas(nullptr);
	g3_start_frame();
	glClearColor(0, 0, 0, 1);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	vms_vector eye = { 0, 0, -8 * F1_0 };
	g3_set_view_matrix(&eye, &vmd_identity_matrix, F1_0);
	for (int slot : slots) Laser_render(&Objects[slot]);
	bytes rgba(static_cast<size_t>(SWIDTH) * SHEIGHT * 4);
	glPixelStorei(GL_PACK_ALIGNMENT, 1);
	glReadPixels(0, 0, SWIDTH, SHEIGHT, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
	const unsigned error = glGetError();
	g3_end_frame();
	gr_set_current_canvas(saved_canvas);
	require(error == GL_NO_ERROR, "projectile GPU readback failed");
	bytes rgb;
	rgb.reserve(static_cast<size_t>(SWIDTH) * SHEIGHT * 3);
	for (int y = SHEIGHT - 1; y >= 0; --y)
		for (int x = 0; x < SWIDTH; ++x) {
			const size_t offset = (static_cast<size_t>(y) * SWIDTH + x) * 4;
			rgb.insert(rgb.end(), rgba.begin() + offset, rgba.begin() + offset + 3);
		}
	require(std::any_of(rgb.begin(), rgb.end(), [](unsigned char v) { return v != 0; }), "projectiles rendered no visible pixels");
	std::string header = "P6\n" + std::to_string(SWIDTH) + " " + std::to_string(SHEIGHT) + "\n255\n";
	bytes ppm(header.begin(), header.end());
	ppm.insert(ppm.end(), rgb.begin(), rgb.end());
	write_file(stem + ".ppm", ppm.data(), ppm.size());
	laser_runtime_state runtime = {};
	laser_get_runtime_state(&runtime);
	return { { "frame", name }, { "shots", shots }, { "energy", static_cast<int64_t>(Players[Player_num].energy) }, { "spreadfire_toggle", runtime.spreadfire_toggle }, { "bitmap", bitmap.index }, { "name", piggy_game_bitmap_name(source) }, { "width", source->bm_w }, { "height", source->bm_h }, { "transparent", !!(source->bm_flags & BM_FLAG_TRANSPARENT) }, { "render_width", SWIDTH }, { "render_height", SHEIGHT }, { "cadence_clock_deltas", { game_get_fusion_next_sound_time() - GameTime64, fuelcen_get_last_sound_time() - GameTime64, collide_get_collision_delay_last_play_time() - GameTime64 } } };
}

std::vector<int> fire()
{
	std::array<int, MAX_OBJECTS> signatures;
	for (int i = 0; i < MAX_OBJECTS; ++i) signatures[i] = Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature;
	Next_laser_fire_time = GameTime64;
	const fix energy = Players[Player_num].energy;
	do_laser_firing_player();
	std::vector<int> slots;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_WEAPON && Objects[i].signature != signatures[i]) slots.push_back(i);
	require(slots.size() == 3, "Spreadfire did not emit exactly three live pellets");
	require(energy - Players[Player_num].energy == F1_0 / 2, "Spreadfire did not use original accounting energy");
	// Advance the real projectile physics into the authored corridor before saving
	FrameTime = F1_0 / 8;
	GameTime64 += FrameTime;
	for (int slot : slots) object_move_one(&Objects[slot]);
	return slots;
}
} // namespace

int game_automate_weapon_art(char *reason, size_t reason_size)
{
	rewind_memory_buffer checkpoint = {};
	const fix saved_frame_time = FrameTime;
	try {
		require(!(Game_mode & GM_MULTI) && Screen_mode == SCREEN_GAME && ConsoleObject && !Player_is_dead,
		        "weapon art probe requires a live single-player mine");
#ifdef DXX_BUILD_DESCENT_II
		require(d1_in_d2_use_d1_gameplay(), "weapon art probe requires D1 gameplay");
#endif
		require(PHYSFS_mkdir("weapon-art") != 0, "cannot create weapon art output directory");
		require(Primary_weapon_to_weapon_info[SPREADFIRE_INDEX] == 12, "Spreadfire accounting identity changed");
		ConsoleObject->pos = ConsoleObject->last_pos = vmd_zero_vector;
		const int segment = find_point_seg(&ConsoleObject->pos, 0);
		require(segment >= 0, "weapon art fixture origin is outside the loaded mine");
		obj_relink(ConsoleObject - Objects, segment);
		ConsoleObject->orient = vmd_identity_matrix;
		ConsoleObject->mtype.phys_info.velocity = vmd_zero_vector;
		Players[Player_num].primary_weapon = SPREADFIRE_INDEX;
		Players[Player_num].primary_weapon_flags |= HAS_SPREADFIRE_FLAG;
		Players[Player_num].energy = 100 * F1_0;
		laser_runtime_state runtime = {};
		laser_set_runtime_state(&runtime);
		FrameTime = F1_0 / 64;
		const auto first = fire();
		game_set_fusion_next_sound_time(GameTime64 + F1_0 / 8);
		fuelcen_set_last_sound_time(GameTime64 - (1LL << 40));
		collide_set_collision_delay_last_play_time(GameTime64 - F1_0 / 4);
		json records = json::array();
		records.push_back(capture("first", first));
		const fix64 checkpoint_time = GameTime64;
		require(state_save_to_memory(&checkpoint, "Spreadfire in flight", ANDROID_SAVE_META_KIND_MANUAL, 1), "cannot save live Spreadfire checkpoint");
		// Exercise the production memory adapter and history admission for each
		// flyout phase; actual phase motion is covered by the host flyout fixture
		android_rewind_reset_level();
		android_rewind_maybe_capture_frame();
		int history_count = 0;
		android_rewind_get_history(&history_count, nullptr, nullptr);
		require(history_count == 1, "playable mine did not seed rewind history");
		for (int phase = 1; phase <= 4; ++phase) {
			rewind_memory_buffer active = {};
			Endlevel_sequence = phase;
			GameTime64 += 5 * F1_0;
			const int accepted = state_save_to_memory(&active, "Active flyout", ANDROID_SAVE_META_KIND_MANUAL, 1);
			android_rewind_maybe_capture_frame();
			int count = 0;
			android_rewind_get_history(&count, nullptr, nullptr);
			Endlevel_sequence = 0;
			const bool untouched = active.size == 0;
			rewind_memory_buffer_discard(&active);
			require(!accepted && untouched && count == history_count, "flyout captured incomplete state or replaced playable rewind history");
		}
		GameTime64 = checkpoint_time;
		for (const int exploded : { 0, 1 }) {
			rewind_memory_buffer active = {};
			Player_is_dead = 1;
			Player_exploded = exploded;
			GameTime64 += 5 * F1_0;
			const int accepted = state_save_to_memory(&active, "Active death", ANDROID_SAVE_META_KIND_MANUAL, 1);
			android_rewind_maybe_capture_frame();
			int count = 0;
			android_rewind_get_history(&count, nullptr, nullptr);
			Player_is_dead = Player_exploded = 0;
			const bool untouched = active.size == 0;
			rewind_memory_buffer_discard(&active);
			require(!accepted && untouched && count == history_count, "death captured incomplete state or replaced playable rewind history");
		}
		GameTime64 = checkpoint_time;
		write_file("checkpoint.dsg", checkpoint.data, checkpoint.size);
		for (int slot : first) obj_delete(slot);
		game_set_fusion_next_sound_time(123);
		fuelcen_set_last_sound_time(456);
		collide_set_collision_delay_last_play_time(789);
		require(state_restore_from_memory(&checkpoint), "cannot restore live Spreadfire checkpoint");
		json restored = capture("restored", first);
		json expected = records[0];
		expected["frame"] = "restored";
		require(restored == expected, "restored Spreadfire state or resource identity changed");
		records.push_back(std::move(restored));
		// Exercise the actual rewind entry with a nonzero epoch, taking all
		// cadence clocks from the save rather than collision-only metadata
		android_rewind_authoritative_restore rewind = {};
		rewind.buffer = checkpoint;
		rewind.snapshot_index = -1;
		rewind.game_time64 = checkpoint_time;
		game_set_fusion_next_sound_time(321);
		fuelcen_set_last_sound_time(654);
		collide_set_collision_delay_last_play_time(987);
		require(android_rewind_restore_authoritative(&rewind) == ANDROID_REWIND_STATUS_RESTORED && GameTime64 == checkpoint_time,
		        "cannot rewind live Spreadfire checkpoint to its original epoch");
		json rewound = capture("rewound", first);
		expected["frame"] = "rewound";
		require(rewound == expected, "rewind changed relative cadence clocks, Spreadfire state or resource identity");
		records.push_back(std::move(rewound));
		records.push_back(capture("second", fire()));
		json report = { { "engine",
#ifdef DXX_BUILD_DESCENT_II
			              "d2"
#else
			              "d1"
#endif
			            },
			            { "frames", records } };
		const std::string result = report.dump(2) + "\n";
		write_file("weapon-art.json", result.data(), result.size());
		rewind_memory_buffer_discard(&checkpoint);
		FrameTime = saved_frame_time;
		return 1;
	} catch (const std::exception &error) {
		rewind_memory_buffer_discard(&checkpoint);
		FrameTime = saved_frame_time;
		std::snprintf(reason, reason_size, "%s", error.what());
		return 0;
	}
}
#endif
