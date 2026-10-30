#ifndef DXX_XL_AUDIO_ROUTES_TEST_H
#define DXX_XL_AUDIO_ROUTES_TEST_H
#include "surface_audio_test.h"
#include "doors.h"
#include "cloak.h"
#include "../trigger_flags.h"
extern "C" {
#include "game.h"
#include "piggy.h"
#include "fireball.h"
#include "switch.h"
void do_exploding_wall_frame(void);
}
namespace xl_audio_route_control
{
using xl_surface_audio_control::check;
struct Guard {
	xl_surface_audio_control::Guard surface;
	fix frame = FrameTime;
	int doors = Num_open_doors, cloaks = Num_cloaking_walls;
	std::vector<active_door> active{ ActiveDoors, ActiveDoors + Num_open_doors };
	std::vector<cloaking_wall> cloaking{ CloakingWalls, CloakingWalls + Num_cloaking_walls };
	std::vector<wclip> animations{ WallAnims, WallAnims + Num_wall_anims };
	std::array<expl_wall, MAX_EXPLODING_WALLS> exploding{};
	Guard()
	{
		std::copy_n(expl_wall_list, MAX_EXPLODING_WALLS, exploding.begin());
	}
	~Guard()
	{
		FrameTime = frame;
		Num_open_doors = doors;
		Num_cloaking_walls = cloaks;
		std::copy(active.begin(), active.end(), ActiveDoors);
		std::copy(cloaking.begin(), cloaking.end(), CloakingWalls);
		std::copy(animations.begin(), animations.end(), WallAnims);
		std::copy(exploding.begin(), exploding.end(), expl_wall_list);
	}
};
template <class Output>
void run(Output output)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	check(xl_level_extensions_active() && Num_walls >= 2 && Num_wall_anims > 0, "Audio route fixture requires loaded XL walls");
	SDL_PauseAudio(1);
	const auto initial = named_audio_snapshot();
	Guard guard;
	object listener = *ConsoleObject;
	const int first = listener.segnum;
	int middle = -1, last = -1, outward = -1, back = -1, onward = -1, inward = -1;
	for (int side = 0; side < 6 && last < 0; ++side) {
		const int child = Segments[first].children[side];
		if (child < 0) continue;
		for (int next = 0; next < 6 && last < 0; ++next) {
			const int destination = Segments[child].children[next];
			if (destination < 0 || destination == first) continue;
			middle = child;
			last = destination;
			outward = side;
			back = find_connect_side(&Segments[first], &Segments[middle]);
			onward = next;
			inward = find_connect_side(&Segments[middle], &Segments[last]);
		}
	}
	check(last >= 0 && back >= 0 && inward >= 0, "Audio routes require three real connected segments");
	int opaque = -1, transparent = -1;
	for (int i = 0; i < NumTextures; ++i) {
		const auto flags = piggy_bitmap_get_flags(&GameBitmaps[Textures[i].index]);
		if (!(flags & (BM_FLAG_TRANSPARENT | BM_FLAG_SUPER_TRANSPARENT)) && opaque < 0) opaque = i;
		if ((flags & BM_FLAG_TRANSPARENT) && transparent < 0) transparent = i;
	}
	check(opaque >= 0 && transparent >= 0, "Audio routes require authored opaque and transparent textures");
	for (int i = 0; i < Num_segments; ++i) {
		std::fill_n(Segments[i].children, 6, short(-1));
		Segments[i].objects = -1;
	}
	Segments[first].children[outward] = short(middle);
	Segments[middle].children[back] = short(first);
	Segments[middle].children[onward] = short(last);
	Segments[last].children[inward] = short(middle);
	Segments[first].sides[outward].wall_num = 0;
	Segments[middle].sides[back].wall_num = 1;
	Segments[middle].sides[onward].wall_num = Segments[last].sides[inward].wall_num = -1;
	for (int i = 0; i < 2; ++i) {
		auto &wall = Walls[i];
		wall = {};
		wall.segnum = i ? middle : first;
		wall.sidenum = i ? back : outward;
		wall.type = WALL_CLOSED;
		wall.state = WALL_DOOR_CLOSED;
		wall.cloak_value = 31;
		wall.linked_wall = -1;
		wall.clip_num = 0;
		auto &face = Segments[wall.segnum].sides[wall.sidenum];
		face.tmap_num = short(opaque);
		face.tmap_num2 = 0;
	}
	Num_open_doors = Num_cloaking_walls = 0;
	for (auto &entry : expl_wall_list) entry.segnum = -1;
	std::fill_n(Sounds, MAX_SOUNDS, 255);
	GameArg.SysLowMem = 0;
	GameCfg.DigiVolume = 8;
	digi_set_digi_volume(32768);
	Viewer = &listener;
	compute_segment_center(&listener.pos, &Segments[first]);
	vms_vector position;
	compute_segment_center(&position, &Segments[last]);
	xl_named_audio_stop();
	clear_linked_scene();
	const auto token = xl_audio_link_position(0, last, 0, &position, 1, F1_0, 256 * F1_0);
	check(token > 0 && owners.count == 1 && owners.entries[0].channel < 0, "Closed route started an audible owner");
	const auto sync = [&](bool audible) {
		xl_audio_sync_objects();
		check(Viewer->segnum == first && owners.count == 1 && owners.entries[0].serial == std::uint64_t(token) && owners.valid(mixer) && !Mix_Playing(-1), "Route update changed listener, owner identity or Redux playback");
		check((owners.entries[0].channel >= 0) == audible, audible ? "Opened route retained a stale inaudible owner" : "Closed route retained a stale audible owner");
		if (audible) output();
	};
	trigger action{};
	action.type = 19;
	action.flags = TF_NO_MESSAGE;
	action.num_links = 2;
	action.seg[0] = short(first);
	action.side[0] = short(outward);
	action.seg[1] = short(middle);
	action.side[1] = short(back);
	const auto texture = [&](int value) {
		action.value = value;
		check(xl_trigger_texture_action(&action) == 0, "Actual texture action failed in audio route control");
	};
	texture(transparent);
	sync(true);
	texture(opaque);
	sync(false);
	texture(transparent);
	sync(true);
	const auto earned = named_audio_snapshot();
	action.num_links = 0;
	xl_audio_defer_objects(1);
	const auto pending = named_audio_snapshot();
	check(xl_trigger_texture_action(&action) == 0 && named_audio_snapshot() == pending, "Empty route invalidation changed earned playback, deferred state or ownership");
	xl_audio_defer_objects(0);
	sync(true);
	action.num_links = 2;
	texture(opaque);
	sync(false);
	xl_named_audio_stop();
	check(restore_named_audio(earned, true) && named_audio_snapshot() == earned, "Cannot restore earned route playback");
	sync(false);
	texture(transparent);
	sync(true);
	texture(opaque);
	sync(false);
	check(xl_start_wall_cloak(first, outward, 1) == 1, "Cannot start actual audio-route cloak");
	sync(false);
	FrameTime = F1_0 + 1;
	xl_cloak_frame_process();
	check(!Num_cloaking_walls && Walls[0].type == WALL_OPEN, "Actual cloak failed to complete");
	sync(true);
	check(xl_start_wall_cloak(first, outward, 0) == 1, "Cannot start actual audio-route decloak");
	sync(true);
	xl_cloak_frame_process();
	sync(false);
	for (int i = 0; i < 2; ++i) {
		Walls[i].type = WALL_DOOR;
		Walls[i].flags = 0;
		Walls[i].state = WALL_DOOR_CLOSED;
	}
	WallAnims[0].num_frames = 4;
	WallAnims[0].play_time = F1_0;
	WallAnims[0].open_sound = WallAnims[0].close_sound = -1;
	for (int i = 0; i < 4; ++i) WallAnims[0].frames[i] = short(opaque);
	check(xl_door_open(first, outward) == 1, "Cannot start actual audio-route door");
	sync(true);
	xl_door_frame_process();
	xl_door_frame_process();
	sync(true);
	check(xl_door_close(first, outward, 1) == 1, "Cannot close actual audio-route door");
	sync(true);
	xl_door_frame_process();
	sync(false);
	for (int i = 0; i < 2; ++i) {
		Walls[i].type = WALL_BLASTABLE;
		Walls[i].flags = 0;
		Walls[i].state = WALL_DOOR_CLOSED;
	}
	explode_wall(first, outward);
	check(expl_wall_list[0].segnum == first, "Actual exploding wall was not registered");
	sync(false);
	// Exercise retirement without creating unrelated damaging fireballs
	expl_wall_list[0].time = F1_0;
	do_exploding_wall_frame();
	check(expl_wall_list[0].segnum == -1 && (Walls[0].flags & WALL_BLASTED), "Actual exploding wall failed to retire");
	sync(true);
	xl_named_audio_stop();
	guard.surface.restore();
	check(restore_named_audio(initial, true), "Cannot restore incoming audio after route controls");
}
} // namespace xl_audio_route_control
#endif
