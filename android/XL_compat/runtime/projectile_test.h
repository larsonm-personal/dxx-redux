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
#include "static_contact_test.h"

static void projectile_sound_dependency_controls()
{
	int sample = -1;
	for (int id = 0; id < MAX_SOUNDS && sample < 0; ++id) {
		const int resolved = digi_xlat_sound(id);
		if (resolved >= 0 && resolved < MAX_SOUND_FILES && GameSounds[resolved].data && GameSounds[resolved].length > 0) sample = resolved;
	}
	require(sample >= 0, "Sound dependency controls require a loaded original sample");
	struct Restore {
		int sample, low_memory = GameArg.SysLowMem;
		ubyte sound = Sounds[0], alternate = AltSounds[0];
		ubyte *data;
		~Restore()
		{
			Sounds[0] = sound;
			AltSounds[0] = alternate;
			GameArg.SysLowMem = low_memory;
			GameSounds[sample].data = data;
		}
	} restore{ sample, GameArg.SysLowMem, Sounds[0], AltSounds[0], GameSounds[sample].data };
	const auto rejects = [](int id) {
		try {
			xl_assets::ObjectDependencies graph;
			graph.sound(id);
			return false;
		} catch (const std::exception &) {
			return true;
		}
	};
	for (int low_memory : { 0, 1 }) {
		GameArg.SysLowMem = low_memory;
		AltSounds[0] = 0;
		Sounds[0] = 255;
		xl_assets::ObjectDependencies silent;
		silent.sound(0);
		require(silent.sounds.empty() && digi_xlat_sound(0) == -1, "Original silent mapping acquired a sample dependency");
	}
	AltSounds[0] = 255;
	xl_assets::ObjectDependencies alternate_silent;
	alternate_silent.sound(0);
	require(alternate_silent.sounds.empty(), "Silent alternate mapping acquired a sample dependency");
	GameArg.SysLowMem = 0;
	Sounds[0] = static_cast<ubyte>(sample);
	xl_assets::ObjectDependencies mapped;
	mapped.sound(0);
	require(mapped.sounds.count(sample) == 1, "Mapped sound lost its original sample dependency");
	GameSounds[sample].data = nullptr;
	require(rejects(0), "Mapped sound with a missing sample was admitted");
	GameSounds[sample].data = restore.data;
	Sounds[0] = MAX_SOUND_FILES;
	require(rejects(0) && rejects(MAX_SOUNDS) && rejects(-2), "Invalid sound mapping or logical ID was admitted");
	std::cout << "Original silent sound mappings and missing-sample rejection passed\n";
}

static void projectile_hostile_robot_contact(bool xl)
{
	int room = -1, guidebot = -1;
	vms_vector center{};
	for (int i = 0; i < Num_segments; ++i) {
		compute_segment_center(&center, &Segments[i]);
		if (Segments[i].objects < 0 && !get_seg_masks(&center, i, i2f(10), __FILE__, __LINE__).facemask) {
			room = i;
			break;
		}
	}
	for (int id = 0; id < N_robot_types; ++id)
		if (Robot_info[id].companion) {
			guidebot = id;
			break;
		}
	require(room >= 0 && guidebot >= 0, "hostile contact controls require an empty room and guidebot");
	const int victim = projectile_robot(), attacker = projectile_robot(), nearby = projectile_robot();
	for (const int actor : { victim, attacker, nearby }) {
		obj_relink(actor, room);
		Objects[actor].pos = center;
	}
	Objects[attacker].pos.z += i2f(10);
	Objects[nearby].pos.z += F1_0;
	GameTime64 += F1_0;
	const int saved_mode = Game_mode, saved_cheat = cheats.robotskillrobots;
	for (const int profile : { 0, 1, 2, 3, 4 })
		for (const bool persistent : { false, true })
			for (const int id : { FUSION_ID, CONCUSSION_ID }) {
				Game_mode = profile == 4 ? GM_MULTI : 0;
				cheats.robotskillrobots = profile == 3;
				Objects[attacker].id = profile == 1 ? guidebot : 1;
				Objects[victim].shields = Objects[nearby].shields = i2f(1000);
				Objects[victim].mtype.phys_info.velocity = Objects[nearby].mtype.phys_info.velocity = {};
				const int parent = profile == 2 ? int(ConsoleObject - Objects) : attacker;
				const int shot = projectile_test_shot(parent, id);
				Objects[shot].pos = Objects[victim].pos;
				obj_relink(shot, room);
				Objects[shot].mtype.phys_info.flags = persistent ? PF_PERSISTENT : 0;
				const auto before = Objects[shot];
				const auto victim_physics = Objects[victim].mtype.phys_info, nearby_physics = Objects[nearby].mtype.phys_info;
				const int before_count = num_objects;
				std::array<int, MAX_OBJECTS> signatures{};
				const int prior_highest = Highest_object_index;
				for (int i = 0; i <= prior_highest; ++i) signatures[i] = Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature;
				d_srand(1234);
				unsigned prior_rng = 0;
				d_rand_get_state(&prior_rng);
				const auto calls = d_rand_get_call_count();
				collide_robot_and_weapon(&Objects[victim], &Objects[shot], &Objects[victim].pos);
				const bool rejected = xl && profile != 2 && profile != 3 && profile != 4;
				if (rejected) {
					unsigned after_rng = 0;
					d_rand_get_state(&after_rng);
					require(Objects[victim].shields == i2f(1000) && Objects[nearby].shields == i2f(1000) &&
					            !std::memcmp(&Objects[nearby].mtype.phys_info, &nearby_physics, sizeof(physics_info)) &&
					            !std::memcmp(&Objects[victim].mtype.phys_info, &victim_physics, sizeof(physics_info)),
					        "rejected enemy contact applied direct damage or nearby splash force");
					require(!std::memcmp(&Objects[shot], &before, sizeof(object)) &&
					            !xl_projectile_has_hit(shot, victim) && Objects[shot].ctype.laser_info.last_hitobj == before.ctype.laser_info.last_hitobj,
					        "rejected enemy contact recorded history or consumed its projectile");
					require(num_objects == before_count && after_rng == prior_rng && d_rand_get_call_count() == calls,
					        "rejected enemy contact created effects or consumed simulation randomness");
					collide_robot_and_weapon(&Objects[victim], &Objects[shot], &Objects[victim].pos);
					require(num_objects == before_count && Objects[shot].flags == before.flags && !xl_projectile_has_hit(shot, victim),
					        "repeat rejected enemy contact changed projectile state");
				} else {
					if (profile == 2 || profile == 3) require(Objects[victim].shields < i2f(1000), "allowed player/cheat control lost shield damage");
					const bool xl_history = xl && profile != 4;
					require(persistent ? (xl_history ? xl_projectile_has_hit(shot, victim) : Objects[shot].ctype.laser_info.hitobj_list[victim] != 0)
					                   : (Objects[shot].flags & OF_SHOULD_BE_DEAD) != 0,
					        "allowed player/classic/multiplayer/cheat contact lost history or disposition");
					if (id == CONCUSSION_ID)
						require(Objects[nearby].mtype.phys_info.velocity.z != 0, "allowed splash control did not reach nearby actor");
				}
				for (int i = Highest_object_index; i >= 0; --i)
					if (Objects[i].type != OBJ_NONE && (i > prior_highest || signatures[i] != Objects[i].signature)) obj_delete(i);
				obj_delete(shot);
			}
	Game_mode = saved_mode;
	cheats.robotskillrobots = saved_cheat;
	obj_delete(nearby);
	obj_delete(attacker);
	obj_delete(victim);
	std::cout << "Hostile ordinary contact early return, repeat, splash and disposition controls passed; XL=" << xl << "\n";
}

static void projectile_ordinary_gauss_spin(bool xl)
{
	ScopedNetworkTest network;
	// Test torque independently of splash, which has its own live blast controls
	struct RestoreRadius {
		fix radius = Weapon_info[GAUSS_ID].damage_radius;
		~RestoreRadius()
		{
			Weapon_info[GAUSS_ID].damage_radius = radius;
		}
	} restore_radius;
	Weapon_info[GAUSS_ID].damage_radius = 0;
	const int victim = projectile_robot();
	GameTime64 += F1_0;
	const int saved_mode = Game_mode;
	const fix saved_frame = FrameTime;
	struct Case {
		fix frame;
		int skip;
		unsigned seed;
	};
	for (const int multiplayer : { 0, 1 })
		for (const auto test : { Case{ 512, 0, 1 }, Case{ 512, 126, 17 },
		                         Case{ 1024, 63, 17 }, Case{ 1024, 64, 17 }, Case{ F1_0, 0, 1234 }, Case{ F1_0, 1, 1 } }) {
			const auto frame = test.frame;
			const auto skip = test.skip;
			const auto seed = test.seed;
			const int prior_highest = Highest_object_index;
			std::array<int, MAX_OBJECTS> signatures{};
			for (int i = 0; i <= prior_highest; ++i) signatures[i] = Objects[i].signature;
			Game_mode = multiplayer ? GM_MULTI : 0;
			FrameTime = frame;
			const int shot = projectile_test_shot(ConsoleObject - Objects, GAUSS_ID);
			Objects[shot].flags |= OF_HARMLESS;
			auto &robot = Objects[victim];
			robot.ctype.ai_info.SKIP_AI_COUNT = skip;
			robot.mtype.phys_info.rotthrust = { 71, -51, 91 };
			robot.mtype.phys_info.flags = 3;
			Num_awareness_events = 0;
			d_srand(seed);
			if (!multiplayer) d_rand(); // Ordinary contact awareness agitation
			int expected_skip = skip;
			std::array<std::int32_t, 3> expected{ 71, -51, 91 };
			const bool spin = skip * frame < F1_0;
			if (spin) {
				++expected_skip;
				for (auto &axis : expected) {
					const int draw = d_rand();
					axis = fixmul(xl && !multiplayer ? 16383 - draw : draw - 16384, frame * expected_skip);
				}
			}
			unsigned expected_rng = 0;
			d_rand_get_state(&expected_rng);
			d_srand(seed);
			const auto calls = d_rand_get_call_count();
			collide_robot_and_weapon(&robot, &Objects[shot], &robot.pos);
			unsigned actual_rng = 0;
			d_rand_get_state(&actual_rng);
			require(robot.shields == i2f(100) && !(robot.flags & OF_EXPLODING) &&
			            robot.ctype.ai_info.SKIP_AI_COUNT == expected_skip &&
			            robot.mtype.phys_info.rotthrust.x == expected[0] && robot.mtype.phys_info.rotthrust.y == expected[1] &&
			            robot.mtype.phys_info.rotthrust.z == expected[2] && robot.mtype.phys_info.flags == (3 | (spin ? PF_USES_THRUST : 0)),
			        ("ordinary Gauss torque differs: xl=" + std::to_string(xl) + " multi=" + std::to_string(multiplayer) + " frame=" + std::to_string(frame) + " skip=" + std::to_string(skip) + " actual_skip=" + std::to_string(robot.ctype.ai_info.SKIP_AI_COUNT) + " flags=" + std::to_string(robot.flags) + " thrust=" + std::to_string(robot.mtype.phys_info.rotthrust.x) + " expected=" + std::to_string(expected[0]) + " rng_calls=" + std::to_string(d_rand_get_call_count() - calls)).c_str());
			require(actual_rng == expected_rng && d_rand_get_call_count() - calls == unsigned((multiplayer ? 0 : 1) + (spin ? 3 : 0)),
			        "ordinary Gauss contact changed draw count or ordering");
			for (int i = Highest_object_index; i >= 0; --i)
				if (Objects[i].type != OBJ_NONE && (i > prior_highest || Objects[i].signature != signatures[i])) obj_delete(i);
		}
	Game_mode = saved_mode;
	FrameTime = saved_frame;
	obj_delete(victim);
	std::cout << "Ordinary Gauss signed torque and RNG controls passed; XL=" << xl << "\n";
}

static void projectile_robot_attacker_blast()
{
	int room = -1, guidebot = -1;
	vms_vector center{};
	for (int i = 0; i < Num_segments; ++i) {
		compute_segment_center(&center, &Segments[i]);
		if (Segments[i].objects < 0 && !get_seg_masks(&center, i, i2f(10), __FILE__, __LINE__).facemask) {
			room = i;
			break;
		}
	}
	for (int id = 0; id < N_robot_types; ++id)
		if (Robot_info[id].companion) {
			guidebot = id;
			break;
		}
	require(room >= 0 && guidebot >= 0 && !Robot_info[1].companion && !Robot_info[1].boss_flag,
	        "radial attacker controls require an empty room and distinct hostile/guidebot definitions");
	const int victim = projectile_robot(), attacker = projectile_robot();
	for (const int actor : { victim, attacker }) {
		obj_relink(actor, room);
		Objects[actor].pos = center;
	}
	Objects[attacker].pos.z += i2f(10);
	Objects[victim].pos.z += F1_0;
	GameTime64 += F1_0;
	const int old_cheat = cheats.robotskillrobots;
	for (const int profile : { 0, 1, 2, 3 }) {
		Objects[attacker].id = profile == 1 ? guidebot : 1;
		cheats.robotskillrobots = profile == 3;
		Objects[victim].shields = i2f(100);
		Objects[victim].mtype.phys_info.velocity = {};
		const int parent = profile == 2 ? int(ConsoleObject - Objects) : attacker;
		auto *effect = object_create_badass_explosion(nullptr, room, &center, F1_0, VCLIP_SMALL_EXPLOSION,
		                                              i2f(20), i2f(4), i2f(4), parent);
		require(effect, "radial attacker control could not allocate its explosion");
		require(Objects[victim].shields == i2f(profile == 0 ? 100 : 85),
		        "ordinary radial robot-attacker damage crossed its source profile");
		require(Objects[victim].mtype.phys_info.velocity.z != 0,
		        "shield-rejected radial contact lost its earlier force");
		obj_delete(int(effect - Objects));
	}
	cheats.robotskillrobots = old_cheat;
	obj_delete(attacker);
	obj_delete(victim);
	std::cout << "Native hostile/guidebot/player/cheat radial damage and force controls passed\n";
}

static void projectile_robot_damage_grace()
{
	const int victim = projectile_robot();
	const auto born = GameTime64;
	const int first = projectile_test_shot(ConsoleObject - Objects);
	GameTime64 = born + F1_0 - 1;
	collide_robot_and_weapon(&Objects[victim], &Objects[first], &Objects[victim].pos);
	require(Objects[victim].shields == i2f(100) && xl_projectile_has_hit(first, victim),
	        "newborn contact lost shield rejection or persistent history");
	require(Objects[first].shields == i2f(15) && !(Objects[first].flags & OF_SHOULD_BE_DEAD),
	        "newborn shield rejection bypassed projectile attrition");
	++GameTime64;
	collide_robot_and_weapon(&Objects[victim], &Objects[first], &Objects[victim].pos);
	require(Objects[victim].shields == i2f(100) && Objects[first].shields == i2f(15),
	        "maturing robot allowed a recorded duplicate hit");
	const int fresh = projectile_test_shot(ConsoleObject - Objects);
	collide_robot_and_weapon(&Objects[victim], &Objects[fresh], &Objects[victim].pos);
	require(Objects[victim].shields == i2f(60) && xl_projectile_has_hit(fresh, victim) &&
	            Objects[fresh].shields == i2f(25),
	        "fresh boundary shot lost ordinary damage or disposition");
	obj_delete(fresh);
	obj_delete(first);
	obj_delete(victim);
	std::cout << "Native robot grace contact, retained history and boundary fresh shot passed\n";
}

static void projectile_static_robot_controls(bool xl)
{
	for (int behavior : { AIB_STILL, AIB_NORMAL, 135 }) {
		const int owner = projectile_robot();
		auto &robot = Objects[owner];
		init_ai_object(owner, behavior, -1);
		const bool stationary = xl && behavior == 135;
		require(robot.control_type == (stationary ? CT_NONE : CT_AI) && robot.movement_type == (stationary ? MT_NONE : MT_PHYSICS),
		        "Static robot initialization changed ordinary or stock controls");
		GameTime64 += F1_0; // Mature the target without changing its birth
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
	GameTime64 += F1_0;
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
	GameTime64 += F1_0;
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
	GameTime64 += F1_0;
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
	const auto motion_before = xl_camera_motion_snapshot();
	const auto clock_before = xl_input_demo_cached_clock();
	const auto frame_before = FrameTime;
	const auto calls_before = d_rand_get_call_count();
	std::vector<xl_compat::CameraHitState> live_hits(hits_before.size());
	std::vector<bool> had_hits;
	for (std::size_t i = 0; i < hits_before.size(); ++i) had_hits.push_back(xl_camera_hit_state_get(hits_before[i].owner, live_hits[i]));
	for (int variant = 0; variant < 56; ++variant) {
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
		if (variant == 34) block.erase("cached_clock");
		if (variant == 35) block["cached_clock"] = -1;
		if (variant == 36) block["cached_clock"] = UINT64_MAX;
		if (variant == 37) block["cached_clock"] = 0.5;
		if (variant == 38) block["cached_clock"] = nlohmann::json::boolean_t(true);
		if (variant == 39) block.erase("camera_motion");
		if (variant == 40) block["camera_motion"]["ticks"] = block["cached_clock"].get<uint32_t>() + 1;
		if (variant == 41) block["camera_motion"]["ticks"] = -1;
		if (variant == 42) block["camera_motion"]["states"].push_back(nlohmann::json::array());
		if (variant == 43) block["camera_motion"]["version"] = 2;
		if (variant == 44) block.erase("frame_time");
		if (variant == 45) block["frame_time"] = -1;
		if (variant == 46) block["frame_time"] = uint64_t(INT32_MAX) + 1;
		if (variant == 47) block["frame_time"] = UINT64_MAX;
		if (variant == 48) block["frame_time"] = 0.5;
		if (variant == 49) block["frame_time"] = nlohmann::json::boolean_t(true);
		if (variant == 50) block.erase("sim_rng_calls");
		if (variant == 51) block["sim_rng_calls"] = -1;
		if (variant == 52) block["sim_rng_calls"] = uint64_t(UINT32_MAX) + 1;
		if (variant == 53) block["sim_rng_calls"] = UINT64_MAX;
		if (variant == 54) block["sim_rng_calls"] = 0.5;
		if (variant == 55) block["sim_rng_calls"] = nlohmann::json::boolean_t(true);
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
		require(FrameTime == frame_before && d_rand_get_call_count() == calls_before, "Malformed trailer published frame time or SIM count");
		require(xl_camera_motion_snapshot() == motion_before && xl_input_demo_cached_clock() == clock_before,
		        "Malformed trailer published camera motion or clock state");
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
	std::cout << "Fifty-six malformed projectile/camera hit/flash/clock trailers rejected before table publication; valid recovery passed\n";
}

static nlohmann::json camera_hit_continuation_snapshot()
{
	unsigned int sim_state = 0;
	require(d_rand_get_state(&sim_state), "Cannot capture SIM state for cold continuation");
	auto hits = nlohmann::json::array();
	for (const auto &hit : xl_camera_hit_snapshot()) hits.push_back({ hit.owner, hit.signature, hit.type, hit.elapsed });
	return { { "motion", xl_camera_motion_snapshot() }, { "cached_clock", xl_input_demo_cached_clock() }, { "hits", hits }, { "flash", xl_camera_flash_snapshot() }, { "children", camera_children_capture() }, { "frame_time", FrameTime }, { "sim_rng_calls", d_rand_get_call_count() }, { "sim_rng_state", sim_state } };
}
static void camera_hit_disk_controls(const std::filesystem::path &root)
{
	const int owner = int(ConsoleObject - Objects);
	xl_compat::CameraHitState earned;
	require(xl_camera_hit_state_get(owner, earned) && earned.repaired > 0, "Full camera hit save requires earned physical state");
	reset_time();
	SDL_Delay(2);
	calc_frame_time();
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
	const auto saved_continuation = camera_hit_continuation_snapshot();
	require(FrameTime > 0 && d_rand_get_call_count() > 0, "Cold continuation needs earned frame time and SIM calls");
	require(state_save_all_sub(save, description), "Cannot save earned camera hit state");
	std::ofstream receipt(root / "camera-hit-continuation.json");
	receipt << saved_continuation.dump(2) << '\n';
	receipt.close();
	require(bool(receipt), "Cannot write camera hit cold continuation receipt");
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_level_extensions_active() &&
	            xl_camera_hit_snapshot().empty() && !xl_camera_hit_state_get(owner, earned) && xl_camera_flash_snapshot().is_null() && !xl_camera_flash_frame(),
	        "Camera hit state leaked into stock");
	SDL_Delay(2);
	timer_update();
	xl_camera_controller_begin_frame();
	require(int32_t(xl_camera_controller_ticks()) > saved_ticks, "Camera hit disk restore did not advance the host clock");
	require(state_restore_all_sub(save, 0), "Cannot restore camera hit save through stock");
	require(camera_hit_continuation_snapshot() == saved_continuation, "Disk restore changed frame, RNG or camera continuation state");
	require(xl_camera_controller_ticks() == unsigned(saved_ticks), "Camera disk restore lost its cached frame clock");
	require(xl_camera_flash_snapshot() == saved_flash, "Full disk restore lost impact-earned flash delay/color");
	require(camera_children_capture() == saved_children, "Full disk restore lost smart child ownership, target or motion");
	for (const auto &child : saved_children) require(!xl_object_birth_order(child[0]), "Restored smart child retained omitted source creation order");
	xl_compat::CameraHitState restored;
	require(xl_camera_hit_state_get(owner, restored) && restored.repaired == int64_t(saved_ticks) - elapsed &&
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

static void camera_hit_cold_controls(const std::filesystem::path &root)
{
	nlohmann::json expected;
	std::ifstream receipt(root / "camera-hit-continuation.json");
	receipt >> expected;
	char save[] = "camera-hits.sav";
	require(state_restore_all_sub(save, 0), "Cannot cold restore camera hit save");
	const auto restored = camera_hit_continuation_snapshot();
	std::ofstream actual(root / "camera-hit-cold.json");
	actual << restored.dump(2) << '\n';
	actual.close();
	require(bool(actual), "Cannot write cold camera state receipt");
	require(restored == expected, "Cold camera hit restore changed clocks, elapsed repair, flash or children");
	const auto cached = xl_input_demo_cached_clock();
	SDL_Delay(2);
	timer_update();
	xl_camera_controller_begin_frame();
	require(xl_camera_controller_ticks() > cached && xl_camera_controller_ticks() == xl_input_demo_cached_clock(),
	        "Cold camera continuation did not advance in the saved clock domain");
	std::cout << "Cold camera hit continuation: cached clocks, repair ages, flash, children and next frame passed\n";
}

static int projectile_test(int argc, char **argv)
{
	std::setvbuf(stdout, nullptr, _IONBF, 0);
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		if (!std::strcmp(find_arg_value(argc, argv, "-projectile-test"), "resume")) {
			std::strcpy(Players[Player_num].callsign, "XLTest");
			input_demo_set_skip_level_intro(1);
			camera_hit_cold_controls(root);
			return 0;
		}
		require(load_base_mission("neptunxl", error, sizeof(error)), error);
		const bool vertigo = std::filesystem::exists(root / "data/d2x.ham");
		if (vertigo) require(N_robot_types == 78 && Robot_info[68].model_num == 168 && Robot_info[76].model_num >= 0,
			                 "Original Vertigo extension did not populate its authored robot slots");
		if (vertigo) {
			xl_assets::ObjectDependencies static_graph;
			static_graph.xl_boss_spawning = true;
			static_graph.robot_behavior(68, 135);
			static_graph.robot_behavior(76, AIB_NORMAL);
			static_graph.robot_behavior(76, 135);
			std::vector<robot_info> staged_static(Robot_info, Robot_info + N_robot_types);
			staged_static[76].companion = 1;
			static_graph.robot_data = staged_static.data();
			bool rejected = false;
			try {
				static_graph.robot_behavior(76, 135);
			} catch (const std::runtime_error &error) {
				rejected = std::strstr(error.what(), "static companion/thief") != nullptr;
			}
			require(rejected, "Static companion bypassed contact qualification");
			std::cout << "Original static robot behavior profiles and companion rejection passed\n";
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
		projectile_sound_dependency_controls();
		static_robot_native_controls(root, vertigo);
		require(load_base_mission("neptunxl", error, sizeof(error)), error);
		StartNewGame(1);
		Game_suspended = SUSP_ROBOTS;
		const bool camera_controls = camera_weapon_controls(root);
		projectile_static_robot_controls(true);
		if (camera_controls) camera_hit_disk_controls(root);
		else std::cout << "Camera disk controls not run: optional original XL addon absent\n";
		std::cout << "XL_CAMERA_SUBCHECKS: " << (camera_controls ? "executed" : "not_run_missing_addon") << '\n';
		StartNewGame(1);
		Game_suspended = SUSP_ROBOTS;
		projectile_impacts(root);
		projectile_hostile_robot_contact(true);
		projectile_ordinary_gauss_spin(true);
		projectile_robot_attacker_blast();
		projectile_robot_damage_grace();
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
		}
		// Batch creation preserves all recorded births at epoch seven
		GameTime64 += F1_0;
		for (const int victim : victims) {
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
		projectile_hostile_robot_contact(false);
		projectile_ordinary_gauss_spin(false);
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
