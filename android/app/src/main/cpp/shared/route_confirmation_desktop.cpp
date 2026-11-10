#include "route_confirmation_desktop.h"

#ifdef _WIN32
#include <winsock2.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <fstream>
#include <nlohmann/json.hpp>
#if defined(DXX_XL_RUNTIME) && defined(OGL) && defined(DXX_XL_DEVELOPER_PROBE)
#include <stdexcept>
#include "slow_motion_runtime.h"
#include "slow_motion_save.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#endif
#ifdef DXX_XL_RUNTIME
#include "preview.h"
#endif

#include <physfs.h>
#include <SDL.h>

#include "route_confirmation.h"
#include "route_confirmation_result.h"

extern "C" {
#include "args.h"
#include "config.h"
#include "dxxerror.h"
#include "endlevel.h"
#include "event.h"
#include "game.h"
#include "gameseq.h"
#include "gamesave.h"
#include "inferno.h"
#include "input_demo_start.h"
#include "mission.h"
#include "maths.h"
#include "newdemo.h"
#include "object.h"
#include "ogl_init.h"
#include "player.h"
#include "playsave.h"
#include "segment.h"
#include "startup_resume_shared.h"
#include "window.h"
void GameProcessFrame(void);
}

#if defined(DXX_XL_RUNTIME) && defined(OGL) && defined(DXX_XL_DEVELOPER_PROBE)
extern "C" {
#include "automap.h"
#include "dxxerror.h"
#include "endlevel.h"
#include "ogl_init.h"
#include "laser.h"
#include "weapon.h"
#include "state.h"
#include "ai.h"
#include "collide.h"
#include "key.h"
#include "input_demo_hooks.h"
#include "input_demo_newdemo_shared.h"
#include "input_demo_hooks_shared.h"
void GameProcessFrame(void);
void omega_charge_frame(void);
void do_omega_stuff(object *, vms_vector *, object *);
void game_render_frame(void);
void game_render_frame_mono(int);
}
#include "render_clock_probe.h"
#include "omega_damage_probe.h"
#endif

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

bool write_readiness(const char *path, nlohmann::ordered_json &proof, const char *phase)
{
	proof["phase"] = phase;
	std::ofstream output(path);
	output << proof.dump(2) << '\n';
	output.flush();
	return output.good();
}

int rendered_readiness(const char *path, nlohmann::ordered_json &proof)
{
	proof["mission"] = Current_mission_filename;
	proof["level"] = Current_level_num;
	proof["level_file"] = Gamesave_current_filename;
	proof["segments"] = Num_segments;
#ifdef DXX_XL_RUNTIME
	proof["xl_active"] = xl_level_extensions_active() != 0;
	proof["level_version"] = xl_source_level_version();
#else
	proof["xl_active"] = false;
#endif
	proof["frames"] = 0;
	proof["rendered_frames"] = 0;
	proof["fixed_hz"] = ROUTE_CONFIRMATION_FIXED_HZ;
	proof["seed"] = ROUTE_CONFIRMATION_CANONICAL_SEED;
	proof["assisted"] = false;
	const auto *renderer = glGetString(GL_RENDERER);
	proof["renderer"] = renderer ? reinterpret_cast<const char *>(renderer) : "";
	if (!renderer || !Game_wind || route_confirmation_get_summary()->status != ROUTE_CONFIRMATION_IDLE || game_is_time_paused() || !write_readiness(path, proof, "loaded"))
		return 1;
	int remainder = 0;
	for (int frame = 0; frame < 120; ++frame) {
		remainder += F1_0;
		FrameTime = remainder / ROUTE_CONFIRMATION_FIXED_HZ;
		remainder %= ROUTE_CONFIRMATION_FIXED_HZ;
		calc_game_time();
		GameProcessFrame();
		if (Current_level_num != State.level || Player_is_dead || Endlevel_sequence || game_is_time_paused()) {
			write_readiness(path, proof, "simulation_failed");
			return 1;
		}
		proof["frames"] = frame + 1;
		game_render_frame_mono(0);
		gr_flip();
		const auto error = glGetError();
		if (error != GL_NO_ERROR) {
			proof["gl_error"] = error;
			write_readiness(path, proof, "rendering_failed");
			return 1;
		}
		proof["rendered_frames"] = frame + 1;
	}
	return write_readiness(path, proof, "loaded_ticked") ? 0 : 1;
}

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
	if (startup_find_cmd_arg("-route-confirm-capture")) {
		game_render_frame_mono(0);
		save_screen_shot(0);
	}
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
	const char *readiness = argument_value("-xl-readiness-proof");
	if (startup_find_cmd_arg("-xl-readiness-proof") && (!readiness || !output ||
	                                                    startup_find_cmd_arg("-route-confirm-exit-trigger") || startup_find_cmd_arg("-route-confirm-speed-percent") ||
	                                                    startup_find_cmd_arg("-route-confirm-timeout-seconds") || startup_find_cmd_arg("-route-confirm-capture") ||
	                                                    startup_find_cmd_arg("-route-confirm-checkpoint") || startup_find_cmd_arg("-route-confirm-checkpoint-out") ||
	                                                    startup_find_cmd_arg("-route-confirm-key") || startup_find_cmd_arg("-route-confirm-segment") ||
	                                                    startup_find_cmd_arg("-route-confirm-native-transition") || startup_find_cmd_arg("-route-confirm-load-proof") ||
	                                                    startup_find_cmd_arg("-xl-render-clock-proof") || startup_find_cmd_arg("-xl-omega-damage-proof"))) {
		fprintf(stderr, "Rendered readiness requires proof/output paths and no assisted route options\n");
		return 1;
	}
	nlohmann::ordered_json proof;
	if (readiness) {
		// Fatal admission errors must exit unattended instead of opening a modal dialog
		error_init(nullptr);
		set_warn_func(warn_printf);
		proof["schema"] = "dxx-xl-rendered-load-tick-v1";
		if (!write_readiness(readiness, proof, "initializing")) return 1;
	}
#if defined(DXX_XL_RUNTIME) && defined(OGL) && defined(DXX_XL_DEVELOPER_PROBE)
	const char *clock_probe = argument_value("-xl-render-clock-proof");
	const char *omega_probe = argument_value("-xl-omega-damage-proof");
	if (startup_find_cmd_arg("-xl-render-clock-proof") || startup_find_cmd_arg("-xl-omega-damage-proof")) {
		if ((!clock_probe && !omega_probe) || (clock_probe && omega_probe) || !output || !argument_value("-route-confirm-user-dir")) {
			std::fprintf(stderr, "Render-clock probe requires proof/output paths and an isolated user directory\n");
			return 1;
		}
		error_init(nullptr);
		set_warn_func(warn_printf);
	}
#endif
	const char *level_text;
	const char *time_limit_text;
	const char *exit_goal;
	const char *extra_dir;
	std::string mission;
	if (!output)
		return -1;
	level_text = argument_value("-level");
	time_limit_text = argument_value("-route-confirm-timeout-seconds");
	exit_goal = argument_value("-route-confirm-exit-trigger");
	if (!parse_level(level_text, &State.level) ||
	    (startup_find_cmd_arg("-route-confirm-exit-trigger") && !exit_goal) ||
	    !route_confirmation_configure_exit_goal(exit_goal) ||
	    !route_confirmation_configure_speed(argument_value("-route-confirm-speed-percent")) ||
	    (time_limit_text && !configure_time_limit(time_limit_text))) {
		fprintf(stderr,
		        "usage: -route-confirm-json-out <path> -level <number> "
		        "[-mission <name>] [-extra-dir <path>] "
		        "[-route-confirm-timeout-seconds <1..3600>] "
		        "[-route-confirm-exit-trigger <number>] "
		        "[-route-confirm-speed-percent <100..200>]\n");
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
	if (readiness && !write_readiness(readiness, proof, "mission_load")) return 1;
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
	/* Match headless workers instead of inheriting an absent or interactive pilot */
	new_player_config();
	GameArg.SysUseNiceFPS = 1;
	GameArg.SysMaxFPS = ROUTE_CONFIRMATION_FIXED_HZ;
	PlayerCfg.maxFps = ROUTE_CONFIRMATION_FIXED_HZ;
	GameCfg.Grabinput = 0;
	input_demo_set_skip_level_intro(1);
	if (readiness) {
		d_srand_stream(D_RNG_SIM, ROUTE_CONFIRMATION_CANONICAL_SEED);
		d_srand_stream(D_RNG_FX, ROUTE_CONFIRMATION_CANONICAL_SEED);
		d_rand_reset_stream_call_count(D_RNG_SIM);
		d_rand_reset_stream_call_count(D_RNG_FX);
		Difficulty_level = 2;
		if (!write_readiness(readiness, proof, "level_load")) return 1;
	}
	if (State.level < 0)
		Newdemo_state = ND_STATE_PLAYBACK;
	StartNewGame(State.level);
	if (State.level < 0)
		Newdemo_state = ND_STATE_NORMAL;
	PlayerCfg.maxFps = ROUTE_CONFIRMATION_FIXED_HZ;
	if (game_is_time_paused())
		start_time();
	if (readiness) return rendered_readiness(readiness, proof);
#if defined(DXX_XL_RUNTIME) && defined(OGL) && defined(DXX_XL_DEVELOPER_PROBE)
	if (clock_probe) return xl_render_clock_probe(clock_probe);
	if (omega_probe) return xl_omega_damage_probe(omega_probe);
#endif
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
