#include "route_confirmation_desktop.h"

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
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
#include "named_audio.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"
#include "input_demo_state_trace.h"
#include "input_demo_rng_trace.h"
#include "input_demo_world_trace.h"
#include "input_demo_object_trace.h"
#endif
#ifdef DXX_XL_RUNTIME
#include "preview.h"
#include "particle_objects.h"
#include "particle_resources.h"
#include "resource_validation.h"
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
#include "input_demo_start_shared.h"
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
#include "gameseg.h"
#include "fvi.h"
#include "timer.h"
#include "robot.h"
#include "input_demo_hooks.h"
#include "input_demo_newdemo_shared.h"
#include "input_demo_hooks_shared.h"
void GameProcessFrame(void);
void omega_charge_frame(void);
void do_omega_stuff(object *, vms_vector *, object *);
void game_render_frame(void);
void game_render_frame_mono(int);
int HandleGameKey(int);
void do_weapon_n_item_stuff(void);
void set_robot_state(object *, int);
void ai_fire_laser_at_player(object *, vms_vector *, int, vms_vector *);
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
	int scene_object = -1;
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
	proof["assisted"] = bool{ false };
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
#if defined(DXX_XL_RUNTIME) && defined(OGL) && defined(DXX_XL_DEVELOPER_PROBE)
#ifdef HAVE_LIBPNG
	const char *screenshot_extension = "png";
#else
	const char *screenshot_extension = "tga";
#endif
	const std::string effect_screenshot = std::string("screenshots/scrn0000.") + screenshot_extension;
	const std::string empty_screenshot = std::string("screenshots/scrn0001.") + screenshot_extension;
	const std::string repeat_screenshot = std::string("screenshots/scrn0002.") + screenshot_extension;
	if (State.scene_object >= 0) {
		const int index = State.scene_object;
		if (index > Highest_object_index || Objects[index].type == OBJ_NONE || Objects[index].segnum < 0 || Objects[index].segnum >= Num_segments) {
			write_readiness(path, proof, "scene_failed");
			return 1;
		}
		int particles = 0;
		std::optional<xl_compat::ParticleFace> surface;
		std::optional<xl_compat::ParticlePoint> snow_point;
		for (const auto &system : xl_compat::particle_systems())
			if (system.object == index) {
				particles = system.count();
				std::visit([&](const auto &emitter) { surface = emitter.face; }, system.emitter);
				if (const auto *snow = std::get_if<xl_compat::SnowEmitter>(&system.emitter))
					if (snow->count) snow_point = snow->slots[std::size_t(snow->first)].motion.position;
			}
		if (!particles) {
			write_readiness(path, proof, "scene_failed");
			return 1;
		}
		const int segment = Objects[index].segnum;
		vms_vector center, forward, target, camera;
		compute_segment_center(&center, &Segments[segment]);
		target = Objects[index].pos;
		if (surface && surface->count >= 3 && surface->count <= 4)
			for (unsigned axis = 0; axis < 3; ++axis) {
				std::int64_t sum = 0;
				for (unsigned corner = 0; corner < surface->count; ++corner) sum += surface->corners[corner][axis];
				const int value = int(sum / surface->count);
				if (axis == 0) target.x = value;
				else if (axis == 1) target.y = value;
				else target.z = value;
			}
		if (snow_point) target = { (*snow_point)[0], (*snow_point)[1], (*snow_point)[2] };
		vm_vec_sub(&forward, &target, &center);
		if (vm_vec_normalize_quick(&forward) <= 0) {
			write_readiness(path, proof, "scene_failed");
			return 1;
		}
		for (int fraction : { snow_point ? 9 : 5, 7, 5, 0 }) {
			camera = { int((std::int64_t(center.x) * (10 - fraction) + std::int64_t(target.x) * fraction) / 10),
				       int((std::int64_t(center.y) * (10 - fraction) + std::int64_t(target.y) * fraction) / 10),
				       int((std::int64_t(center.z) * (10 - fraction) + std::int64_t(target.z) * fraction) / 10) };
			if (find_point_seg(&camera, segment) == segment) break;
		}
		ConsoleObject->pos = ConsoleObject->last_pos = camera;
		obj_relink(int(ConsoleObject - Objects), segment);
		vm_vector_2_matrix(&ConsoleObject->orient, &forward, nullptr, nullptr);
		vms_angvec camera_angles;
		vm_extract_angles_matrix(&camera_angles, &ConsoleObject->orient);
		vm_vec_zero(&ConsoleObject->mtype.phys_info.velocity);
		vm_vec_zero(&ConsoleObject->mtype.phys_info.rotvel);
		game_render_frame_mono(0);
		save_screen_shot(1);
		gr_flip();
		const auto effect_error = glGetError();
		if (!PHYSFS_exists(effect_screenshot.c_str())) {
			write_readiness(path, proof, "scene_failed");
			return 1;
		}
		proof["assisted_scene"] = { { "object", index }, { "segment", segment }, { "particles", particles }, { "assisted", bool{ true } }, { "view", snow_point ? "near_live_snow_particle" : "mid_segment_toward_authored_face" }, { "camera_position_fixed", { int(camera.x), int(camera.y), int(camera.z) } }, { "camera_pitch", int(camera_angles.p) }, { "camera_bank", int(camera_angles.b) }, { "camera_heading", int(camera_angles.h) }, { "face_corners", surface ? surface->count : 0 }, { "effect_screenshot", effect_screenshot }, { "empty_particle_control", empty_screenshot }, { "effect_gl_error", effect_error } };
		if (effect_error != GL_NO_ERROR) {
			write_readiness(path, proof, "scene_failed");
			return 1;
		}
	}
#endif
#ifdef DXX_XL_RUNTIME
	static constexpr const char *kinds[] = { "fire", "smoke", "bubble", "waterfall", "rain", "snow" };
	static constexpr int subtypes[] = { 3, 0, 2, 4, 5, 6 };
	const auto *fire = xl_prepared_particle_image(), *smoke = xl_prepared_particle_image(0), *bubble = xl_prepared_particle_image(2);
	const auto *rain = xl_prepared_particle_image(5), *snow = xl_prepared_particle_image(6);
	const auto *image = fire ? fire : smoke ? smoke
	                              : bubble  ? bubble
	                              : rain    ? rain
	                                        : snow;
	const xl_compat::ParticleBasis basis{ { 65536, 0, 0 }, { 0, 65536, 0 } };
	proof["particle_systems"] = nlohmann::ordered_json::array();
	for (const auto &system : xl_compat::particle_systems()) {
		const int index = system.object;
		if (index < 0 || index > Highest_object_index || !system.count()) continue;
		const int segment = Objects[index].segnum;
		nlohmann::ordered_json owner = { { "object", index }, { "segment", segment }, { "kind", kinds[system.emitter.index()] }, { "particles", system.count() } };
		if (image && segment >= 0 && segment < Num_segments &&
		    (system.emitter.index() == 2 || system.emitter.index() == 3 || system.emitter.index() == 5)) {
			owner["scene_view"] = "assisted_owner_position_segment_only";
			owner["scene_method"] = "native_geometry_quad";
			const auto &position = Objects[index].pos;
			const xl_compat::ParticlePoint eye{ position.x, position.y, position.z };
			const auto items = xl_compat::particle_scene_items({ segment }, eye, basis, int(SDL_GetTicks()),
			                                                   image->bitmap.width, image->bitmap.height,
			                                                   smoke ? smoke->bitmap.width : 0, smoke ? smoke->bitmap.height : 0,
			                                                   bubble ? bubble->bitmap.width : 0, bubble ? bubble->bitmap.height : 0);
			int matching = 0;
			for (const auto &item : items)
				if (item.subtype == subtypes[system.emitter.index()]) {
					++matching;
					if (matching == 1) {
						const auto quad = xl_compat::particle_scene_quad(item);
						owner["scene_alpha"] = quad.vertices[0].color[3];
						owner["scene_image"] = quad.image;
					}
				}
			owner["scene_items"] = matching;
		}
		proof["particle_systems"].push_back(std::move(owner));
	}
#endif
#if defined(DXX_XL_RUNTIME) && defined(OGL) && defined(DXX_XL_DEVELOPER_PROBE)
	if (State.scene_object >= 0) {
		xl_particle_objects_clear();
		game_render_frame_mono(0);
		save_screen_shot(1);
		gr_flip();
		proof["assisted_scene"]["control_gl_error"] = glGetError();
		game_render_frame_mono(0);
		save_screen_shot(1);
		gr_flip();
		proof["assisted_scene"]["repeat_control"] = repeat_screenshot;
		proof["assisted_scene"]["repeat_gl_error"] = glGetError();
		if (!PHYSFS_exists(empty_screenshot.c_str()) || !PHYSFS_exists(repeat_screenshot.c_str()) ||
		    proof["assisted_scene"]["control_gl_error"] != GL_NO_ERROR ||
		    proof["assisted_scene"]["repeat_gl_error"] != GL_NO_ERROR) {
			write_readiness(path, proof, "scene_failed");
			return 1;
		}
	}
#endif
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

int parse_scene_object(const char *text, int *index)
{
	char *end = nullptr;
	const long value = text ? strtol(text, &end, 10) : -1;
	if (!text || !end || *end || value < 0 || value > 100000) return 0;
	*index = int(value);
	return 1;
}

#if defined(DXX_XL_RUNTIME) && defined(OGL) && defined(DXX_XL_DEVELOPER_PROBE)
int original_hxm_actor_proof(const char *mode, const char *path)
{
	nlohmann::ordered_json proof = {
		{ "schema", "dxx-xl-original-hxm-actor-v1" }, { "mission", Current_mission_filename }, { "level", Current_level_num }, { "assisted", !strcmp(mode, "warm") }
	};
	const auto fail = [&](const char *reason) {
		proof["error"] = reason;
		write_readiness(path, proof, "failed");
		return 1;
	};
	// Source object 226 is authored in segment 528. The normal loader can relink
	// its position into another segment, so keep its object identity as the witness.
	constexpr int actor_index = 226;
	if (strcmp(Current_mission_filename, "wshock20") || Current_level_num != 5 ||
	    !xl_level_extensions_active() || Highest_object_index < actor_index ||
	    Objects[actor_index].type != OBJ_ROBOT || Objects[actor_index].id != 51)
		return fail("Original Warp 5 robot 51 object 226 is absent");
	proof["actor_index"] = actor_index;
	object *actor = &Objects[actor_index];
	proof["authored_segment"] = 528;
	proof["runtime_segment"] = int(actor->segnum);
	const auto &robot = Robot_info[51];
	if (robot.model_num != 75 || robot.n_guns != 2 || robot.gun_submodels[0] != 2 ||
	    robot.gun_submodels[1] != 4 || robot.weapon_type != 21)
		return fail("Original Warp 5 robot replacement was not installed");
	for (int gun = 0; gun <= MAX_GUNS; ++gun)
		for (int state = 0; state < N_ANIM_STATES; ++state) {
			const auto &list = robot.anim_states[gun][state];
			if ((gun < 2 && list.n_joints != 2) || (gun >= 2 && list.n_joints != 0) ||
			    (gun > 2 && list.offset != 0)) return fail("Warp 5 animation groups were not normalized correctly");
		}
	for (int state = 0; state < N_ANIM_STATES; ++state) {
		set_robot_state(actor, state);
		for (int gun = 0; gun < robot.n_guns; ++gun) {
			vms_vector muzzle;
			calc_gun_point(&muzzle, actor, gun);
			if (find_point_seg(&muzzle, actor->segnum) < 0) return fail("Warp 5 gun transform left the mine");
		}
	}
	if (!strcmp(mode, "warm")) {
		// Position assist is explicit in the proof; production AI and damage paths remain live.
		compute_segment_center(&ConsoleObject->pos, &Segments[actor->segnum]);
		ConsoleObject->last_pos = ConsoleObject->pos;
		obj_relink(int(ConsoleObject - Objects), actor->segnum);
		vms_vector muzzle;
		calc_gun_point(&muzzle, actor, 1);
		Ai_local_info[actor_index].next_fire = Ai_local_info[actor_index].next_fire2 = 0;
		ai_fire_laser_at_player(actor, &muzzle, 1, &ConsoleObject->pos);
		int shot = -1;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_WEAPON && Objects[i].id == 21 &&
			    Objects[i].ctype.laser_info.parent_num == actor_index) {
				if (shot >= 0) return fail("Warp 5 robot fired duplicate projectiles");
				shot = i;
			}
		if (shot < 0 || Ai_local_info[actor_index].next_fire <= 0) return fail("Warp 5 robot did not fire its replacement weapon");
		obj_delete(shot);
		if (GameTime64 < F1_0) GameTime64 = F1_0;
		const fix shields = actor->shields;
		if (shields <= i2f(7) || apply_damage_to_robot(actor, i2f(7), ConsoleObject - Objects) ||
		    actor->shields != shields - i2f(7)) return fail("Warp 5 robot did not take combat damage");
		proof["shot_weapon"] = 21;
		proof["shields_before"] = shields;
		proof["shields_after"] = int(actor->shields);
		stop_time();
		char save[] = "warp5.sav", description[] = "Original Warp 5 combat";
		if (!state_save_all_sub(save, description)) return fail("Warp 5 normal save failed");
		char stock[] = "d2";
		if (!load_mission_by_name(stock) || load_level("d2leva-1.rl2") || xl_level_extensions_active())
			return fail("Warp 5 stock transition failed");
		if (!state_restore_all_sub(save, 0) || Current_level_num != 5 || !xl_level_extensions_active() ||
		    Objects[actor_index].shields != shields - i2f(7)) return fail("Warp 5 warm restore lost combat state");
	} else {
		proof["shields_after"] = int(actor->shields);
		if (actor->shields <= 0 || game_is_time_paused()) return fail("Warp 5 cold restore did not resume combat state");
	}
	game_render_frame_mono(0);
	gr_flip();
	if (glGetError() != GL_NO_ERROR) return fail("Warp 5 rendered combat frame failed");
	proof["rendered_frame"] = true;
	if (!strcmp(mode, "cold")) {
		char stock[] = "d2";
		if (!load_mission_by_name(stock) || load_level("d2leva-1.rl2") || xl_level_extensions_active())
			return fail("Warp 5 cold stock transition failed");
	}
	return write_readiness(path, proof, "passed") ? 0 : 1;
}
#endif

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
	                                                    startup_find_cmd_arg("-xl-render-clock-proof") || startup_find_cmd_arg("-xl-omega-damage-proof") ||
	                                                    startup_find_cmd_arg("-xl-time-equipment-proof") || startup_find_cmd_arg("-xl-message-clock-proof"))) {
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
	if (startup_find_cmd_arg("-xl-omega-recording") && !omega_probe) {
		std::fprintf(stderr, "Omega recording requires the existing isolated damage-probe owner\n");
		return 1;
	}
	const char *equipment_probe = argument_value("-xl-time-equipment-proof");
	const char *message_probe = argument_value("-xl-message-clock-proof");
	const char *hxm_actor_probe = argument_value("-xl-hxm-actor-proof");
	const char *scene_object = argument_value("-xl-readiness-scene-object");
	if (startup_find_cmd_arg("-xl-readiness-scene-object") &&
	    (!readiness || !scene_object || !parse_scene_object(scene_object, &State.scene_object))) {
		std::fprintf(stderr, "Original effect scene assist requires rendered readiness and a bounded object index\n");
		return 1;
	}
	if (startup_find_cmd_arg("-xl-hxm-actor-proof") &&
	    (!hxm_actor_probe || (strcmp(hxm_actor_probe, "warm") && strcmp(hxm_actor_probe, "cold")) ||
	     !output || !argument_value("-route-confirm-user-dir"))) {
		std::fprintf(stderr, "Original HXM actor proof requires warm/cold mode, output and isolated user directory\n");
		return 1;
	}
	if (hxm_actor_probe) {
		error_init(nullptr);
		set_warn_func(warn_printf);
	}
	if (startup_find_cmd_arg("-xl-render-clock-proof") || startup_find_cmd_arg("-xl-omega-damage-proof") || startup_find_cmd_arg("-xl-time-equipment-proof") || startup_find_cmd_arg("-xl-message-clock-proof")) {
		if (int(clock_probe != nullptr) + int(omega_probe != nullptr) + int(equipment_probe != nullptr) + int(message_probe != nullptr) != 1 || !output || !argument_value("-route-confirm-user-dir")) {
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
#if defined(DXX_XL_RUNTIME) && defined(OGL) && defined(DXX_XL_DEVELOPER_PROBE)
	if (hxm_actor_probe && !strcmp(hxm_actor_probe, "cold")) {
		char save[] = "warp5.sav";
		if (!state_restore_all_sub(save, 0)) {
			std::fprintf(stderr, "Original HXM actor proof could not restore saved game\n");
			return 1;
		}
		return original_hxm_actor_proof(hxm_actor_probe, output);
	}
#endif
	StartNewGame(State.level);
	if (State.level < 0)
		Newdemo_state = ND_STATE_NORMAL;
	PlayerCfg.maxFps = ROUTE_CONFIRMATION_FIXED_HZ;
	if (game_is_time_paused())
		start_time();
	if (readiness) return rendered_readiness(readiness, proof);
#if defined(DXX_XL_RUNTIME) && defined(OGL) && defined(DXX_XL_DEVELOPER_PROBE)
	if (hxm_actor_probe) return original_hxm_actor_proof(hxm_actor_probe, output);
	if (clock_probe) return xl_render_clock_probe(clock_probe);
	if (message_probe) return xl_message_clock_probe(message_probe);
	if (omega_probe) return startup_find_cmd_arg("-xl-omega-recording") ? xl_omega_recording_probe(omega_probe) : xl_omega_damage_probe(omega_probe);
	if (equipment_probe) {
		int carried_level = 0;
		if (const char *text = argument_value("-xl-time-equipment-carried-level")) {
			if (!parse_level(text, &carried_level) || carried_level > Last_level || carried_level < Last_secret_level) {
				std::fprintf(stderr, "Time-equipment carried level is outside the selected mission\n");
				return 1;
			}
		}
		return xl_time_equipment_probe(equipment_probe, startup_find_cmd_arg("-xl-time-equipment-resume") != 0, carried_level,
		                               argument_value("-xl-time-equipment-transition"));
	}
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
