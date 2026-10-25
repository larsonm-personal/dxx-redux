#include "resource_validation.h"
#include "input_demo_replay.h"
#include <map>
extern "C" {
#include "physics.h"
#include "playsave.h"
#include "gamepal.h"
#include "palette.h"
int check_volatile_wall(object *, int, int, vms_vector *);
int d1_in_d2_load_mission_assets(void);
void reset_level_robots_file(void);
extern int Robot_replacements_loaded;
}

static int arena_remainder = 0;
static bool arena_recording = false;
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
	if (arena_recording) input_demo_record_input_sample();
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
	require(xl_linked_sound_count(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, 0) == 1, "water loop did not create a native sound object");
	digi_kill_sound_linked_to_object(Players[Player_num].objnum);
	require(xl_linked_sound_count(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, 0) == 0, "native sound reset did not remove water loop");
	playing = xl_preview_fluid_loop(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, playing);
	require(xl_linked_sound_count(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, 0) == 1, "water loop did not recover after native sound reset");
	playing = xl_preview_fluid_loop(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, playing);
	require(xl_linked_sound_count(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, 0) == 1, "water loop duplicated native sound object");
	ConsoleObject->mtype.phys_info.thrust = {};
	const int silent = xl_preview_water_sound(Players[Player_num].objnum, 0, SOUND_SHIP_IN_WATERFALL);
	require(silent == -1 && xl_preview_fluid_loop(Players[Player_num].objnum, silent, playing) == 0, "idle segment contact retained fluid loop");
	require(xl_linked_sound_count(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, 0) == 0, "idle water left native loop linked");
	int filled = 0;
	while (filled < 1000 && digi_link_sound_to_object3(SOUND_SHIP_IN_WATERFALL, Players[Player_num].objnum, 1, F1_0, i2f(256), -1, -1) >= 0) ++filled;
	require(filled > 0 && filled < 1000, "sound registry exhaustion fixture failed");
	playing = xl_preview_fluid_loop(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, 0);
	require(playing == 0 && xl_linked_sound_count(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, 0) == 0, "failed fluid allocation was marked playing");
	digi_kill_sound_linked_to_object(Players[Player_num].objnum);
	playing = xl_preview_fluid_loop(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, playing);
	require(playing == SOUND_SHIP_IN_WATER + 1 && xl_linked_sound_count(Players[Player_num].objnum, SOUND_SHIP_IN_WATER, 0) == 1, "water loop did not retry after registry capacity recovered");
	digi_kill_sound_linked_to_object(Players[Player_num].objnum);
	obj_relink(Players[Player_num].objnum, saved.segnum);
	const int next = ConsoleObject->next, prev = ConsoleObject->prev;
	*ConsoleObject = saved;
	ConsoleObject->next = next;
	ConsoleObject->prev = prev;
	FrameTime = old_time;
}

static void arena_fluid_objects()
{
	const auto old_time = FrameTime;
	FrameTime = F1_0 / 4;
	vms_matrix orientation = vmd_identity_matrix;
	for (const auto type : { OBJ_ROBOT, OBJ_WEAPON }) {
		vms_vector start;
		compute_segment_center(&start, &Segments[187]);
		const int index = obj_create(type, 0, 187, &start, &orientation, F1_0 / 8, type == OBJ_WEAPON ? CT_WEAPON : CT_NONE, MT_PHYSICS, RT_NONE);
		require(index >= 0, "cannot create fluid motion probe");
		auto &probe = Objects[index];
		probe.mtype.phys_info.mass = F1_0;
		probe.mtype.phys_info.drag = 0;
		probe.mtype.phys_info.flags = 0;
		probe.mtype.phys_info.velocity = { F1_0, 0, 0 };
		probe.mtype.phys_info.thrust = {};
		probe.mtype.phys_info.rotvel = {};
		do_physics_sim(&probe);
		require(probe.pos.x - start.x == 12288 && probe.pos.y == start.y && probe.pos.z == start.z &&
		            probe.mtype.phys_info.velocity.x == F1_0,
		        "non-player water displacement differs from reference scale");
		probe.mtype.phys_info.thrust.x = F1_0;
		const auto calls = d_rand_get_stream_call_count(D_RNG_SIM);
		require(!xl_preview_water_frame(index) && d_rand_get_stream_call_count(D_RNG_SIM) == calls,
		        "non-player water motion entered the reference's player-only turbulence path");
		probe.mtype.phys_info.thrust = {};
		if (type == OBJ_WEAPON) {
			// A terminating wall hit reaches the final reconstruction branch, unlike
			// a successful wall slide that retries and finishes with HIT_NONE
			probe.pos.x = -25 * F1_0 + probe.size + F1_0 / 16;
			const auto collision_start = probe.pos;
			probe.mtype.phys_info.velocity = { -4 * F1_0, 0, 0 };
			probe.ctype.laser_info.parent_type = OBJ_PLAYER;
			probe.ctype.laser_info.parent_num = Players[Player_num].objnum;
			probe.ctype.laser_info.parent_signature = ConsoleObject->signature;
			do_physics_sim(&probe);
			vms_vector moved, expected;
			vm_vec_sub(&moved, &probe.pos, &collision_start);
			vm_vec_copy_scale(&expected, &moved, 3 * F1_0); // (1 / 0.25 seconds) * 75 percent
			require((probe.flags & OF_SHOULD_BE_DEAD) && moved.x < 0 && moved.x > -F1_0 / 8,
			        "weapon probe did not terminate against the authored water wall");
			require(!std::memcmp(&probe.mtype.phys_info.velocity, &expected, sizeof(expected)),
			        "final water wall velocity omitted the reference reconstruction scale");
		}
		obj_delete(index);
	}
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
		if (const auto replay = find_arg_value(argc, argv, "-arena-replay")) {
			std::strcpy(Players[Player_num].callsign, "XLWater");
			input_demo_set_skip_level_intro(1);
			require(input_demo_load_replay_from_path(replay, error, sizeof(error)), error);
			const auto frames = input_demo_replay_frame_count();
			require(frames > 0 && frames < 18000 && input_demo_start_loaded_replay() == 0, "cannot start bounded arena input replay");
			require(!xl_preview_water_segment(ConsoleObject->segnum), "arena replay did not start at dry spawn");
			unsigned completed = 0;
			while (input_demo_replay_is_loaded() && completed < frames) {
				require(input_demo_step_replay_frame(), "arena input replay stopped before completing its frames");
				++completed;
			}
			require(completed == frames && !input_demo_replay_is_loaded() && input_demo_replay_actual_result_written(),
			        "arena input replay did not produce a terminal result");
			require(!Player_is_dead && ConsoleObject->segnum == 187 && xl_preview_water_segment(187),
			        "arena replay did not reach water alive");
			std::cout << "Arena spawn replay completed through native ordered input: " << completed << " frames\n";
			return 0;
		}
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
		{
			const auto descriptor_path = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions" / "rtarena.mn2";
			std::ifstream input(descriptor_path, std::ios::binary);
			const std::string original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
			input.close();
			require(!original.empty(), "cannot read private descriptor mutation control");
			const auto changed = original + "\nunsupported_runtime_option = yes\n";
			write_bytes(descriptor_path.string().c_str(), xl_compat::Bytes(changed.begin(), changed.end()));
			require(xl_level_supported("rtarena.rl2", 0), "selected mission reopened changed descriptor requirements");
			require(load_base_mission("rtarena", error, sizeof(error)), error);
			require(!xl_level_supported("rtarena.rl2", 0) && std::strstr(xl_level_rejection(), "unsupported_runtime_option"),
			        "reselection did not acquire changed descriptor requirements");
			write_bytes(descriptor_path.string().c_str(), xl_compat::Bytes(original.begin(), original.end()));
			require(!xl_level_supported("rtarena.rl2", 0) && std::strstr(xl_level_rejection(), "unsupported_runtime_option"),
			        "editing disk descriptor bypassed selected mission requirements");
			require(load_base_mission("rtarena", error, sizeof(error)), error);
			require(xl_level_supported("rtarena.rl2", 0), "original descriptor requirements did not recover on reselection");
			input_demo_set_skip_level_intro(1);
			StartNewGame(1);
			std::cout << "Mission requirement snapshot: source edits cannot change admission until reselection; rejection and recovery follow owned descriptors\n";
		}
		{
			const int weapon_count = N_weapon_types;
			N_weapon_types = 0;
			const bool accepted = xl_level_supported("rtarena.rl2", 0) != 0;
			N_weapon_types = weapon_count;
			require(!accepted && std::strstr(xl_level_rejection(), "weapon"), "water exception bypassed common weapon dependencies");
			require(xl_level_supported("rtarena.rl2", 0), "water resource dependency check did not recover");
		}
		// A valid RL2 must not hide a malformed companion behind the developer option
		const auto resource_shadow = std::filesystem::path(find_arg_value(argc, argv, "-userdir")) / "resource-shadow";
		std::filesystem::create_directories(resource_shadow);
		{
			const auto path = resource_shadow / "descent2.ham";
			PHYSFS_file *base_ham = PHYSFSX_openReadBuffered("descent2.ham");
			require(base_ham != nullptr, "cannot read base HAM identity control");
			xl_compat::Bytes same_ham(static_cast<std::size_t>(PHYSFS_fileLength(base_ham)));
			const auto ham_read = PHYSFS_readBytes(base_ham, same_ham.data(), same_ham.size());
			PHYSFS_close(base_ham);
			require(ham_read == PHYSFS_sint64(same_ham.size()), "cannot copy base HAM identity control");
			write_bytes(path.string().c_str(), same_ham);
			require(PHYSFS_addToSearchPath(resource_shadow.string().c_str(), 0), "cannot mount base HAM shadow control");
			require(xl_level_supported("rtarena.rl2", 0), "identical base HAM from a different mounted source was rejected");
			write_bytes(path.string().c_str(), xl_compat::Bytes{ 'B', 'A', 'D', '!' });
			const auto definitions = gameplay_definitions();
			const auto model = arena_model();
			require(*xl_resource_base_rejection(), "detected XL mission did not check its base HAM source");
			xl_resource_select_mission(0);
			require(*xl_resource_base_rejection(), "mounted XL detection depended on the browser hint");
			xl_resource_select_mission(1);
			require(d1_in_d2_load_mission_assets() == 0 && gameplay_definitions() == definitions && arena_model() == model,
			        "HAM rejection freed or replaced live definitions");
			const int replacements = Robot_replacements_loaded;
			reset_level_robots_file();
			require(replacements && Robot_replacements_loaded == replacements && gameplay_definitions() == definitions && arena_model() == model,
			        "blocked HAM reset discarded active replacement state");
			require(!xl_level_supported("rtarena.rl2", 0) && std::strstr(xl_level_rejection(), "base HAM"),
			        "shadowed base HAM was not explained by level preflight");
			require(PHYSFS_removeFromSearchPath(resource_shadow.string().c_str()), "cannot unmount base HAM shadow control");
			require(std::filesystem::remove(path), "cannot remove base HAM shadow control");
			require(xl_level_supported("rtarena.rl2", 0), "base HAM source recovery failed");
		}
		for (const char *bank : { "descent2.s11", "descent2.s22" }) {
			const auto path = resource_shadow / bank;
			PHYSFS_file *file = PHYSFSX_openReadBuffered(bank);
			xl_compat::Bytes original;
			if (file) {
				original.resize(static_cast<std::size_t>(PHYSFS_fileLength(file)));
				const auto read = PHYSFS_readBytes(file, original.data(), original.size());
				PHYSFS_close(file);
				require(read == PHYSFS_sint64(original.size()), "cannot copy base sound identity control");
				write_bytes(path.string().c_str(), original);
			}
			require(PHYSFS_addToSearchPath(resource_shadow.string().c_str(), 0), "cannot mount sound bank control");
			require(xl_level_supported("rtarena.rl2", 0), "identical or absent base sound bank rejected");
			const auto definitions = gameplay_definitions();
			const auto model = arena_model();
			const auto state = arena_state();
			const int replacements = Robot_replacements_loaded;
			const auto sounds = std::string(reinterpret_cast<const char *>(GameSounds), sizeof(GameSounds));
			write_bytes(path.string().c_str(), xl_compat::Bytes{ 'B', 'A', 'D', '!' });
			xl_resource_select_mission(0);
			require(std::strstr(xl_resource_base_rejection(), bank), "mounted XL detection missed changed sound bank");
			xl_resource_select_mission(1);
			require(!xl_level_supported("rtarena.rl2", 0) && std::strstr(xl_level_rejection(), bank), "sound bank rejection lacks resource name");
			require(d1_in_d2_load_mission_assets() == 0, "changed sound bank initialized mission resources");
			reset_level_robots_file();
			require(gameplay_definitions() == definitions && arena_model() == model && arena_state() == state &&
			            Robot_replacements_loaded == replacements &&
			            sounds == std::string(reinterpret_cast<const char *>(GameSounds), sizeof(GameSounds)),
			        "sound bank rejection changed active definitions, samples or replacement state");
			require(PHYSFS_removeFromSearchPath(resource_shadow.string().c_str()), "cannot unmount sound bank control");
			require(std::filesystem::remove(path), "cannot remove sound bank control");
			require(xl_level_supported("rtarena.rl2", 0), "sound bank source recovery failed");
		}
		std::cout << "XL base sound banks: identical sources accepted, changed banks rejected before resource mutation, recovery passed\n";
		{
			std::string palette = Current_level_palette;
			for (char &c : palette)
				if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
			const auto pig = palette.substr(0, palette.find_last_of('.')) + ".pig";
			auto graphics = [] {
				return std::string(reinterpret_cast<const char *>(gr_palette), sizeof(gr_palette)) +
				       std::string(reinterpret_cast<const char *>(gr_fade_table), sizeof(gr_fade_table)) +
				       std::string(reinterpret_cast<const char *>(GameBitmaps), sizeof(GameBitmaps)) +
				       std::string(last_palette_loaded) + "\n" + last_palette_loaded_pig;
			};
			for (const auto &name : { palette, pig }) {
				PHYSFS_file *file = PHYSFSX_openReadBuffered(name.c_str());
				require(file != nullptr, "cannot read graphics identity control");
				xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file)));
				const auto read = PHYSFS_readBytes(file, bytes.data(), bytes.size());
				PHYSFS_close(file);
				require(!bytes.empty() && read == PHYSFS_sint64(bytes.size()), "cannot copy graphics identity control");
				const auto path = resource_shadow / name;
				write_bytes(path.string().c_str(), bytes);
				require(PHYSFS_addToSearchPath(resource_shadow.string().c_str(), 0), "cannot mount graphics control");
				require(xl_level_supported("rtarena.rl2", 0), "identical graphics from different source rejected");
				const auto before_graphics = graphics();
				const auto before_state = arena_state();
				bytes.back() ^= 1;
				write_bytes(path.string().c_str(), bytes);
				require(!xl_level_supported("rtarena.rl2", 0) && std::strstr(xl_level_rejection(), name.c_str()), "changed graphics rejection lacks filename");
				require(load_level("rtarena.rl2") != 0 && std::strstr(xl_level_rejection(), name.c_str()), "loader bypassed graphics identity check");
				require(graphics() == before_graphics && arena_state() == before_state, "graphics rejection mutated active palette, bitmaps or gameplay");
				arena_water_contract();
				require(PHYSFS_removeFromSearchPath(resource_shadow.string().c_str()), "cannot unmount graphics control");
				require(std::filesystem::remove(path), "cannot remove graphics control");
				require(xl_level_supported("rtarena.rl2", 0), "graphics source recovery failed");
				StartNewGame(1);
				arena_water_contract();
			}
		}
		std::cout << "XL graphics sources: identical copies accepted, changed palette/PIG rejected before mutation, native recovery passed\n";
		{
			// Keep the original level bytes, but remove each declared companion in turn
			const std::string archive = PHYSFS_getRealDir("rtarena.rl2");
			std::map<std::string, xl_compat::Bytes> members;
			for (const char *name : { "rtarena.rl2", "rtarena.HXM", "rtarena.POG" }) {
				PHYSFS_file *file = PHYSFSX_openReadBuffered(name);
				require(file != nullptr, "cannot read original resource completeness fixture");
				auto &bytes = members[name];
				bytes.resize(static_cast<std::size_t>(PHYSFS_fileLength(file)));
				const auto count = PHYSFS_read(file, bytes.data(), 1, static_cast<PHYSFS_uint32>(bytes.size()));
				PHYSFS_close(file);
				require(count == PHYSFS_sint64(bytes.size()), "cannot copy original resource completeness fixture");
				write_bytes((resource_shadow / name).string().c_str(), bytes);
			}
			require(PHYSFS_removeFromSearchPath(archive.c_str()), "cannot isolate original resource archive");
			require(PHYSFS_addToSearchPath(resource_shadow.string().c_str(), 0), "cannot mount incomplete package controls");
			const auto before_state = arena_state();
			const auto before_definitions = gameplay_definitions();
			for (const auto &entry : { std::pair<const char *, const char *>{ "rtarena.HXM", "custom robots" }, { "rtarena.POG", "custom textures" } }) {
				const auto path = resource_shadow / entry.first;
				require(std::filesystem::remove(path), "cannot omit declared companion");
				require(!xl_level_supported("rtarena.rl2", 0) && std::strstr(xl_level_rejection(), entry.second),
				        "incomplete mission silently fell back to stock resources");
				require(arena_state() == before_state && gameplay_definitions() == before_definitions,
				        "incomplete-package preflight changed live gameplay");
				write_bytes(path.string().c_str(), members.at(entry.first));
				require(xl_level_supported("rtarena.rl2", 0), "complete mission package did not recover");
			}
			require(PHYSFS_removeFromSearchPath(resource_shadow.string().c_str()), "cannot unmount incomplete package controls");
			require(PHYSFS_addToSearchPath(archive.c_str(), 0), "cannot restore original resource archive");
			for (const auto &entry : members)
				require(std::filesystem::remove(resource_shadow / entry.first), "cannot remove private package control");
		}
		for (const char *extension : { "HXM", "POG" }) {
			const auto path = resource_shadow / (std::string("rtarena.") + extension);
			write_bytes(path.string().c_str(), { 'B', 'A', 'D', '!' });
			require(PHYSFS_addToSearchPath(resource_shadow.string().c_str(), 0), "cannot mount malformed resource control");
			const auto before = arena_state();
			const auto definitions = gameplay_definitions();
			require(!xl_level_supported("rtarena.rl2", 0), "malformed companion passed XL preflight");
			require(std::strstr(xl_level_rejection(), extension), "resource rejection omitted companion format");
			require(arena_state() == before && gameplay_definitions() == definitions && xl_preview_water_segment(187),
			        "resource preflight mutated live gameplay");
			require(PHYSFS_removeFromSearchPath(resource_shadow.string().c_str()), "cannot unmount malformed resource control");
			require(std::filesystem::remove(path), "cannot remove malformed resource control");
			require(xl_level_supported("rtarena.rl2", 0), "original resource preflight did not recover");
		}
		{
			// One indexed, uncompressed 1x1 bitmap; validate pixels without installing it
			xl_compat::Bytes pog(33, 0);
			std::memcpy(pog.data(), "DPOG", 4);
			pog[4] = pog[8] = pog[12] = pog[23] = pog[24] = 1;
			pog[32] = 42;
			const auto path = resource_shadow / "rtarena.POG";
			write_bytes(path.string().c_str(), pog);
			require(PHYSFS_addToSearchPath(resource_shadow.string().c_str(), 0), "cannot mount bitmap boundary control");
			const auto original_bitmap = GameBitmaps[1];
			require(xl_level_supported("rtarena.rl2", 0), "valid bounded bitmap companion rejected");
			pog.pop_back();
			write_bytes(path.string().c_str(), pog);
			require(!xl_level_supported("rtarena.rl2", 0), "truncated bitmap pixels accepted");
			pog.push_back(42);
			pog[12] = pog[13] = 255;
			write_bytes(path.string().c_str(), pog);
			require(!xl_level_supported("rtarena.rl2", 0), "bitmap index outside loaded registry accepted");
			require(!std::memcmp(&original_bitmap, &GameBitmaps[1], sizeof(original_bitmap)), "bitmap preflight installed a replacement");
			require(PHYSFS_removeFromSearchPath(resource_shadow.string().c_str()), "cannot unmount bitmap boundary control");
			require(std::filesystem::remove(path), "cannot remove bitmap boundary control");
		}
		{
			// Structurally valid HXM: one object-bitmap replacement, no other records
			xl_compat::Bytes hxm(34, 0);
			std::memcpy(hxm.data(), "HMX!", 4);
			hxm[4] = hxm[20] = hxm[28] = 1;
			const auto path = resource_shadow / "rtarena.HXM";
			write_bytes(path.string().c_str(), hxm);
			require(PHYSFS_addToSearchPath(resource_shadow.string().c_str(), 0), "cannot mount HXM dependency control");
			const auto definitions = gameplay_definitions();
			const auto original_bitmap = ObjBitmaps[0];
			const auto original_model = arena_model();
			const bool valid = xl_level_supported("rtarena.rl2", 0) != 0;
			require(valid, xl_level_rejection());
			hxm[28] = Num_bitmap_files & 255;
			hxm[29] = Num_bitmap_files >> 8;
			write_bytes(path.string().c_str(), hxm);
			PHYSFS_file *control = PHYSFSX_openReadBuffered("rtarena.HXM");
			require(control && bm_validate_level_hxm(control), "missing-bitmap control did not pass structural validation");
			PHYSFS_close(control);
			require(!xl_level_supported("rtarena.rl2", 0) && std::strstr(xl_level_rejection(), "missing bitmap"),
			        "structurally valid HXM escaped loaded dependency validation");
			require(gameplay_definitions() == definitions && ObjBitmaps[0].index == original_bitmap.index && arena_model() == original_model,
			        "staged HXM validation changed live resources");
			require(PHYSFS_removeFromSearchPath(resource_shadow.string().c_str()), "cannot unmount HXM dependency control");
			require(std::filesystem::remove(path), "cannot remove HXM dependency control");
			require(xl_level_supported("rtarena.rl2", 0), "original HXM dependency validation did not recover");
		}
		{
			const auto path = resource_shadow / "rtarena.HXM";
			const auto original_robot = Robot_info[0];
			auto write_robot = [&](const robot_info &robot) {
				PHYSFS_file *out = PHYSFS_openWrite("resource-shadow/rtarena.HXM");
				require(out != nullptr, "cannot write staged robot control");
				PHYSFS_writeSLE32(out, 0x21584d48);
				PHYSFS_writeSLE32(out, 1);
				PHYSFS_writeSLE32(out, 1);
				PHYSFS_writeSLE32(out, 0);
				require(robot_info_write_n(&robot, 1, out), "cannot serialize staged robot control");
				for (int section = 0; section < 4; ++section) PHYSFS_writeSLE32(out, 0);
				PHYSFS_close(out);
			};
			write_robot(original_robot);
			require(PHYSFS_addToSearchPath(resource_shadow.string().c_str(), 0), "cannot mount staged robot control");
			require(xl_level_supported("rtarena.rl2", 0), "valid staged robot rejected");
			// Simulate a previous level changing a model needed by the next HXM
			// Its preflight must use the owned mission baseline without repairing live state
			auto &live_model = Polygon_models[Player_ship->model_num];
			auto *live_payload = live_model.model_data;
			live_model.model_data = nullptr;
			bool baseline_valid = false;
			try {
				XlPreparedResources resources("rtarena.rl2");
				resources.dependencies().model(Player_ship->model_num);
				baseline_valid = xl_level_supported("rtarena.rl2", 0) != 0;
			} catch (const std::exception &) {
			}
			const bool live_unchanged = live_model.model_data == nullptr;
			live_model.model_data = live_payload;
			require(baseline_valid && live_unchanged, "HXM validation depended on or repaired previous-level model state");
			auto changed = original_robot;
			changed.weapon_type = N_weapon_types;
			{
				auto staged_robot = original_robot;
				staged_robot.lighting ^= 1;
				write_robot(staged_robot);
				XlPreparedResources prepared("rtarena.rl2");
				write_robot(changed); // The on-disk file is now invalid
				prepared.install_hxm();
				const bool installed_staged = Robot_info[0].lighting == staged_robot.lighting &&
				                              Robot_info[0].weapon_type == original_robot.weapon_type;
				Robot_info[0] = original_robot;
				require(installed_staged, "HXM installation reopened the file instead of using prepared definitions");
			}
			write_robot(changed);
			PHYSFS_file *control = PHYSFSX_openReadBuffered("rtarena.HXM");
			require(control && bm_validate_level_hxm(control), "missing-weapon control did not pass structural validation");
			PHYSFS_close(control);
			require(!xl_level_supported("rtarena.rl2", 0) && std::strstr(xl_level_rejection(), "missing weapon"),
			        "staged robot checked live definition instead of its replacement");
			require(!std::memcmp(&Robot_info[0], &original_robot, sizeof(original_robot)), "robot preflight changed live definition");
			require(PHYSFS_removeFromSearchPath(resource_shadow.string().c_str()), "cannot unmount staged robot control");
			require(std::filesystem::remove(path), "cannot remove staged robot control");
		}
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
		arena_fluid_objects();
		std::cout << "Arena physical water contract passed\n";
		d_srand_stream(D_RNG_SIM, 1);
		d_srand_stream(D_RNG_FX, 1);
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		const int spawn_segment = ConsoleObject->segnum;
		require(!xl_preview_water_segment(spawn_segment), "water traversal must start at the authored dry spawn");
		const auto recording_root = std::filesystem::path(PHYSFS_getWriteDir()) / "input_demo_recordings" / "new";
		std::set<std::filesystem::path> prior_recordings;
		if (std::filesystem::exists(recording_root))
			for (const auto &entry : std::filesystem::directory_iterator(recording_root)) prior_recordings.insert(entry.path());
		const int paused = game_is_time_paused();
		require(input_demo_start_quick_recording_common() != 0 && game_is_time_paused() == paused,
		        "cannot record water traversal without changing pause state");
		arena_recording = true;
		const int into = arena_traverse(187);
		arena_recording = false;
		input_demo_stop_quick_recording_common();
		{
			const auto root = std::filesystem::path(PHYSFS_getWriteDir());
			bool exported = false;
			for (const auto &entry : std::filesystem::directory_iterator(root / "input_demo_recordings" / "new")) {
				if (entry.path().extension() != ".dximdemo" || prior_recordings.count(entry.path())) continue;
				std::ifstream file(entry.path());
				std::string line;
				std::getline(file, line);
				const auto header = nlohmann::json::parse(line);
				require(header.at("mission") == "rtarena" && header.at("start_mode") == "new_level" && header.at("level") == 1 && header.at("frame_count") == into,
				        "water traversal recording lacks new-level start or complete inputs");
				std::filesystem::copy_file(entry.path(), root / "xl-water-spawn.dximdemo", std::filesystem::copy_options::overwrite_existing);
				exported = true;
			}
			require(exported, "water traversal recording was not exported");
			require(write_player_file() == 0, "cannot export water traversal player profile");
			std::cout << "Arena spawn input recording: segment=" << spawn_segment << " frames=" << into
			          << "; new-level start and controls exported; cross-platform replay unverified\n";
		}
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
		require(xl_resource_mission_enabled() && *mission_resource_archive(), "water save restore lost XL resource identity");
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
		{
			// One indexed 1x1 bitmap, installed after the source bytes change
			xl_compat::Bytes pog(33, 0);
			std::memcpy(pog.data(), "DPOG", 4);
			pog[4] = pog[8] = pog[12] = pog[23] = pog[24] = 1;
			pog[32] = 42;
			const auto path = resource_shadow / "rtarena.POG";
			write_bytes(path.string().c_str(), pog);
			require(PHYSFS_addToSearchPath(resource_shadow.string().c_str(), 0), "cannot mount prepared POG control");
			XlPreparedResources prepared("rtarena.rl2");
			pog[32] = 99;
			write_bytes(path.string().c_str(), pog);
			for (int repeat = 0; repeat < 2; ++repeat) {
				prepared.install_pog();
				require(GameBitmaps[1].bm_w == 1 && GameBitmaps[1].bm_h == 1 && GameBitmaps[1].bm_data[0] == 42,
				        "POG installation reopened changed source pixels");
			}
			require(PHYSFS_removeFromSearchPath(resource_shadow.string().c_str()), "cannot unmount prepared POG control");
			require(std::filesystem::remove(path), "cannot remove prepared POG control");
		}
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
		{
			// Repack the complete original archive under a different mission/level identity
			std::ifstream archive(PHYSFS_getRealDir("rtarena.rl2"), std::ios::binary);
			xl_compat::Bytes archive_bytes{ std::istreambuf_iterator<char>(archive), std::istreambuf_iterator<char>() };
			auto members = xl_compat::read_hog(archive_bytes);
			xl_compat::Bytes repacked{ 'D', 'H', 'F' };
			for (auto &member : members) {
				if (!d_stricmp(member.name.c_str(), "rtarena.rl2")) member.bytes = changed;
				if (member.name.size() > 7 && !d_strnicmp(member.name.c_str(), "rtarena", 7)) member.name.replace(0, 7, "wateralt");
				const auto header = repacked.size();
				repacked.resize(header + 17, 0);
				require(member.name.size() < 13, "renamed HOG member exceeds native name capacity");
				std::memcpy(repacked.data() + header, member.name.data(), member.name.size());
				for (unsigned byte = 0; byte < 4; ++byte) repacked[header + 13 + byte] = static_cast<unsigned char>(member.bytes.size() >> (byte * 8));
				repacked.insert(repacked.end(), member.bytes.begin(), member.bytes.end());
			}
			const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
			write_bytes((root / "wateralt.hog").string().c_str(), repacked);
			std::ofstream descriptor(root / "wateralt.mn2");
			descriptor << "name = Renamed fluid fixture\nnum_levels = 1\nwateralt.rl2\ncustom_robots = yes\ncustom_textures = yes\n";
			descriptor.close();
			require(load_base_mission("wateralt", error, sizeof(error)), error);
			input_demo_set_skip_level_intro(1);
			StartNewGame(1);
			require(!std::strcmp(Gamesave_current_filename, "wateralt.rl2") && xl_preview_water_segment(187) && arena_model() == custom_model,
			        "detected fluid profile depended on original mission/name/hash");
			require(load_base_mission("rtarena", error, sizeof(error)), error);
			input_demo_set_skip_level_intro(1);
			StartNewGame(1);
			std::cout << "Renamed/repacked/changed-hash water mission admitted by detected features\n";
		}
		const auto inventory = xl_compat::inspect_level(changed);
		const auto objects = std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const xl_compat::Section &section) { return section.name == "objects"; });
		require(objects != inventory.sections.end() && objects->count > 0, "missing object table for water capability control");
		changed.at(objects->offset) = OBJ_ROBOT;
		const auto mutation = std::filesystem::path(find_arg_value(argc, argv, "-userdir")) / "mutation";
		std::filesystem::create_directories(mutation);
		const auto shadow = (mutation / "rtarena.rl2").string();
		write_bytes(shadow.c_str(), changed);
		require(PHYSFS_addToSearchPath(mutation.string().c_str(), 0), "cannot mount private mutation");
		const auto resolved = PHYSFS_getRealDir("rtarena.rl2");
		require(resolved && std::filesystem::equivalent(resolved, mutation), "negative probe did not resolve to changed bytes");
		const int rejected = load_level("rtarena.rl2");
		const std::string reason = xl_level_rejection();
		require(PHYSFS_removeFromSearchPath(mutation.string().c_str()), "cannot unmount private mutation");
		require(std::filesystem::remove(shadow), "cannot remove private changed-source shadow");
		require(rejected != 0 && xl_preview_water_segment(187) && reason.find("water with robots") != std::string::npos,
		        "unqualified fluid capability admitted or not explained");
		require(load_level("rtarena.rl2") == 0 && xl_preview_water_segment(187), "arena admission did not recover");
		std::cout << "Unqualified water/robot capability rejected, active water preserved, original admission recovered\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "Arena test FAILED: " << e.what() << '\n';
		return 1;
	}
}
