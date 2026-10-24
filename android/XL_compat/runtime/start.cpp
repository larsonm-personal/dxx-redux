// Reproducible developer preview startup; no gameplay assists
#include "preview.h"
#include <cstdio>
#include <cstring>
extern "C" {
#include "args.h"
#include "game.h"
#include "gameseq.h"
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
	if (!has_option("-xl-static-light-preview")) {
		std::fprintf(stderr, "XL preview startup requires -xl-static-light-preview\n");
		return 1;
	}
	char mission[] = "xlprobe";
	if (!load_mission_by_name(mission)) {
		std::fprintf(stderr, "XL preview cannot load private xlprobe mission\n");
		return 1;
	}
	std::strcpy(Players[Player_num].callsign, "XLTest");
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
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
}
