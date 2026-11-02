#include "projectile.h"
#include "object_birth.h"
#include "../tests/hog_fixture.h"
extern "C" {
void maybe_kill_weapon(object *, object *);
void collide_weapon_and_controlcen(object *, object *, vms_vector *);
}

static int projectile_test_shot(int parent, int weapon = FUSION_ID)
{
	const int shot = Laser_create_new(&ConsoleObject->orient.fvec, &ConsoleObject->pos, ConsoleObject->segnum, parent, weapon, 0);
	require(shot >= 0, "Cannot create projectile through production firing");
	Objects[shot].ctype.laser_info.multiplier = F1_0;
	Objects[shot].shields = i2f(40);
	return shot;
}

static std::vector<xl_projectile_state> projectile_capture()
{
	std::vector<xl_projectile_state> result;
	for (int owner = 0; owner <= Highest_object_index; ++owner) {
		xl_projectile_state state{};
		if (xl_projectile_state_get(owner, &state)) result.push_back(state);
	}
	return result;
}

static bool projectile_same(const std::vector<xl_projectile_state> &a, const std::vector<xl_projectile_state> &b)
{
	if (a.size() != b.size()) return false;
	for (std::size_t i = 0; i < a.size(); ++i)
		if (a[i].owner != b[i].owner || a[i].signature != b[i].signature || a[i].count != b[i].count ||
		    !std::equal(a[i].victims, a[i].victims + a[i].count, b[i].victims)) return false;
	return true;
}

static void projectile_impacts(const std::filesystem::path &root)
{
	const int shot = projectile_test_shot(ConsoleObject - Objects);
	const int other = projectile_test_shot(ConsoleObject - Objects);
	const auto saved_other = Objects[other];
	std::ofstream samples(root / "projectile-impacts.txt");
	unsigned count = 0;
	for (int id : { LASER_ID_L1, FUSION_ID, PROXIMITY_ID, SUPERPROX_ID, PMINE_ID, ROBOT_SUPERPROX_ID })
		for (int persistent : { 0, 1 })
			for (int shield : { 0, 1, 65536, 10485760 })
				for (int type : { OBJ_ROBOT, OBJ_PLAYER, OBJ_WEAPON, OBJ_CNTRLCEN })
					for (int other_persistent : { 0, 1 })
						for (int other_shield : { -65537, 0, 1, 262147, 10485760 }) {
							auto &weapon = Objects[shot];
							auto &target = Objects[other];
							weapon.id = id;
							weapon.shields = shield;
							weapon.flags = 0;
							weapon.mtype.phys_info.flags = persistent ? PF_PERSISTENT : 0;
							target.type = static_cast<object_type_t>(type);
							target.shields = other_shield;
							target.mtype.phys_info.flags = other_persistent ? PF_PERSISTENT : 0;
							maybe_kill_weapon(&weapon, &target);
							samples << id << ' ' << persistent << ' ' << shield << ' ' << type << ' ' << other_persistent << ' ' << other_shield << ' ' << weapon.shields << ' ' << bool(weapon.flags & OF_SHOULD_BE_DEAD) << '\n';
							++count;
						}
	Objects[other] = saved_other;
	obj_delete(other);
	obj_delete(shot);
	std::cout << count << " actual projectile impact samples exported\n";
}

static int projectile_robot()
{
	const int victim = obj_create(OBJ_ROBOT, 0, ConsoleObject->segnum, &ConsoleObject->pos, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_POLYOBJ);
	require(victim >= 0, "Cannot allocate combat target");
	auto &robot = Objects[victim];
	robot.rtype.pobj_info.model_num = Robot_info[0].model_num;
	robot.mtype.phys_info.mass = Robot_info[0].mass;
	robot.mtype.phys_info.drag = Robot_info[0].drag;
	robot.shields = i2f(100);
	init_ai_object(victim, AIB_NORMAL, -1);
	return victim;
}

#include "camera_weapon_test.h"

static void projectile_static_robot_controls(bool xl)
{
	for (int behavior : { AIB_STILL, AIB_NORMAL, 135 }) {
		const int owner = projectile_robot();
		auto &robot = Objects[owner];
		init_ai_object(owner, behavior, -1);
		const bool stationary = xl && behavior == 135;
		require(robot.control_type == (stationary ? CT_NONE : CT_AI) && robot.movement_type == (stationary ? MT_NONE : MT_PHYSICS),
		        "Static robot initialization changed ordinary or stock controls");
		const int shot = projectile_test_shot(ConsoleObject - Objects);
		collide_robot_and_weapon(&robot, &Objects[shot], &robot.pos);
		require(robot.shields == 60 * F1_0, "Static robot lost ordinary projectile damage");
		obj_delete(shot);
		obj_delete(owner);
	}
	std::cout << "Static robot controls and ordinary damage passed; XL=" << xl << "\n";
}

static void projectile_combat(bool xl)
{
	const int victim = projectile_robot();
	const int shot = projectile_test_shot(ConsoleObject - Objects);
	auto &weapon = Objects[shot];
	auto &robot = Objects[victim];
	require(weapon.mtype.phys_info.flags & PF_PERSISTENT, "Real fusion shot is not persistent");
	collide_robot_and_weapon(&robot, &weapon, &robot.pos);
	require(robot.shields == i2f(60) && weapon.shields == (xl ? i2f(25) : i2f(40)), "Robot hit damage or projectile attrition differs");
	const auto rng = d_rand_get_stream_call_count(D_RNG_SIM);
	const int count = num_objects;
	collide_robot_and_weapon(&robot, &weapon, &robot.pos);
	require(robot.shields == i2f(60) && weapon.shields == (xl ? i2f(25) : i2f(40)) && num_objects == count && d_rand_get_stream_call_count(D_RNG_SIM) == rng, "Repeat robot hit changed damage, effects or simulation RNG");
	require(xl ? xl_projectile_has_hit(shot, victim) && !weapon.ctype.laser_info.hitobj_list[victim] : weapon.ctype.laser_info.hitobj_list[victim] && !xl_projectile_has_hit(shot, victim), "XL/Redux hit state dispatch crossed profiles");
	// Source retains victim slot identity through reuse, but never projectile ownership
	const int signature = robot.signature;
	obj_delete(victim);
	const int reused = projectile_robot();
	require(reused == victim && Objects[reused].signature != signature, "Victim slot reuse control did not reuse its slot");
	collide_robot_and_weapon(&Objects[reused], &weapon, &Objects[reused].pos);
	require(Objects[reused].shields == i2f(100), "Victim slot reuse changed reference duplicate-hit identity");
	obj_delete(shot);
	const int replacement = projectile_test_shot(ConsoleObject - Objects);
	require(replacement == shot && !xl_projectile_has_hit(replacement, victim), "New projectile inherited the old owner's history");
	collide_robot_and_weapon(&Objects[reused], &Objects[replacement], &Objects[reused].pos);
	require(Objects[reused].shields == i2f(60), "New projectile could not hit reused victim");
	obj_delete(replacement);
	const int enemy = projectile_test_shot(reused);
	auto &incoming = Objects[enemy];
	Players[Player_num].shields = ConsoleObject->shields = i2f(100);
	Players[Player_num].flags &= ~(PLAYER_FLAGS_INVULNERABLE | PLAYER_FLAGS_CLOAKED);
	Player_is_dead = 0;
	collide_player_and_weapon(ConsoleObject, &incoming, &ConsoleObject->pos);
	require(Players[Player_num].shields == i2f(60) && incoming.shields == (xl ? i2f(15) : i2f(40)), "Player impact damage or pre-damage attrition differs");
	collide_player_and_weapon(ConsoleObject, &incoming, &ConsoleObject->pos);
	require(Players[Player_num].shields == i2f(60), "Repeat player hit applied damage");
	obj_delete(enemy);
	obj_delete(reused);
	int reactor = -1;
	for (int i = 0; i <= Highest_object_index; ++i)
		if (Objects[i].type == OBJ_CNTRLCEN) {
			reactor = i;
			break;
		}
	require(reactor >= 0, "Missing authored reactor control");
	const int reactor_shot = projectile_test_shot(ConsoleObject - Objects);
	Objects[reactor_shot].shields = i2f(10);
	Objects[reactor].shields = i2f(40);
	collide_weapon_and_controlcen(&Objects[reactor_shot], &Objects[reactor], &Objects[reactor].pos);
	require(Objects[reactor].shields == i2f(xl ? 30 : 20) && Objects[reactor_shot].shields == (xl ? i2f(10) / 4 : i2f(10)), "Reactor acquired wrong damage scaling or attrition");
	collide_weapon_and_controlcen(&Objects[reactor_shot], &Objects[reactor], &Objects[reactor].pos);
	require(Objects[reactor].shields == (xl ? i2f(30) - i2f(10) / 4 : i2f(20)), "Reactor repeat dispatch differs");
	require(!xl_projectile_has_hit(reactor_shot, reactor), "Reactor incorrectly acquired XL hit suppression");
	obj_delete(reactor_shot);
	std::cout << (xl ? "XL" : "Redux") << " actual robot/player/reactor damage, duplicate and owner/victim reuse controls passed\n";
}

static void projectile_physics()
{
	int room = -1;
	vms_vector center{};
	for (int i = 0; i < Num_segments; ++i) {
		compute_segment_center(&center, &Segments[i]);
		if (Segments[i].objects < 0 && !get_seg_masks(&center, i, i2f(10), __FILE__, __LINE__).facemask) {
			room = i;
			break;
		}
	}
	require(room >= 0, "Cannot find empty room for controlled projectile physics");
	const int a = projectile_robot(), b = projectile_robot();
	for (int target : { a, b }) {
		obj_relink(target, room);
		Objects[target].pos = center;
		Objects[target].pos.z += target == a ? 0 : i2f(5);
		Objects[target].last_pos = Objects[target].pos;
		Objects[target].mtype.phys_info.velocity = {};
	}
	auto position = center;
	position.z -= i2f(5);
	vms_vector direction{ 0, 0, F1_0 };
	const int shot = Laser_create_new(&direction, &position, room, ConsoleObject - Objects, FUSION_ID, 0);
	require(shot >= 0, "Cannot fire controlled physical fusion shot");
	Objects[shot].shields = i2f(40);
	Objects[shot].ctype.laser_info.multiplier = F1_0;
	FrameTime = F1_0 / 120;
	for (int frame = 0; frame < 120 && Objects[b].shields == i2f(100) && !(Objects[shot].flags & OF_SHOULD_BE_DEAD); ++frame) {
		GameTime64 += FrameTime;
		do_physics_sim(&Objects[shot]);
	}
	xl_projectile_state state{};
	require(Objects[a].shields == i2f(60) && Objects[b].shields == i2f(75) && Objects[shot].shields == i2f(25) / 4 &&
	            xl_projectile_state_get(shot, &state) && state.count == 2 && state.victims[0] == a && state.victims[1] == b,
	        "Physical projectile did not pierce both targets with sequential XL damage/attrition");
	obj_delete(shot);
	obj_delete(b);
	obj_delete(a);
	std::cout << "Production projectile physics pierced two positioned robots with sequential XL damage and hit order\n";
}

static void projectile_corrupt_controls(const std::filesystem::path &root)
{
	xl_camera_flash_blast(16 * F1_0, 4, false);
	const auto flash_before = xl_camera_flash_snapshot();
	auto *file = PHYSFS_openWrite("projectile-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Cannot write projectile trailer");
	const auto user = root / "preview-user";
	std::ifstream input(user / "projectile-state.bin", std::ios::binary);
	const xl_compat::Bytes encoded{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	const auto before = projectile_capture();
	require(before.size() >= 2, "Need multiple projectile owners for malformed-state controls");
	const auto hits_before = xl_camera_hit_snapshot();
	std::vector<xl_compat::CameraHitState> live_hits(hits_before.size());
	std::vector<bool> had_hits;
	for (std::size_t i = 0; i < hits_before.size(); ++i) had_hits.push_back(xl_camera_hit_state_get(hits_before[i].owner, live_hits[i]));
	for (int variant = 0; variant < 34; ++variant) {
		auto block = nlohmann::json::parse(encoded.begin() + 16, encoded.end());
		auto &rows = block["projectiles"];
		if (variant == 0) rows[0][0] = MAX_OBJECTS;
		if (variant == 1) rows[0][1] = before[0].signature + 1;
		if (variant == 2) rows[0][2] = nlohmann::json::array();
		if (variant == 3) rows[0][2].push_back(0);
		if (variant == 4) rows[0][2][0] = -1;
		if (variant == 5) rows[0][2][0] = MAX_OBJECTS;
		if (variant == 6) rows[0][2][0] = rows[0][2][1];
		if (variant == 7) rows[1] = rows[0];
		if (variant == 8) rows.erase(rows.begin());
		if (variant == 9) rows[0][0] = ConsoleObject - Objects;
		if (variant == 10) rows[0][1] = UINT64_MAX;
		if (variant == 11) rows[0][2][0] = nlohmann::json::boolean_t(1);
		if (variant == 12) rows[0][2][19] = MAX_OBJECTS - 1;
		if (variant == 13) block.erase("projectiles");
		auto &hit_rows = block["camera_hits"];
		if (variant == 14) block.erase("camera_hits");
		if (variant == 15) hit_rows[0][0] = MAX_OBJECTS;
		if (variant == 16) hit_rows[0][1] = hits_before[0].signature + 1;
		if (variant == 17) hit_rows[0][2] = OBJ_ROBOT;
		if (variant == 18) hit_rows[0][3] = INT32_MIN;
		if (variant == 19) hit_rows[0][3] = UINT64_MAX;
		if (variant == 20) hit_rows[0][3] = nlohmann::json::boolean_t(1);
		if (variant == 21) hit_rows.erase(hit_rows.begin());
		if (variant == 22) hit_rows.push_back(hit_rows[0]);
		if (variant == 23) hit_rows[0].push_back(0);
		if (variant == 24) hit_rows = nullptr;
		if (variant == 25) block.erase("camera_flash");
		if (variant == 26) block["camera_flash"] = 1;
		if (variant == 27) block["camera_flash"][0] = -1;
		if (variant == 28) block["camera_flash"][0] = 4 * F1_0 + 1;
		if (variant == 29) block["camera_flash"][1] = UINT64_MAX;
		if (variant == 30) block["camera_flash"][2][0] = -1;
		if (variant == 31) block["camera_flash"][2][0] = 65;
		if (variant == 32) block["camera_flash"][2][0] = nlohmann::json::boolean_t(1);
		if (variant == 33) block["camera_flash"].push_back(0);
		const auto text = block.dump();
		xl_compat::Bytes bad(encoded.begin(), encoded.begin() + 16);
		bad.insert(bad.end(), text.begin(), text.end());
		std::uint32_t sum = 2166136261u;
		for (unsigned char byte : text) sum = (sum ^ byte) * 16777619u;
		for (unsigned byte = 0; byte < 4; ++byte) {
			bad[8 + byte] = uint8_t(text.size() >> (8 * byte));
			bad[12 + byte] = uint8_t(sum >> (8 * byte));
		}
		write_bytes((user / "projectile-bad.bin").string().c_str(), bad);
		file = PHYSFS_openRead("projectile-bad.bin");
		const bool rejected = file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && projectile_same(before, projectile_capture());
		if (!rejected) std::cerr << "Malformed projectile variant=" << variant << "\n";
		require(rejected, "Malformed projectile trailer was accepted or changed live history");
		require(xl_camera_flash_snapshot() == flash_before, "Malformed trailer published flash state");
		for (std::size_t i = 0; i < hits_before.size(); ++i) {
			xl_compat::CameraHitState current;
			require(xl_camera_hit_state_get(hits_before[i].owner, current) == had_hits[i] &&
			            current.repaired == live_hits[i].repaired && current.next == live_hits[i].next &&
			            current.critical_time == live_hits[i].critical_time && current.shield_time == live_hits[i].shield_time &&
			            current.points == live_hits[i].points && current.times == live_hits[i].times,
			        "Malformed trailer published camera hit state");
		}
	}
	xl_projectile_clear();
	file = PHYSFS_openRead("projectile-state.bin");
	require(file && xl_wall_trigger_read(file) && PHYSFS_close(file) && projectile_same(before, projectile_capture()), "Valid projectile trailer did not recover");
	std::cout << "Thirty-four malformed projectile/camera hit/flash trailers rejected before table publication; valid recovery passed\n";
}

static void camera_hit_disk_controls()
{
	const int owner = int(ConsoleObject - Objects);
	xl_compat::CameraHitState earned;
	require(xl_camera_hit_state_get(owner, earned) && earned.repaired > 0, "Full camera hit save requires earned physical state");
	SDL_Delay(2);
	xl_camera_controller_begin_frame();
	const auto saved_ticks = int32_t(xl_camera_controller_ticks());
	const int32_t elapsed = earned.repaired_elapsed(saved_ticks);
	const auto saved_hits = xl_camera_hit_snapshot();
	const auto saved_flash = xl_camera_flash_snapshot();
	const auto saved_children = camera_children_capture();
	require(saved_children.size() == 6, "Camera save requires six smart child weapons");
	for (const auto &child : saved_children) require(xl_object_birth_order(child[0]) > 0, "New smart child lacks source append order");
	require(!saved_flash.is_null(), "Full camera save requires impact-earned flash state");
	unsigned robots = 0;
	for (const auto &entry : saved_hits) {
		xl_compat::CameraHitState robot_hit;
		if (entry.type == OBJ_ROBOT && xl_camera_hit_state_get(entry.owner, robot_hit) && robot_hit.repaired > 0) ++robots;
	}
	require(robots > 0, "Full camera disk save requires a physics-earned robot hit");
	require(elapsed > 0, "Camera hit disk save did not exercise elapsed repair time");
	char save[] = "camera-hits.sav", description[] = "XL camera hit state", error[256]{};
	stop_time();
	require(state_save_all_sub(save, description), "Cannot save earned camera hit state");
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active() &&
	            xl_camera_hit_snapshot().empty() && !xl_camera_hit_state_get(owner, earned) && xl_camera_flash_snapshot().is_null() && !xl_camera_flash_frame(),
	        "Camera hit state leaked into stock");
	SDL_Delay(2);
	xl_camera_controller_begin_frame();
	const auto restored_ticks = int32_t(xl_camera_controller_ticks());
	require(restored_ticks > saved_ticks, "Camera hit disk restore did not exercise clock rebasing");
	require(state_restore_all_sub(save, 0), "Cannot restore camera hit save through stock");
	require(xl_camera_flash_snapshot() == saved_flash, "Full disk restore lost impact-earned flash delay/color");
	require(camera_children_capture() == saved_children, "Full disk restore lost smart child ownership, target or motion");
	for (const auto &child : saved_children) require(!xl_object_birth_order(child[0]), "Restored smart child retained omitted source creation order");
	xl_compat::CameraHitState restored;
	require(xl_camera_hit_state_get(owner, restored) && restored.repaired == int64_t(restored_ticks) - elapsed &&
	            !restored.next && !restored.shield_time && !restored.critical_time &&
	            restored.points == decltype(restored.points){} && restored.times == decltype(restored.times){},
	        "Full camera hit restore lost elapsed repair time or retained omitted hit history");
	const auto restored_hits = xl_camera_hit_snapshot();
	require(restored_hits.size() == saved_hits.size(), "Camera disk restore changed tracked actor count");
	for (std::size_t i = 0; i < saved_hits.size(); ++i) {
		const auto &before = saved_hits[i], &after = restored_hits[i];
		require(before.owner == after.owner && before.signature == after.signature && before.type == after.type && before.elapsed == after.elapsed,
		        "Camera disk restore changed an actor identity or elapsed repair time");
	}
	require(!game_is_time_paused(), "Camera hit disk restore left simulation paused");
	std::cout << "Physics-earned player/robot camera hit state disk restore through stock passed\n";
}

static int projectile_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		require(load_base_mission("neptunxl", error, sizeof(error)), error);
		const bool vertigo = std::filesystem::exists(root / "data/d2x.ham");
		if (vertigo) require(N_robot_types == 78 && Robot_info[68].model_num == 168 && Robot_info[76].model_num >= 0,
			                 "Original Vertigo extension did not populate its authored robot slots");
		if (vertigo) {
			// Android separately mounts the selected HOG's parent after descriptor lookup
			const auto parent = root / "missions";
			const auto sibling = parent / "d2x.ham";
			require(!std::filesystem::exists(sibling), "Vertigo sibling fixture is already occupied");
			std::filesystem::copy_file(root / "data/d2x.ham", sibling);
			require(PHYSFS_addToSearchPath(parent.string().c_str(), 0), "Cannot mount selected mission parent control");
			xl_resource_baseline_clear();
			require(xl_resource_load_vertigo() == 1 && N_robot_types == 78, "Selected mission parent lost original Vertigo ownership");
			xl_resource_baseline_capture();
			require(PHYSFS_removeFromSearchPath(parent.string().c_str()), "Cannot unmount selected mission parent control");
			std::filesystem::remove(sibling);
			const auto loose = root / "data/d2x.ham", hidden = root / "data/vertigo-control.ham", archive = root / "data/d2x.hog";
			require(!std::filesystem::exists(hidden) && !std::filesystem::exists(archive), "Vertigo archive fixture is already occupied");
			std::ifstream input(loose, std::ios::binary);
			xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() }, packed{ 'D', 'H', 'F' };
			input.close();
			xl_test::hog_member(packed, "d2x.ham", original, false);
			write_bytes(archive.string().c_str(), packed);
			std::filesystem::rename(loose, hidden);
			xl_resource_baseline_clear();
			require(xl_resource_load_vertigo() == 1 && N_robot_types == 78 && !PHYSFS_getMountPoint("__xl_vertigo_resources__.hog"),
			        "Original Vertigo private archive load failed or retained its mount");
			xl_resource_baseline_capture();
			std::filesystem::rename(hidden, loose);
			std::filesystem::remove(archive);
		}
		const auto before_extension = gameplay_definitions();
		const auto bad_extension = root / "d2x.ham";
		require(!std::filesystem::exists(bad_extension), "Vertigo rejection fixture path is already occupied");
		write_bytes(bad_extension.string().c_str(), xl_compat::Bytes{ 'X', 'H', 'A', 'M', 1, 0, 0, 0 });
		require(xl_resource_load_vertigo() == -1 && std::strstr(xl_resource_base_rejection(), "original Vertigo") &&
		            gameplay_definitions() == before_extension && !PHYSFS_getMountPoint("__xl_vertigo_resources__.hog"),
		        "Unqualified Vertigo extension changed definitions or escaped preflight");
		std::filesystem::remove(bad_extension);
		require(load_base_mission("neptunxl", error, sizeof(error)), error);
		require(!*xl_resource_base_rejection() && gameplay_definitions() == before_extension, "Vertigo rejection did not recover on mission reload");
		std::cout << "XL Vertigo extension rejection and mission reload passed; original=" << vertigo << "\n";
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		Game_suspended = SUSP_ROBOTS;
		camera_weapon_controls(root);
		projectile_static_robot_controls(true);
		camera_hit_disk_controls();
		StartNewGame(1);
		Game_suspended = SUSP_ROBOTS;
		projectile_impacts(root);
		projectile_combat(true);
		projectile_physics();
		StartNewGame(1);
		Game_suspended = SUSP_ROBOTS;
		GameTime64 = 7 * F1_0;
		const int first = projectile_test_shot(ConsoleObject - Objects);
		const int second = projectile_test_shot(ConsoleObject - Objects);
		std::vector<int> victims;
		for (int i = 0; i < 22; ++i) {
			const int victim = projectile_robot();
			victims.push_back(victim);
			if (!i) init_ai_object(victim, 135, -1);
			Objects[first].shields = i2f(40);
			collide_robot_and_weapon(&Objects[victim], &Objects[first], &Objects[victim].pos);
			require(Objects[victim].shields == i2f(60), "Distinct victim did not take fusion damage");
		}
		xl_projectile_state state{};
		require(xl_projectile_state_get(first, &state) && state.count == 20 && std::equal(state.victims, state.victims + 20, victims.begin() + 2), "Full projectile table did not evict oldest victims");
		const int retained = victims.back();
		collide_robot_and_weapon(&Objects[retained], &Objects[first], &Objects[retained].pos);
		require(Objects[retained].shields == i2f(60), "Full history forgot newest victim");
		const int evicted = victims.front();
		Objects[first].shields = i2f(40);
		collide_robot_and_weapon(&Objects[evicted], &Objects[first], &Objects[evicted].pos);
		require(Objects[evicted].shields == i2f(20), "Evicted victim was incorrectly suppressed");
		collide_robot_and_weapon(&Objects[retained], &Objects[second], &Objects[retained].pos);
		require(Objects[retained].shields == i2f(20), "Separate projectile inherited another owner's hit history");
		require(xl_object_birth_time(first) == 7 * F1_0 && xl_object_birth_time(retained) == 7 * F1_0, "Dynamic object births did not use creation time");
		auto *saved_effect = object_create_explosion(ConsoleObject->segnum, &ConsoleObject->pos, 65536, VCLIP_SMALL_EXPLOSION);
		require(saved_effect != nullptr, "Cannot create saved explosion motion control");
		xl_explosion_motion_begin(saved_effect);
		const int effect_slot = int(saved_effect - Objects);
		saved_effect->lifeleft /= 2;
		do_explosion_sequence(saved_effect);
		const auto effect_position = saved_effect->pos;
		const auto saved = projectile_capture();
		char save[] = "projectiles.sav", description[] = "XL projectile history";
		stop_time();
		require(state_save_all_sub(save, description), "Cannot save earned projectile histories");
		projectile_corrupt_controls(root);
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active() && projectile_capture().empty(), "Projectile state leaked into stock");
		projectile_combat(false);
		projectile_static_robot_controls(false);
		require(state_restore_all_sub(save, 0) && projectile_same(saved, projectile_capture()), "Disk through stock lost full projectile histories");
		require(Objects[evicted].control_type == CT_NONE && Objects[evicted].movement_type == MT_NONE,
		        "Full save through stock reactivated a static robot");
		require(xl_object_birth_time(first) == 0 && xl_object_birth_time(retained) == 0, "Disk restore did not reset private creation times like source level reload");
		auto *saved_viewer = Viewer;
		auto restore_viewer = *ConsoleObject;
		restore_viewer.pos.x += 10 * F1_0;
		Viewer = &restore_viewer;
		do_explosion_sequence(&Objects[effect_slot]);
		Viewer = saved_viewer;
		require(Objects[effect_slot].type == OBJ_FIREBALL && !std::memcmp(&effect_position, &Objects[effect_slot].pos, sizeof(effect_position)),
		        "Restored explosion inherited unsaved source motion state");
		std::cout << "XL object birth and explosion motion disk reset passed\n";
		require(!game_is_time_paused(), "Native save/restore left the simulation paused");
		const auto shields = Objects[retained].shields;
		collide_robot_and_weapon(&Objects[retained], &Objects[first], &Objects[retained].pos);
		require(Objects[retained].shields == shields, "Restored projectile forgot an earlier victim");
		std::cout << "XL projectile native controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL projectile controls failed: " << error.what() << '\n';
		return 1;
	}
}
