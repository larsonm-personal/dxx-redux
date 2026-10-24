// Reproducible developer preview startup; no gameplay assists
#include "preview.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>
extern "C" {
#include "args.h"
#include "game.h"
#include "gameseq.h"
#include "gameseg.h"
#include "wall.h"
#include "inferno.h"
#include "input_demo_start.h"
#include "mission.h"
#include "player.h"
#include <physfs.h>
int input_demo_maybe_start_replay_from_cmdline(void);
}

namespace
{
bool running = false;
unsigned rendered_ticks = 0;
bool has_option(const char *name)
{
	for (int i = 1; i < Num_args; ++i)
		if (!std::strcmp(Args[i], name)) return true;
	return false;
}
} // namespace

extern "C" int xl_preview_start(void)
{
	const bool option = has_option("-xl-preview-start");
	if (!option) return input_demo_maybe_start_replay_from_cmdline();
	if (!has_option("-xl-static-light-preview") && !has_option("-xl-pilot")) {
		std::fprintf(stderr, "XL preview startup requires -xl-static-light-preview\n");
		return 1;
	}
	char mission[16];
	std::strcpy(mission, has_option("-xl-pilot") ? "d2-xl" : "xlprobe");
	if (!load_mission_by_name(mission)) {
		std::fprintf(stderr, "XL preview cannot load mission %s\n", mission);
		return 1;
	}
	std::strcpy(Players[Player_num].callsign, "XLTest");
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	for (int i = 1; i < Num_args; ++i) {
		if (std::strcmp(Args[i], "-xl-preview-wall")) continue;
		char *end = nullptr;
		const long index = i + 1 < Num_args ? std::strtol(Args[i + 1], &end, 10) : -1;
		if (!end || *end || index < 0 || index >= Num_walls) {
			std::fprintf(stderr, "XL preview wall index invalid\n");
			return 1;
		}
		const wall &w = Walls[index];
		vms_vector target, direction;
		compute_segment_center(&ConsoleObject->pos, &Segments[w.segnum]);
		compute_center_point_on_side(&target, &Segments[w.segnum], w.sidenum);
		vm_vec_sub(&direction, &target, &ConsoleObject->pos);
		vm_vector_2_matrix(&ConsoleObject->orient, &direction, nullptr, nullptr);
		obj_relink(Players[Player_num].objnum, w.segnum);
		std::fprintf(stdout, "XL material camera repositioned to wall %ld; not a gameplay test\n", index);
		break;
	}
	running = true;
	return 0;
}

extern "C" void xl_preview_before_swap(void)
{
	if (!running || !xl_preview_vertex_calls()) return;
	if (++rendered_ticks != 30) return;
	save_screen_shot(0);
	std::fprintf(stdout, "XL preview capture requested after %llu shaded vertices; fidelity and gameplay unverified\n",
	             static_cast<unsigned long long>(xl_preview_vertex_calls()));
	std::fflush(stdout);
	std::fprintf(stdout, "XL dynamic preview unresolved emitters: %u; reference rendering and other lighting modes unverified\n", xl_preview_unresolved_emitters());
	std::fflush(stdout);
}
