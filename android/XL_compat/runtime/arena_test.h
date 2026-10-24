extern "C" {
#include "physics.h"
#include "playsave.h"
int check_volatile_wall(object *, int, int, vms_vector *);
}

static int arena_remainder = 0;
static void arena_frame(const vms_vector *target)
{
	arena_remainder += F1_0;
	FrameTime = arena_remainder / 60;
	arena_remainder %= 60;
	std::memset(&Controls, 0, sizeof(Controls));
	if (target) {
		vms_vector direction;
		vm_vec_sub(&direction, target, &ConsoleObject->pos);
		const double distance = double(vm_vec_normalize(&direction)) / F1_0;
		const double speed = (std::min) (20.0, distance * 2);
		const auto &v = ConsoleObject->mtype.phys_info.velocity;
		const auto &m = ConsoleObject->orient;
		auto thrust = [&](const vms_vector &axis) {
			const double demand = (double(vm_vec_dot(&direction, &axis)) / F1_0 * speed - double(vm_vec_dot(&v, &axis)) / F1_0) / 12;
			return fix(std::clamp(demand, -1.0, 1.0) * FrameTime);
		};
		Controls.forward_thrust_time = thrust(m.fvec);
		Controls.sideways_thrust_time = thrust(m.rvec);
		Controls.vertical_thrust_time = thrust(m.uvec);
	}
	calc_game_time();
	GameProcessFrame();
	require(!Player_is_dead && Players[Player_num].shields > 0, "arena traversal killed player");
}

static int arena_traverse(int goal)
{
	require(goal >= 0 && goal < Num_segments, "invalid arena destination");
	for (int frame = 0; frame < 18000; ++frame) {
		const int start = ConsoleObject->segnum;
		require(start >= 0 && start < Num_segments, "invalid arena player segment");
		std::vector<int> previous(Num_segments, -1), queue{ start };
		previous[start] = start;
		for (std::size_t q = 0; q < queue.size() && previous[goal] < 0; ++q) {
			const int segment = queue[q];
			for (int side = 0; side < 6; ++side) {
				const int child = Segments[segment].children[side];
				if (child < 0 || child >= Num_segments || previous[child] >= 0 || !(WALL_IS_DOORWAY(&Segments[segment], side) & WID_FLY_FLAG)) continue;
				previous[child] = segment;
				queue.push_back(child);
			}
		}
		require(previous[goal] >= 0, "arena goal has no passable route");
		int next = goal;
		while (next != start && previous[next] != start) next = previous[next];
		vms_vector target;
		compute_segment_center(&target, &Segments[next]);
		if (start == goal && vm_vec_dist(&ConsoleObject->pos, &target) < F1_0) return frame;
		arena_frame(&target);
	}
	throw std::runtime_error("arena normal-input traversal exceeded frame bound");
}

static void arena_water_contract()
{
	require(!xl_preview_pilot_active() && !xl_preview_pilot_completed(), "water arena acquired Counterstrike completion identity");
	xl_texture_emitter emitter{};
	for (int segment = 0; segment < Num_segments; ++segment)
		for (int side = 0; side < 6; ++side)
			require(!xl_preview_texture_emitter(segment, side, &emitter), "unqualified corona emitter enabled for water arena");
	require(xl_preview_water_segment(187) && !xl_preview_water_segment(146) && !xl_preview_water_segment(-1) && !xl_preview_water_segment(188), "water segment bounds/identity");
	require(xl_preview_motion_time(187, 1000) == 750 && xl_preview_motion_time(146, 1000) == 1000, "water motion scaling");
	require(xl_preview_motion_time(187, INT32_MAX) == 1610612735, "water scale overflow");
	const auto saved = *ConsoleObject;
	const auto old_time = FrameTime;
	compute_segment_center(&ConsoleObject->pos, &Segments[187]);
	obj_relink(Players[Player_num].objnum, 187);
	ConsoleObject->mtype.phys_info.velocity = { F1_0, 0, 0 };
	ConsoleObject->mtype.phys_info.thrust = {};
	ConsoleObject->mtype.phys_info.rotvel = {};
	ConsoleObject->mtype.phys_info.rotthrust = {};
	ConsoleObject->mtype.phys_info.drag = 0;
	ConsoleObject->mtype.phys_info.flags = 0;
	FrameTime = F1_0 / 4;
	const auto start = ConsoleObject->pos;
	do_physics_sim(ConsoleObject);
	require(ConsoleObject->pos.x - start.x == 12288 && ConsoleObject->pos.y == start.y && ConsoleObject->pos.z == start.z, "native water displacement differs from reference scale");
	require(ConsoleObject->mtype.phys_info.velocity.x == F1_0, "water incorrectly scales stored free-flight velocity");
	// Slide along the original solid x=-25 wall, with a nonzero tangent velocity
	ConsoleObject->pos.x = -25 * F1_0 + ConsoleObject->size + F1_0 / 16;
	const auto collision_start = ConsoleObject->pos;
	ConsoleObject->mtype.phys_info.velocity = { -4 * F1_0, 0, F1_0 };
	do_physics_sim(ConsoleObject);
	vms_vector displacement;
	vm_vec_sub(&displacement, &ConsoleObject->pos, &collision_start);
	std::cout << "Water wall slide: segment=" << ConsoleObject->segnum << " size=" << ConsoleObject->size << " displacement=" << displacement.x << ',' << displacement.y << ',' << displacement.z
	          << " velocity=" << ConsoleObject->mtype.phys_info.velocity.x << ',' << ConsoleObject->mtype.phys_info.velocity.y << ',' << ConsoleObject->mtype.phys_info.velocity.z << '\n';
	require(ConsoleObject->segnum == 187 && displacement.x <= -F1_0 / 16 + 128 && displacement.x >= -F1_0 / 16 - 128 &&
	            displacement.z > F1_0 / 8 && displacement.z <= F1_0 / 4 && std::abs(displacement.y) < F1_0 / 32,
	        "native water wall slide did not collide and retain tangent motion");
	require(ConsoleObject->mtype.phys_info.velocity.x == 0 && ConsoleObject->mtype.phys_info.velocity.z > F1_0 / 2 &&
	            ConsoleObject->mtype.phys_info.velocity.z < 2 * F1_0,
	        "native water wall slide lost projected tangent velocity");
	compute_segment_center(&ConsoleObject->pos, &Segments[187]);
	const auto calls = d_rand_get_stream_call_count(D_RNG_SIM);
	require(!xl_preview_water_frame(Players[Player_num].objnum) && d_rand_get_stream_call_count(D_RNG_SIM) == calls, "idle water consumed RNG");
	ConsoleObject->mtype.phys_info.thrust.x = F1_0;
	require(xl_preview_water_frame(Players[Player_num].objnum) && d_rand_get_stream_call_count(D_RNG_SIM) == calls + 2, "water turbulence RNG accounting");
	// An overlapping ship radius near the original solid wall selects boundary turbulence
	ConsoleObject->pos.x = -25 * F1_0 + ConsoleObject->size / 2;
	char boundary[] = "water boundary fixture";
	require(get_seg_masks(&ConsoleObject->pos, 187, ConsoleObject->size, boundary, __LINE__).sidemask != 0, "water boundary fixture misses wall");
	d_srand_stream(D_RNG_SIM, 1);
	const int expected_x = (16383 - d_rand()) / 2, expected_z = (16383 - d_rand()) / 2;
	d_srand_stream(D_RNG_SIM, 1);
	const auto boundary_calls = d_rand_get_stream_call_count(D_RNG_SIM);
	require(xl_preview_water_frame(Players[Player_num].objnum) && ConsoleObject->mtype.phys_info.rotvel.x == expected_x &&
	            ConsoleObject->mtype.phys_info.rotvel.z == expected_z && d_rand_get_stream_call_count(D_RNG_SIM) == boundary_calls + 2,
	        "near-boundary water turbulence differs from reference samples/divisor");
	const auto shields = Players[Player_num].shields;
	require(check_volatile_wall(ConsoleObject, 187, 0, &ConsoleObject->pos) == 2 && Players[Player_num].shields == shields, "water wall contact damage/classification");
	require(Segments[187].sides[1].wall_num == 17 && Segments[187].sides[1].tmap_num == 399 &&
	            Walls[17].type == WALL_ILLUSION && (TmapInfo[399].flags & TMI_WATER),
	        "original water portal changed");
	require(xl_preview_water_sound(Players[Player_num].objnum, 1, SOUND_SHIP_IN_WATERFALL) == SOUND_SHIP_IN_WATERFALL, "texture waterfall sound lost");
	require(xl_preview_water_sound(Players[Player_num].objnum, 0, SOUND_SHIP_IN_WATERFALL) == SOUND_SHIP_IN_WATER, "segment contact misclassified as waterfall");
	int playing = xl_preview_fluid_loop(Players[Player_num].objnum, SOUND_SHIP_IN_WATERFALL, 0);
	require(playing == SOUND_SHIP_IN_WATERFALL + 1, "waterfall loop bookkeeping");
	playing = xl_preview_fluid_loop(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, playing);
	require(playing == SOUND_SHIP_IN_WATER + 1, "waterfall to interior loop did not switch");
	ConsoleObject->mtype.phys_info.thrust = {};
	const int silent = xl_preview_water_sound(Players[Player_num].objnum, 0, SOUND_SHIP_IN_WATERFALL);
	require(silent == -1 && xl_preview_fluid_loop(Players[Player_num].objnum, silent, playing) == 0, "idle segment contact retained fluid loop");
	obj_relink(Players[Player_num].objnum, saved.segnum);
	const int next = ConsoleObject->next, prev = ConsoleObject->prev;
	*ConsoleObject = saved;
	ConsoleObject->next = next;
	ConsoleObject->prev = prev;
	FrameTime = old_time;
}

// Private original-mission check, included after the lifecycle test helpers
static std::vector<int64_t> arena_state()
{
	const auto &o = *ConsoleObject;
	unsigned rng = 0;
	require(d_rand_get_stream_state(D_RNG_SIM, &rng), "simulation RNG state unavailable");
	return { o.segnum, o.pos.x, o.pos.y, o.pos.z, o.mtype.phys_info.velocity.x, o.mtype.phys_info.velocity.y, o.mtype.phys_info.velocity.z,
		     o.orient.rvec.x, o.orient.rvec.y, o.orient.rvec.z, o.orient.uvec.x, o.orient.uvec.y, o.orient.uvec.z,
		     o.orient.fvec.x, o.orient.fvec.y, o.orient.fvec.z, Players[Player_num].shields, Players[Player_num].energy,
		     GameTime64, rng };
}

static std::vector<unsigned char> arena_model()
{
	require(N_polygon_models > 108, "stock model index outside loaded table");
	const auto &model = Polygon_models[108];
	require(model.model_data && model.model_data_size > 0 && model.model_data_size < 1024 * 1024, "invalid model payload bounds");
	return { model.model_data, model.model_data + model.model_data_size };
}

static int arena_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		new_player_config();
		require(load_base_mission("d2", error, sizeof(error)), error);
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		const auto stock_model = arena_model();
		const auto stock_definitions = gameplay_definitions();
		require(load_base_mission("rtarena", error, sizeof(error)), error);
		std::strcpy(Players[Player_num].callsign, "XLWater");
		Difficulty_level = 2;
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(Current_level_num == 1 && !std::strcmp(Gamesave_current_filename, "rtarena.rl2"), "arena startup lost mission identity");
		require(Num_segments == 188 && Num_vertices == 540 && Num_walls == 18, "arena geometry differs");
		require(bm_compatibility_warning_count() == 0, "arena assets have compatibility warnings");
		require(N_polygon_models > 108 && Polygon_models[108].model_data_size == 17892, "original HXM model replacement absent");
		const auto custom_model = arena_model();
		require(custom_model != stock_model, "HXM did not replace stock model");
		ObjectDependencies graph;
		graph.sound(SOUND_SHIP_IN_WATER);
		graph.sound(SOUND_SHIP_IN_WATERFALL);
		for (int i = 0; i <= Highest_object_index; ++i) {
			const auto &o = Objects[i];
			if (o.type == OBJ_POWERUP) graph.powerup(o.id);
			if (o.type != OBJ_NONE && o.render_type == RT_POLYOBJ) graph.model(o.rtype.pobj_info.model_num);
		}
		for (const auto id : Primary_weapon_to_weapon_info) graph.weapon(id);
		for (const auto id : Secondary_weapon_to_weapon_info) graph.weapon(id);
		for (int id = LASER_ID_L1; id <= LASER_ID_L4; ++id) graph.weapon(id);
		graph.weapon(LASER_ID_L5);
		graph.weapon(LASER_ID_L6);
		graph.weapon(FLARE_ID);
		for (int wall = 0; wall < Num_walls; ++wall) {
			require(Walls[wall].clip_num == -1, "arena wall animation inventory changed");
			graph.wall_clip(Walls[wall].clip_num);
		}
		for (int seg = 0; seg < Num_segments; ++seg)
			for (const auto &side : Segments[seg].sides) {
				graph.texture(side.tmap_num);
				if (side.tmap_num2) graph.texture(side.tmap_num2 & 0x3fff);
			}
		std::cout << "Arena original load and resource graph: " << graph.models.size() << " models, " << graph.bitmaps.size() << " bitmaps, " << graph.sounds.size() << " sounds\n";
		arena_water_contract();
		std::cout << "Arena physical water contract passed\n";
		d_srand_stream(D_RNG_SIM, 1);
		d_srand_stream(D_RNG_FX, 1);
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		const int into = arena_traverse(187);
		std::cout << "Arena water reached through ordinary controls in " << into << " frames\n";
		char save[] = "xl-water.sav", description[] = "Water segment checkpoint";
		// The low-level native save consumes its caller's pause
		stop_time();
		require(state_save_all_sub(save, description) != 0, "water save failed");
		const auto saved_state = arena_state();
		const auto saved_position = ConsoleObject->pos;
		const int saved_remainder = arena_remainder;
		const auto saved_rng_calls = d_rand_get_stream_call_count(D_RNG_SIM);
		const int out = arena_traverse(146);
		const auto expected_state = arena_state();
		const auto expected_rng_calls = d_rand_get_stream_call_count(D_RNG_SIM) - saved_rng_calls;
		require(!xl_preview_water_segment(ConsoleObject->segnum), "water leaked after exit");
		require(state_restore_all_sub(save, 0) != 0, "water restore failed");
		require(ConsoleObject->segnum == 187 && !std::memcmp(&ConsoleObject->pos, &saved_position, sizeof(saved_position)) && xl_preview_water_segment(187), "water checkpoint did not restore property/position");
		require(d_rand_get_stream_call_count(D_RNG_SIM) == 0, "native restore did not reset diagnostic SIM count");
		const auto restored_state = arena_state();
		for (std::size_t i = 0; i < saved_state.size(); ++i)
			if (saved_state[i] != restored_state[i]) std::cout << "Restore field " << i << ": " << saved_state[i] << " -> " << restored_state[i] << '\n';
		require(restored_state == saved_state, "water checkpoint restored a different simulation state");
		arena_remainder = saved_remainder;
		const int replay_frames = arena_traverse(146);
		const auto replay_state = arena_state();
		std::cout << "Replay frames " << out << " -> " << replay_frames << '\n';
		for (std::size_t i = 0; i < expected_state.size(); ++i)
			if (expected_state[i] != replay_state[i]) std::cout << "Replay field " << i << ": " << expected_state[i] << " -> " << replay_state[i] << '\n';
		require(d_rand_get_stream_call_count(D_RNG_SIM) == expected_rng_calls, "water replay consumed different SIM samples");
		require(replay_frames == out && replay_state == expected_state, "water checkpoint replay diverged");
		std::cout << "Arena water exit: " << out << " frames; checkpoint replay matches position/orientation/velocity/player/time/SIM RNG\n";
		require(load_base_mission("d2", error, sizeof(error)), error);
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(!xl_preview_water_segment(187) && xl_preview_motion_time(187, 1000) == 1000, "water state leaked into stock");
		require(arena_model() == stock_model && gameplay_definitions() == stock_definitions, "custom gameplay assets leaked into stock");
		require(load_base_mission("rtarena", error, sizeof(error)), error);
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_preview_water_segment(187) && arena_model() == custom_model, "arena did not recover after stock transition");
		std::cout << "Arena -> stock -> arena: water, HXM model and gameplay definition isolation passed\n";
		PHYSFS_file *original = PHYSFS_openRead("rtarena.rl2");
		require(original && PHYSFS_fileLength(original) == 80598, "cannot read bounded original arena for rejection test");
		xl_compat::Bytes changed(80598);
		const auto count = PHYSFS_read(original, changed.data(), 1, changed.size());
		PHYSFS_close(original);
		require(count == PHYSFS_sint64(changed.size()), "cannot read original arena bytes");
		changed.back() ^= 1;
		const auto mutation = std::filesystem::path(find_arg_value(argc, argv, "-userdir")) / "mutation";
		std::filesystem::create_directories(mutation);
		const auto shadow = (mutation / "rtarena.rl2").string();
		write_bytes(shadow.c_str(), changed);
		require(PHYSFS_addToSearchPath(mutation.string().c_str(), 0), "cannot mount private mutation");
		const auto resolved = PHYSFS_getRealDir("rtarena.rl2");
		require(resolved && std::filesystem::equivalent(resolved, mutation), "negative probe did not resolve to changed bytes");
		const int rejected = load_level("rtarena.rl2");
		require(PHYSFS_removeFromSearchPath(mutation.string().c_str()), "cannot unmount private mutation");
		require(std::filesystem::remove(shadow), "cannot remove private changed-source shadow");
		require(rejected != 0 && !xl_preview_water_segment(187), "changed arena source admitted or stale water retained");
		require(load_level("rtarena.rl2") == 0 && xl_preview_water_segment(187), "arena admission did not recover");
		std::cout << "Changed source rejected, water cleared, original admission recovered\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "Arena test FAILED: " << e.what() << '\n';
		return 1;
	}
}
