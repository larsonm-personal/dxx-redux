#ifndef DXX_XL_ACTOR_AUDIO_TEST_H
#define DXX_XL_ACTOR_AUDIO_TEST_H
#include "combat_audio_test.h"
#include "boss_spawn.h"
#include "boss_routes.h"
extern "C" {
#include "robot.h"
#include "ai.h"
#include "cntrlcen.h"
#include "piggy.h"
int do_any_robot_dying_frame(object *actor);
}
namespace xl_actor_audio_control
{
using xl_combat_audio_control::check;
template <class Output>
void run(Output output)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	check(Current_level_num > 0 && !xl_boss_spawn_count(), "Actor audio controls require the nonsecret, boss-free derived fixture");
	const auto initial = named_audio_snapshot();
	struct Guard {
		object *viewer = Viewer;
		vms_vector player_position = ConsoleObject->pos;
		fix64 time = GameTime64;
		fix frame = FrameTime, countdown = Countdown_timer;
		int destroyed = Control_center_destroyed, paused = Reactor_countdown_paused, total = Total_countdown_time;
		control_center_triggers triggers = ControlCenterTriggers;
		unsigned sim = 0, fx = 0, sim_calls = d_rand_get_stream_call_count(D_RNG_SIM), fx_calls = d_rand_get_stream_call_count(D_RNG_FX);
		int id = -1;
		robot_info robot{};
		std::vector<int> signatures;
		Guard()
		{
			check(d_rand_get_stream_state(D_RNG_SIM, &sim) && d_rand_get_stream_state(D_RNG_FX, &fx), "Actor controls need stateful RNG");
			for (int i = 0; i <= Highest_object_index; ++i) signatures.push_back(Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature);
		}
		~Guard()
		{
			for (int i = Highest_object_index; i >= 0; --i)
				if (Objects[i].type != OBJ_NONE && (unsigned(i) >= signatures.size() || signatures[unsigned(i)] != Objects[i].signature)) obj_delete(i);
			if (id >= 0) Robot_info[id] = robot;
			xl_boss_spawn_load();
			Viewer = viewer;
			ConsoleObject->pos = player_position;
			GameTime64 = time;
			FrameTime = frame;
			Countdown_timer = countdown;
			Control_center_destroyed = destroyed;
			Reactor_countdown_paused = paused;
			Total_countdown_time = total;
			ControlCenterTriggers = triggers;
			d_rand_set_stream_state(D_RNG_SIM, sim);
			d_rand_set_stream_state(D_RNG_FX, fx);
			d_rand_set_stream_call_count(D_RNG_SIM, sim_calls);
			d_rand_set_stream_call_count(D_RNG_FX, fx_calls);
		}
	} guard;
	for (int id = 0; id < N_robot_types; ++id) {
		if (!robot_info_is_usable(id)) continue;
		bool used = false;
		for (int i = 0; i <= Highest_object_index; ++i) used |= Objects[i].type == OBJ_ROBOT && Objects[i].id == id;
		if (!used) {
			guard.id = id;
			guard.robot = Robot_info[id];
			break;
		}
	}
	check(guard.id >= 0, "Cannot find an unused actor definition");
	auto &robot = Robot_info[guard.id];
	robot.boss_flag = robot.thief = robot.companion = 0;
	robot.death_roll = 2;
	robot.deathroll_sound = robot.see_sound = 7;
	robot.strength = 100 * F1_0;
	Viewer = ConsoleObject;
	compute_segment_center(&Viewer->pos, &Segments[Viewer->segnum]);
	FrameTime = 0; // Isolate the real audio event from random secondary fireballs
	ControlCenterTriggers.num_links = 0;
	const auto create = [&] {
		const int index = obj_create(OBJ_ROBOT, guard.id, Viewer->segnum, &Viewer->pos, &Viewer->orient, F1_0, CT_AI, MT_PHYSICS, RT_POLYOBJ);
		check(index >= 0, "Cannot create actor audio object");
		Objects[index].rtype.pobj_info.model_num = robot.model_num;
		Objects[index].shields = robot.strength;
		return index;
	};
	const auto audible = [&](int logical, int sound_class, int volume) {
		const auto state = named_audio_snapshot();
		check(!Mix_Playing(-1) && state.at("voices").size() == 1, "Actor event failed to emit exactly one XL voice");
		const auto &voice = state.at("voices")[0];
		check(voice[2] == classic_sample(xl_gameplay_sound_sample(logical)) && voice[8] == sound_class && voice[6] == std::int64_t(volume) * priority_volume / F1_0,
		      "Actor event lost its source sample, class or gain");
		output();
	};
	SDL_PauseAudio(1);
	xl_named_audio_stop();
	int actor = create();
	Objects[actor].ctype.ai_info.dying_start_time = 10 * F1_0;
	Objects[actor].ctype.ai_info.dying_sound_playing = 0;
	GameTime64 = 12 * F1_0 - 1;
	check(do_any_robot_dying_frame(&Objects[actor]) && Objects[actor].ctype.ai_info.dying_sound_playing, "Actual ordinary robot groan did not start");
	audible(7, XL_SOUND_ROBOT, F1_0);
	xl_named_audio_stop();
	GameTime64 = 12 * F1_0 + 1;
	check(do_any_robot_dying_frame(&Objects[actor]), "Actual ordinary robot completion failed");
	audible(SOUND_BADASS_EXPLOSION, XL_SOUND_EXPLOSION, F2_0);
	obj_delete(actor);
	xl_named_audio_stop();
	robot.boss_flag = 1;
	actor = create();
	// The source teleport predicate requires any distant actor; this fixed
	// object avoids relying on incidental mission placement or navigation
	auto distant = Viewer->pos;
	int distant_segment = -1;
	for (int segment = 0; segment < Num_segments; ++segment) {
		compute_segment_center(&distant, &Segments[segment]);
		if (vm_vec_dist_quick(&distant, &Viewer->pos) > 20 * F1_0) {
			distant_segment = segment;
			break;
		}
	}
	check(distant_segment >= 0 && obj_create(OBJ_WEAPON, LASER_ID, distant_segment, &distant, &Viewer->orient, F1_0, CT_WEAPON, MT_NONE, RT_NONE) >= 0, "Cannot create teleport predicate control");
	xl_boss_spawn_load();
	check(xl_boss_spawn_count() == 1, "Controlled boss did not acquire independent runtime state");
	auto routes = boss_routes();
	routes[0].segments = { routes[0].home };
	check(restore_boss_routes(routes, 1), "Cannot bound controlled boss teleport destination");
	xl_boss_teleport(actor);
	const auto teleported = named_audio_snapshot();
	check(owners.count == 1 && owners.entries[0].object == actor && owners.entries[0].sound_class == XL_SOUND_ROBOT && owners.entries[0].sound == xl_gameplay_sound_sample(7) &&
	          owners.entries[0].max_distance == 512 * F1_0 && owners.entries[0].decay == XL_SOUND_ROBOT && teleported.at("voices").size() == 2 && !Mix_Playing(-1),
	      "Actual boss teleport lost its robot loop or morph cue");
	output();
	xl_named_audio_stop();
	GameTime64 = 20 * F1_0;
	Objects[actor].shields = -1;
	xl_boss_start_death(actor);
	check(xl_boss_is_dying(actor), "Controlled boss did not start its owned death");
	GameTime64 += BOSS_DEATH_DURATION - 1;
	xl_boss_deaths_frame();
	audible(7, XL_SOUND_ROBOT, 4 * F1_0);
	xl_named_audio_stop();
	GameTime64 += 2;
	xl_boss_deaths_frame();
	xl_boss_spawn_state ended{};
	check(xl_boss_spawn_state_get(0, &ended) && ended.dying == 2 && ended.owner == -1 && Control_center_destroyed, "Boss audio completion did not pass through real retirement/countdown");
	audible(58, XL_SOUND_EXPLOSION, 4 * F1_0);
	GameTime64 = guard.time;
	check(restore_named_audio(initial, true), "Cannot restore incoming actor audio state");
}
} // namespace xl_actor_audio_control
#endif
