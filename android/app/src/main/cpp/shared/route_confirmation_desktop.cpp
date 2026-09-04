#include "route_confirmation_desktop.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>

#include <physfs.h>
#include <SDL.h>

#include "route_confirmation.h"
#include "route_confirmation_result.h"

extern "C" {
#include "args.h"
#include "config.h"
#include "event.h"
#include "game.h"
#include "gameseq.h"
#include "inferno.h"
#include "input_demo_start.h"
#include "mission.h"
#include "newdemo.h"
#include "object.h"
#include "player.h"
#include "playsave.h"
#include "startup_resume_shared.h"
#include "window.h"
}

namespace
{
struct desktop_route_state {
	const char *output;
	const char *mission;
	int level;
	int active;
	unsigned int reported_frame;
};

desktop_route_state State = {};

void publish_progress(const route_confirmation_summary *summary)
{
	char caption[128];
	std::string path;
	FILE *file;
	if (!State.output || !summary)
		return;
	snprintf(caption, sizeof(caption),
	         "GuideBot route L%d: frame %u, objectives %d", State.level,
	         summary->frame_count, summary->objective_count);
	SDL_WM_SetCaption(caption, "GuideBot route");
	path = std::string(State.output) + ".progress";
	file = fopen(path.c_str(), "w");
	if (!file)
		return;
	fprintf(file,
	        "frames=%u\nobjectives=%d\nstatus=%s\npaused=%d\nfront=%d\n"
	        "max_fps=%d\nvsync=%d\n",
	        summary->frame_count, summary->objective_count,
	        route_confirmation_status_name(summary->status),
	        game_is_time_paused(), window_get_front() == Game_wind,
	        PlayerCfg.maxFps, GameCfg.VSync);
	fclose(file);
}

const char *argument_value(const char *name)
{
	const int index = startup_find_cmd_arg(name);
	if (!index || index + 1 >= Num_args || !Args[index + 1] ||
	    !Args[index + 1][0])
		return nullptr;
	return Args[index + 1];
}

int parse_level(const char *text, int *level)
{
	char *end = nullptr;
	const long value = text ? strtol(text, &end, 10) : 0;
	if (!text || !end || *end || value == 0 || value < -127 || value > 127)
		return 0;
	*level = static_cast<int>(value);
	return 1;
}

int configure_time_limit(const char *text)
{
	char *end = nullptr;
	const unsigned long value = text ? strtoul(text, &end, 10) : 0;
	return text && end && !*end && value <= 3600 &&
	       route_confirmation_set_time_limit_seconds(
	           static_cast<unsigned int>(value));
}

void finish_if_terminal()
{
	char error[256] = "";
	const route_confirmation_summary *summary;
	if (!State.active || !route_confirmation_is_terminal())
		return;
	State.active = 0;
	summary = route_confirmation_get_summary();
	if (!route_confirmation_write_json(State.output, State.mission, State.level,
	                                   error, sizeof(error)))
		fprintf(stderr, "ROUTE-CONFIRM FAIL %s\n", error);
	else
		printf("ROUTE-CONFIRM %s mission=%s level=%d frames=%u out=%s\n",
		       summary->status == ROUTE_CONFIRMATION_CONFIRMED ? "OK" : "FAIL",
		       State.mission && *State.mission ? State.mission : "d2",
		       State.level, summary->frame_count, State.output);
}
} // namespace

extern "C" int route_confirmation_desktop_maybe_start(void)
{
	char error[256] = "";
	const char *output = argument_value("-route-confirm-json-out");
	const char *level_text;
	const char *time_limit_text;
	const char *extra_dir;
	std::string mission;
	if (!output)
		return -1;
	level_text = argument_value("-level");
	time_limit_text = argument_value("-route-confirm-timeout-seconds");
	if (!parse_level(level_text, &State.level) ||
	    (time_limit_text && !configure_time_limit(time_limit_text))) {
		fprintf(stderr,
		        "usage: -route-confirm-json-out <path> -level <number> "
		        "[-mission <name>] [-extra-dir <path>] "
		        "[-route-confirm-timeout-seconds <1..3600>]\n");
		return 1;
	}
	extra_dir = argument_value("-extra-dir");
	if (extra_dir && !PHYSFS_addToSearchPath(extra_dir, 0)) {
		fprintf(stderr, "ROUTE-CONFIRM FAIL could not mount extra dir %s\n",
		        extra_dir);
		return 1;
	}
	State.output = output;
	State.mission = argument_value("-mission");
	mission = State.mission && *State.mission ? State.mission : "d2";
	if (!load_mission_by_name(&mission[0])) {
		fprintf(stderr, "ROUTE-CONFIRM FAIL could not load mission %s\n",
		        mission.c_str());
		return 1;
	}
	if ((State.level > 0 && State.level > Last_level) ||
	    (State.level < 0 && State.level < Last_secret_level)) {
		fprintf(stderr, "ROUTE-CONFIRM FAIL level %d outside mission range\n",
		        State.level);
		return 1;
	}
	snprintf(Players[Player_num].callsign,
	         sizeof(Players[Player_num].callsign), "%s", "RouteBot");
	GameArg.SysUseNiceFPS = 1;
	GameArg.SysMaxFPS = ROUTE_CONFIRMATION_FIXED_HZ;
	PlayerCfg.maxFps = ROUTE_CONFIRMATION_FIXED_HZ;
	GameCfg.Grabinput = 0;
	input_demo_set_skip_level_intro(1);
	if (State.level < 0)
		Newdemo_state = ND_STATE_PLAYBACK;
	StartNewGame(State.level);
	if (State.level < 0)
		Newdemo_state = ND_STATE_NORMAL;
	PlayerCfg.maxFps = ROUTE_CONFIRMATION_FIXED_HZ;
	if (game_is_time_paused())
		start_time();
	State.active = 1;
	event_toggle_focus(0);
	if (!route_confirmation_start()) {
		snprintf(error, sizeof(error), "%s",
		         route_confirmation_get_summary()->problem);
		fprintf(stderr, "ROUTE-CONFIRM FAIL start %s\n", error);
	}
	publish_progress(route_confirmation_get_summary());
	fprintf(stderr,
	        "ROUTE-CONFIRM started mission=%s level=%d paused=%d front=%d "
	        "game_window=%d status=%d\n",
	        State.mission && *State.mission ? State.mission : "d2", State.level,
	        game_is_time_paused(), window_get_front() == Game_wind,
	        Game_wind != nullptr, route_confirmation_get_summary()->status);
	fflush(stderr);
	finish_if_terminal();
	return 0;
}

extern "C" void route_confirmation_desktop_after_frame(void)
{
	const route_confirmation_summary *summary = route_confirmation_get_summary();
	if (State.active && summary->frame_count >= State.reported_frame + ROUTE_CONFIRMATION_FIXED_HZ * 10) {
		State.reported_frame = summary->frame_count;
		publish_progress(summary);
		printf("ROUTE-CONFIRM progress mission=%s level=%d frames=%u\n",
		       State.mission && *State.mission ? State.mission : "d2",
		       State.level, summary->frame_count);
		fflush(stdout);
	}
	finish_if_terminal();
}

extern "C" int route_confirmation_desktop_is_active(void)
{
	return State.active;
}

extern "C" int route_confirmation_desktop_should_exit(void)
{
	return State.output && !State.active;
}
