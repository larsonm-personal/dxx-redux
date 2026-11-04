static void camera_player_contact_controls(object &camera);
#include "resource_validation.h"
#include "camera_splash.h"
#include "explosion_motion.h"
#include "camera_controller.h"
#include "camera_collision.h"
#include <SDL.h>
#include "object_trigger.h"
#include "../model_hitbox_query.h"
#include "../model_render_geometry.h"
#include "../model_sphere_query.h"
#include "../camera_sphere_unstick.h"
extern "C" {
#include "automap.h"
#include "physics.h"
void state_object_to_object_rw(object *source, object_rw *target);
void state_object_rw_to_object(object_rw *source, object *target, int native_ai_format);
}
#include "../camera_contact.h"
#include "../camera_splash.h"
extern "C" {
#include "collide.h"
}
static void camera_rotation_native_controls(const std::filesystem::path &root)
{
	const int slot = projectile_robot();
	auto &actor = Objects[slot];
	const auto saved_actor = actor;
	auto *saved_console = ConsoleObject;
	const auto saved_frame = FrameTime;
	const auto saved_automap = Automap_active;
	std::ofstream output(root / "camera-rotation.txt");
	unsigned count = 0;
	std::array<int, 3> profiles{ -1, -1, -1 };
	for (int id = 0; id < N_robot_types; ++id) {
		const auto &r = Robot_info[id];
		const int profile = r.thief ? 1 : r.attack_type ? 2
		                                                : 0;
		if (profiles[profile] < 0) profiles[profile] = id;
	}
	require(profiles[0] >= 0 && profiles[1] >= 0 && profiles[2] >= 0, "Camera rotation requires normal, thief and attack robot definitions");
	for (int profile : { -1, profiles[0], profiles[1], profiles[2] }) {
		const bool robot = profile >= 0;
		for (bool physics : { false, true })
			for (bool morph : { false, true })
				for (int mass : { 0, F1_0, 4 * F1_0 })
					for (int frame : { 0, 512, 1092, 3276 })
						for (int skip : { 0, 60 })
							for (bool console : { false, true })
								for (bool map : { false, true })
									for (vms_vector force : { vms_vector{}, vms_vector{ 17, -13, 11 }, vms_vector{ 65536, 0, 0 }, vms_vector{ 0, -65536, 0 }, vms_vector{ -73177, 49001, 327681 }, vms_vector{ 2000000, -3000000, 4000000 } })
										for (vms_vector forward : { vms_vector{ 0, 0, F1_0 }, vms_vector{ 0, F1_0, 0 }, vms_vector{ F1_0, 0, 0 } }) {
											actor.type = robot ? OBJ_ROBOT : OBJ_PLAYER;
											actor.id = robot ? profile : 0;
											actor.movement_type = physics ? MT_PHYSICS : MT_NONE;
											actor.control_type = morph ? CT_MORPH : CT_NONE;
											actor.mtype.phys_info.mass = mass;
											actor.mtype.phys_info.rotvel = { 8192, -7000, 11 };
											actor.ctype.ai_info.SKIP_AI_COUNT = skip;
											actor.orient.fvec = forward;
											FrameTime = frame;
											ConsoleObject = console ? &actor : saved_console;
											Automap_active = map;
											d_srand(5678);
											const int roll = d_rand();
											d_srand(5678);
											const auto calls = d_rand_get_call_count();
											xl_camera_splash_rotation(actor, force);
											output << robot << ' ' << physics << ' ' << morph << ' ' << mass << ' ' << frame << ' ' << skip << ' '
											       << (robot && Robot_info[profile].thief) << ' ' << (robot && Robot_info[profile].attack_type) << ' '
											       << console << ' ' << map << ' ' << roll << ' ' << force.x << ' ' << force.y << ' ' << force.z << ' '
											       << forward.x << ' ' << forward.y << ' ' << forward.z << " 8192 -7000 11 "
											       << actor.mtype.phys_info.rotvel.x << ' ' << actor.mtype.phys_info.rotvel.y << ' ' << actor.mtype.phys_info.rotvel.z << ' '
											       << int(actor.ctype.ai_info.SKIP_AI_COUNT) << ' ' << d_rand_get_call_count() - calls << '\n';
											++count;
										}
	}
	output.close();
	require(bool(output), "Cannot write native camera rotation receipt");
	actor = saved_actor;
	ConsoleObject = saved_console;
	FrameTime = saved_frame;
	Automap_active = saved_automap;
	obj_delete(slot);
	std::cout << "Native camera rotation samples: " << count << "\n";
}
// Newborn actors still receive force and rotation before the source damage gate
static void camera_radial_robot_controls(const std::filesystem::path &root, object &camera)
{
	const auto saved_time = GameTime64;
	const auto saved_difficulty = Difficulty_level;
	GameTime64 = 5 * F1_0;
	const int target = projectile_robot();
	auto &actor = Objects[target];
	const int shot = projectile_test_shot(ConsoleObject - Objects, GAUSS_ID);
	auto &weapon = Objects[shot];
	const auto original_actor = actor;
	require(xl_object_birth_time(target) == GameTime64, "Camera radial actor lost dynamic birth time");
	std::array<int, 3> profiles{ -1, -1, -1 };
	for (int id = 0; id < N_robot_types; ++id) {
		const auto &r = Robot_info[id];
		if (r.boss_flag || r.companion || r.strength <= 0 || r.mass <= 0) continue;
		const int profile = r.thief ? 1 : r.attack_type ? 2
		                                                : 0;
		if (profiles[profile] < 0) profiles[profile] = id;
	}
	require(profiles[0] >= 0 && profiles[1] >= 0 && profiles[2] >= 0, "Camera radial controls need three original robot profiles");
	std::ofstream output(root / "camera-rotation.txt", std::ios::app);
	unsigned count = 0;
	for (int weapon_id : { GAUSS_ID, CONCUSSION_ID, HOMING_ID, GUIDEDMISS_ID, MERCURY_ID })
		for (int age : { 0, F1_0 - 1, F1_0, 2 * F1_0 })
			for (int profile : profiles)
				for (int difficulty = 0; difficulty < 5; ++difficulty)
					for (int distance : { 0, 1, 3 * F1_0, 10 * F1_0 })
						for (int state = 0; state < 4; ++state) {
							weapon.id = weapon_id;
							GameTime64 = 5 * F1_0 + age;
							Difficulty_level = difficulty;
							const auto &definition = Robot_info[profile];
							actor.id = profile;
							actor.flags = state == 1 ? OF_EXPLODING : state == 3 ? OF_SHOULD_BE_DEAD
							                                                     : 0;
							actor.shields = state == 2 ? -1 : 1000 * F1_0;
							actor.mtype.phys_info.mass = definition.mass;
							actor.mtype.phys_info.velocity = actor.mtype.phys_info.rotvel = {};
							actor.ctype.ai_info.SKIP_AI_COUNT = 0;
							actor.orient = vmd_identity_matrix;
							auto impact = camera.pos;
							const auto expected_impact = xl_compat::camera_splash_impact({ impact.x, impact.y, impact.z }, { camera.pos.x, camera.pos.y, camera.pos.z }, camera.size, weapon.size);
							actor.pos = { expected_impact[0] + distance, expected_impact[1], expected_impact[2] };
							require(find_point_seg(&actor.pos, camera.segnum) == camera.segnum, "Radial actor escaped original isolated room");
							const auto sample = xl_compat::camera_splash_sample({ actor.pos.x, actor.pos.y, actor.pos.z }, expected_impact,
							                                                    Weapon_info[weapon_id].strength[difficulty], Weapon_info[weapon_id].damage_radius, Weapon_info[weapon_id].strength[difficulty]);
							const bool affected = sample.inside && state != 3;
							const auto impulse = affected ? sample.impulse : xl_compat::FixedLightPoint{};
							const auto expected_velocity = xl_compat::camera_force_velocity({}, impulse, definition.mass, false);
							auto rotation = impulse;
							for (auto &axis : rotation) axis = xl_compat::splash_multiply(axis, -2 * F1_0 * (7 - difficulty) / 8);
							const int shields = actor.shields;
							ConsoleObject->shields = Players[Player_num].shields = 1000 * F1_0;
							ConsoleObject->mtype.phys_info.velocity = ConsoleObject->mtype.phys_info.rotvel = {};
							std::array<int, MAX_OBJECTS> signatures{};
							const int highest = Highest_object_index;
							for (int i = 0; i <= highest; ++i) signatures[i] = Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature;
							d_srand(5432);
							const int roll = d_rand();
							d_srand(5432);
							const auto calls = d_rand_get_call_count();
							xl_camera_splash(weapon, camera, impact);
							require(actor.shields == shields - (affected && state == 0 && age >= F1_0 ? sample.damage : 0), "Camera radial robot damage or dead/exploding gate differs");
							require(actor.mtype.phys_info.velocity.x == expected_velocity[0] && actor.mtype.phys_info.velocity.y == expected_velocity[1] &&
							            actor.mtype.phys_info.velocity.z == expected_velocity[2],
							        "Camera radial robot force or radius gate differs");
							output << "1 1 0 " << definition.mass << ' ' << FrameTime << " 0 " << bool(definition.thief) << ' ' << bool(definition.attack_type)
							       << " 0 0 " << roll << ' ' << rotation[0] << ' ' << rotation[1] << ' ' << rotation[2] << " 0 0 65536 0 0 0 "
							       << actor.mtype.phys_info.rotvel.x << ' ' << actor.mtype.phys_info.rotvel.y << ' ' << actor.mtype.phys_info.rotvel.z << ' '
							       << int(actor.ctype.ai_info.SKIP_AI_COUNT) << ' ' << d_rand_get_call_count() - calls << '\n';
							for (int i = 0; i <= Highest_object_index; ++i)
								if (Objects[i].type != OBJ_NONE && (i > highest || Objects[i].signature != signatures[i])) {
									require(Objects[i].type == OBJ_FIREBALL, "Camera radial robot test created unexpected object");
									obj_delete(i);
								}
							++count;
						}
	output.close();
	require(bool(output), "Cannot append camera radial rotation receipt");
	actor = original_actor;
	const int old_signature = actor.signature;
	obj_delete(target);
	require(xl_object_birth_time(target) == 0, "Deleted object retained a live creation time");
	GameTime64 = 9 * F1_0;
	const int reused = projectile_robot();
	require(reused == target && Objects[reused].signature != old_signature && xl_object_birth_time(reused) == GameTime64,
	        "Reused slot inherited the previous object's creation time");
	obj_delete(reused);
	obj_delete(shot);
	GameTime64 = saved_time;
	Difficulty_level = saved_difficulty;
	std::cout << "Native camera radial robot contacts: " << count << " passed\n";
}
static unsigned camera_motion_native_controls(std::ostream &output, object &effect)
{
	const auto origin = effect.pos;
	const auto duration = effect.lifeleft;
	const auto flags = effect.flags;
	auto *saved_viewer = Viewer;
	auto viewer = *ConsoleObject;
	Viewer = &viewer;
	unsigned count = 0;
	for (const vms_vector offset : { vms_vector{}, vms_vector{ 655360, 0, 0 }, vms_vector{ 0, -655360, 0 }, vms_vector{ 1, -1, 1 }, vms_vector{ -733333, 490001, 1310720 } }) {
		viewer.pos = { origin.x + offset.x, origin.y + offset.y, origin.z + offset.z };
		for (int remaining : { duration, duration * 3 / 4, duration / 2, 1, 0, -1 }) {
			effect.lifeleft = remaining;
			effect.flags = flags;
			do_explosion_sequence(&effect);
			output << origin.x << ' ' << origin.y << ' ' << origin.z << ' ' << viewer.pos.x << ' ' << viewer.pos.y << ' ' << viewer.pos.z << ' '
			       << effect.size << ' ' << duration << ' ' << remaining << ' ' << effect.pos.x << ' ' << effect.pos.y << ' ' << effect.pos.z << ' '
			       << effect.lifeleft << ' ' << bool(effect.flags & OF_SHOULD_BE_DEAD) << ' ' << int(effect.id) << '\n';
			++count;
		}
	}
	Viewer = saved_viewer;
	return count;
}
static void camera_controller_native_controls(const std::filesystem::path &root, object &camera)
{
	const int owner = int(&camera - Objects);
	const auto saved = camera;
	xl_compat::Inventory fixture;
	fixture.segments.resize(Num_segments);
	fixture.objects.resize(Highest_object_index + 1);
	fixture.objects[owner].type = 16;
	fixture.object_trigger_records.push_back({ { 16, 0, 0, -1, { { camera.segnum, 0 }, { camera.segnum, 1 } } }, owner });
	unsigned trigger_cases = 0;
	for (int flags : { 0, 4, 2, 16, 128, 132, 144 }) {
		auto triggers = fixture;
		triggers.object_trigger_records[0].action.flags = flags;
		triggers.object_trigger_records.push_back({ { 16, flags, 73, -1, { { camera.segnum, 2 } } }, owner });
		xl_object_trigger_load(triggers);
		xl_object_trigger_state states[2]{};
		require(xl_object_trigger_count() == 2, "Camera trigger records disappeared");
		for (int i = 0; i < 2; ++i) {
			require(xl_object_trigger_state_get(i, &states[i]) && states[i].owner == owner && states[i].signature == camera.signature &&
			            states[i].type == 16 && states[i].flags == flags && states[i].value == (i ? 0 : 73) && !states[i].operated,
			        "Camera trigger sorted identity changed");
		}
		xl_object_trigger_frame();
		for (int i = 0; i < 2; ++i) {
			xl_object_trigger_state current{};
			require(xl_object_trigger_state_get(i, &current) && current.owner == owner && !current.operated &&
			            current.flags == (flags | ((flags & 128) ? TF_DISABLED : 0)),
			        "Camera trigger executed or autoplay bookkeeping changed");
		}
		require(xl_object_trigger_restore(states, 2, 1), "Camera trigger identities failed restore");
		++camera.signature;
		require(!xl_object_trigger_restore(states, 2, 0), "Camera trigger restored a reused object identity");
		--camera.signature;
		++trigger_cases;
	}
	auto invalid_camera_trigger = fixture;
	invalid_camera_trigger.object_trigger_records[0].object = int(ConsoleObject - Objects);
	bool rejected_camera_trigger = false;
	try {
		xl_object_trigger_load(invalid_camera_trigger);
	} catch (const std::runtime_error &) {
		rejected_camera_trigger = true;
	}
	require(rejected_camera_trigger, "Camera trigger accepted an unqualified non-camera owner");
	xl_object_trigger_clear();
	std::cout << "Native camera trigger identity profiles: " << trigger_cases << " passed\n";
	std::ofstream output(root / "camera-controller.txt");
	unsigned cases = 0;
	unsigned restore_cases = 0;
	const auto write_matrix = [&](const vms_matrix &m) {
		for (const auto *v : { &m.rvec, &m.uvec, &m.fvec }) output << v->x << ' ' << v->y << ' ' << v->z << ' ';
	};
	for (const auto original : { vmd_identity_matrix,
	                             vms_matrix{ { 0, F1_0, 0 }, { 0, 0, F1_0 }, { F1_0, 0, 0 } },
	                             vms_matrix{ { 65530, 19, -721 }, { -17, 65500, 1201 }, { 737, -1199, 65519 } } })
		for (int behavior : { 128, 129, 135 }) {
			camera.orient = original;
			camera.ctype.ai_info.behavior = behavior;
			camera.mtype.phys_info.brakes = 73;
			xl_camera_controller_begin_frame();
			const auto load_ticks = xl_camera_controller_ticks();
			xl_camera_controller_load(fixture);
			require(xl_camera_controller_ticks() == load_ticks, "Camera load reset the cached game clock");
			// Exercise the real stock object record on disk, followed by Redux's
			// cleared-slot conversion and the production XL restore hook
			camera.orient = vms_matrix{ { 0, F1_0, 0 }, { 0, 0, F1_0 }, { F1_0, 0, 0 } };
			camera.signature += 7;
			const auto saved_orientation = camera.orient;
			object_rw record{};
			state_object_to_object_rw(&camera, &record);
			{
				std::ofstream disk(root / "camera-object.bin", std::ios::binary);
				disk.write(reinterpret_cast<const char *>(&record), sizeof(record));
				disk.close();
				require(bool(disk), "Cannot write native camera object record");
			}
			record = {};
			{
				std::ifstream disk(root / "camera-object.bin", std::ios::binary);
				disk.read(reinterpret_cast<char *>(&record), sizeof(record));
				require(bool(disk), "Cannot read native camera object record");
			}
			camera = {};
			state_object_rw_to_object(&record, &camera, 1);
			require(!camera.ctype.ai_info.behavior && !camera.mtype.phys_info.brakes, "Stock camera record unexpectedly serialized omitted payload");
			const auto restored = camera;
			camera.control_type = CT_AI;
			require(!xl_camera_controller_restore(1) && !camera.ctype.ai_info.behavior, "Invalid camera restore published authored payload");
			camera = restored;
			require(xl_camera_controller_restore(0) && !camera.ctype.ai_info.behavior && xl_camera_controller_restore(1) &&
			            camera.ctype.ai_info.behavior == behavior && camera.mtype.phys_info.brakes == 73 &&
			            !std::memcmp(&camera.orient, &saved_orientation, sizeof(saved_orientation)),
			        "Camera restore lost authored payload or saved orientation");
			require(xl_camera_controller_ticks() == load_ticks, "Camera restore reset the cached game clock");
			++restore_cases;
			XlCameraControllerState before, after;
			require(xl_camera_controller_get(owner, before) && before.monitor == 1 && !before.rotation.angle && !before.rotation.delta && !before.rotation.t0,
			        "Camera controller did not initialize the last monitor binding");
			for (int frame = 0; frame < 1200; ++frame) {
				xl_camera_controller_begin_frame();
				require(xl_camera_controller_get(owner, before), "Bound camera controller disappeared");
				const auto orientation = camera.orient;
				const auto brakes = camera.mtype.phys_info.brakes;
				const auto calls = d_rand_get_call_count();
				object_move_one(&camera);
				require(xl_camera_controller_get(owner, after) && d_rand_get_call_count() == calls && camera.lifeleft == IMMORTAL_TIME &&
				            camera.type == 16 && camera.control_type == 18 && !std::memcmp(&camera.pos, &saved.pos, sizeof(camera.pos)),
				        "Native CT_CAMERA frame changed unrelated object state or RNG");
				output << behavior << ' ' << xl_camera_controller_ticks() << " 5000 " << before.rotation.angle << ' ' << before.rotation.delta << ' '
				       << before.rotation.t0 << ' ' << brakes << ' ';
				write_matrix(original);
				write_matrix(orientation);
				output << after.rotation.angle << ' ' << after.rotation.delta << ' ' << after.rotation.t0 << ' ' << camera.mtype.phys_info.brakes << ' ';
				write_matrix(camera.orient);
				output << '\n';
				++cases;
			}
			for (int variant = 0; variant < 3; ++variant) {
				auto bad = fixture;
				const auto previous_orientation = camera.orient;
				const auto previous_control = camera.control_type;
				if (variant == 0) bad.object_trigger_records[0].action.targets[0][1] = 6;
				if (variant == 1) camera.control_type = CT_AI;
				if (variant == 2) camera.orient.fvec = {};
				bool rejected = false;
				try {
					xl_camera_controller_load(bad);
				} catch (const std::runtime_error &) {
					rejected = true;
				}
				camera.orient = previous_orientation;
				camera.control_type = previous_control;
				require(rejected && xl_camera_controller_get(owner, before) && before.rotation.angle == after.rotation.angle && before.rotation.delta == after.rotation.delta,
				        "Invalid camera bind replaced live controller state");
			}
		}
	fixture.object_trigger_records.clear();
	xl_camera_controller_load(fixture);
	camera.ctype.ai_info.behavior = 129;
	const auto unbound = camera.orient;
	object_move_one(&camera);
	XlCameraControllerState absent;
	require(!xl_camera_controller_get(owner, absent) && !std::memcmp(&unbound, &camera.orient, sizeof(unbound)), "Unbound camera rotated without a monitor");
	const auto clear_ticks = xl_camera_controller_ticks();
	require(clear_ticks != 0, "Camera clock lifetime controls did not exercise elapsed time");
	xl_camera_controller_clear();
	require(xl_camera_controller_ticks() == clear_ticks, "Camera clear reset the cached game clock");
	camera = saved;
	output.close();
	require(bool(output), "Cannot write native camera controller receipt");
	std::cout << "Native camera controller frames: " << cases << " passed\n";
	std::cout << "Native camera object record restores: " << restore_cases << " passed\n";
}
static void camera_physics_native_controls(object &camera)
{
	using Point = xl_compat::FixedLightPoint;
	const auto point = [](const vms_vector &v) { return Point{ v.x, v.y, v.z }; };
	const auto saved_player = *ConsoleObject, saved_camera = camera;
	const auto saved_frame = FrameTime;
	const int saved_map = Automap_active, saved_awareness = Num_awareness_events;
	const int owner = int(ConsoleObject - Objects);
	unsigned cases = 0, flashes = 0;
	for (int behavior : { 128, 129 })
		for (int axis = 0; axis < 3; ++axis)
			for (int direction : { -1, 1 })
				for (int duration : { F1_0 / 8, F1_0 / 4 }) {
					*ConsoleObject = saved_player;
					camera = saved_camera;
					camera.ctype.ai_info.behavior = behavior;
					camera.flags = 0;
					Automap_active = 0;
					Num_awareness_events = 0;
					FrameTime = duration;
					ConsoleObject->type = OBJ_PLAYER;
					ConsoleObject->id = Player_num;
					ConsoleObject->movement_type = MT_PHYSICS;
					ConsoleObject->flags = 0;
					ConsoleObject->mtype.phys_info = {};
					ConsoleObject->mtype.phys_info.mass = F1_0;
					Point start = point(camera.pos), velocity{};
					start[axis] -= direction * 5 * F1_0;
					velocity[axis] = direction * 32 * F1_0;
					ConsoleObject->pos = { start[0], start[1], start[2] };
					ConsoleObject->last_pos = ConsoleObject->pos;
					ConsoleObject->mtype.phys_info.velocity = { velocity[0], velocity[1], velocity[2] };
					vms_vector offset{}, end{}, hit{}, normal{};
					vm_vec_copy_scale(&offset, &ConsoleObject->mtype.phys_info.velocity, xl_preview_motion_time(ConsoleObject->segnum, FrameTime));
					vm_vec_add(&end, &ConsoleObject->pos, &offset);
					require(!get_seg_masks(&ConsoleObject->pos, ConsoleObject->segnum, ConsoleObject->size, __FILE__, __LINE__).facemask &&
					            !get_seg_masks(&end, ConsoleObject->segnum, ConsoleObject->size, __FILE__, __LINE__).facemask,
					        "Camera physics control touches a room wall");
					xl_object_birth_created(owner);
					xl_camera_controller_begin_frame();
					require(xl_camera_query(&camera, ConsoleObject, &ConsoleObject->pos, &end, ConsoleObject->size, &hit, &normal) != 0, "Camera physics control requires a contact");
					xl_compat::CameraUnstickState expected{ start, point(end), point(offset) };
					xl_compat::CameraHitState expected_hits, actual_hits;
					auto expected_position = point(hit);
					xl_compat::camera_sphere_unstick(expected, expected_hits, expected_position, point(camera.pos), velocity,
					                                 ConsoleObject->size, OBJ_PLAYER, int32_t(xl_camera_controller_ticks()), 2000);
					const auto reflected = xl_compat::camera_reflect_velocity(velocity, F1_0, false);
					std::array<int, MAX_OBJECTS> signatures{};
					for (int i = 0; i <= Highest_object_index; ++i) signatures[i] = Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature;
					const int old_highest = Highest_object_index;
					do_physics_sim(ConsoleObject);
					require(point(ConsoleObject->pos) == expected_position && point(ConsoleObject->mtype.phys_info.velocity) == reflected,
					        "Native camera physics changed source unsticking or reconstructed response velocity");
					require(xl_camera_hit_state_get(owner, actual_hits) && actual_hits.repaired == expected_hits.repaired &&
					            actual_hits.shield_time == expected_hits.shield_time && actual_hits.next == expected_hits.next &&
					            actual_hits.points == expected_hits.points && actual_hits.times == expected_hits.times,
					        "Native camera physics lost reverse-query hit state");
					unsigned created = 0;
					for (int i = 0; i <= Highest_object_index; ++i) {
						if (Objects[i].type == OBJ_NONE || (i <= old_highest && signatures[i] == Objects[i].signature)) continue;
						require(Objects[i].type == OBJ_FIREBALL && point(Objects[i].pos) == point(hit), "Native camera physics did not pass the source contact point to its impact flash");
						++created;
						obj_delete(i);
					}
					require(created == 1, "Native camera physics produced the wrong flash count");
					flashes += created;
					++cases;
				}
	xl_compat::CameraHitState state;
	++ConsoleObject->signature;
	require(!xl_camera_hit_state_get(owner, state), "Reused object signature inherited camera hit state");
	--ConsoleObject->signature;
	ConsoleObject->type = OBJ_WEAPON;
	require(!xl_camera_hit_state_get(owner, state), "Changed object type inherited camera hit state");
	ConsoleObject->type = OBJ_PLAYER;
	xl_object_birth_created(owner);
	require(!xl_camera_hit_state_get(owner, state), "Object creation retained camera hit state");
	xl_object_birth_clear();
	require(!xl_camera_hit_state_get(owner, state), "Level reset retained camera hit state");
	*ConsoleObject = saved_player;
	camera = saved_camera;
	FrameTime = saved_frame;
	Automap_active = saved_map;
	Num_awareness_events = saved_awareness;
	std::cout << "Native camera physics: " << cases << " frames, " << flashes << " exact contact flashes, reverse hit state and identity/reset controls passed\n";
}
static void camera_projectile_physics_controls(object &camera)
{
	using Point = xl_compat::FixedLightPoint;
	const auto point = [](const vms_vector &v) { return Point{ v.x, v.y, v.z }; };
	const auto saved_camera = camera, saved_parent = *ConsoleObject;
	const auto saved_player = Players[Player_num];
	const auto saved_frame = FrameTime;
	const int saved_awareness = Num_awareness_events;
	const int victim = int(&camera - Objects);
	unsigned cases = 0, dead = 0, retries = 0, duplicates = 0;
	unsigned vulcan_aware = 0, vulcan_filtered = 0;
	std::array<int, 2> seeds{ -1, -1 };
	for (int seed = 0; seed < 4096 && (seeds[0] < 0 || seeds[1] < 0); ++seed) {
		d_srand(seed);
		const unsigned branch = d_rand() <= 3276 ? 0 : 1;
		if (seeds[branch] < 0) seeds[branch] = seed;
	}
	require(seeds[0] >= 0 && seeds[1] >= 0, "Cannot seed both source Vulcan awareness branches");
	FrameTime = F1_0 / 8;
	for (int behavior : { 128, 129 })
		for (int id : { LASER_ID_L1, FUSION_ID, GAUSS_ID, VULCAN_ID, CONCUSSION_ID, HOMING_ID, GUIDEDMISS_ID, MERCURY_ID })
			for (bool harmless : { false, true })
				for (int strength : { 10, 40 })
					for (int seed : seeds)
						for (int direction : { -1, 1 }) {
							camera = saved_camera;
							camera.ctype.ai_info.behavior = behavior;
							camera.flags = 0;
							camera.shields = 100 * F1_0;
							camera.mtype.phys_info.flags = 0;
							camera.ctype.ai_info.SKIP_AI_COUNT = 0;
							*ConsoleObject = saved_parent;
							ConsoleObject->mtype.phys_info.velocity = {};
							ConsoleObject->mtype.phys_info.rotvel = {};
							ConsoleObject->shields = Players[Player_num].shields = 1000 * F1_0;
							Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
							const int shot = projectile_test_shot(int(ConsoleObject - Objects), id);
							auto &weapon = Objects[shot];
							weapon.shields = strength * F1_0;
							weapon.flags = harmless ? OF_HARMLESS : 0;
							const bool persistent = weapon.mtype.phys_info.flags & PF_PERSISTENT;
							weapon.mtype.phys_info.flags &= PF_PERSISTENT;
							weapon.mtype.phys_info.drag = 0;
							weapon.mtype.phys_info.thrust = weapon.mtype.phys_info.rotvel = weapon.mtype.phys_info.rotthrust = {};
							const Point velocity{ direction * 32 * F1_0, 0, 0 };
							weapon.mtype.phys_info.velocity = { velocity[0], 0, 0 };
							weapon.pos = camera.pos;
							weapon.pos.x -= direction * F1_0;
							weapon.last_pos = weapon.pos;
							const auto start = weapon.pos;
							vms_vector offset{}, end{}, hit{}, normal{};
							vm_vec_copy_scale(&offset, &weapon.mtype.phys_info.velocity, xl_preview_motion_time(weapon.segnum, FrameTime));
							vm_vec_add(&end, &start, &offset);
							xl_camera_controller_begin_frame();
							require(xl_camera_query(&camera, &weapon, &start, &end, weapon.size, &hit, &normal) != 0 && point(hit) == point(start), "Projectile physics control requires an inside-start contact");
							const auto reflected = xl_compat::camera_reflect_velocity(velocity, weapon.mtype.phys_info.mass, harmless);
							const bool killed = !persistent || strength <= 25;
							xl_compat::CameraUnstickState expected{ point(start), point(end), point(offset) };
							xl_compat::CameraHitState expected_hits;
							auto position = point(hit);
							xl_compat::camera_sphere_unstick(expected, expected_hits, position, point(camera.pos), velocity, weapon.size, OBJ_WEAPON, int32_t(xl_camera_controller_ticks()), 2000);
							if (!killed) {
								vms_vector response{ reflected[0], reflected[1], reflected[2] }, retry{};
								vm_vec_copy_scale(&retry, &response, xl_preview_motion_time(weapon.segnum, FrameTime));
								position[0] += retry.x;
								position[1] += retry.y;
								position[2] += retry.z;
							}
							const vms_vector final_point{ position[0], position[1], position[2] };
							require(!get_seg_masks(&final_point, weapon.segnum, weapon.size, __FILE__, __LINE__).facemask, "Projectile retry control touches a room wall");
							std::array<int, MAX_OBJECTS> signatures{};
							const int old_highest = Highest_object_index;
							for (int i = 0; i <= old_highest; ++i) signatures[i] = Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature;
							Num_awareness_events = 0;
							d_srand(seed);
							const bool aware = id != VULCAN_ID || d_rand() <= 3276;
							const unsigned expected_calls = unsigned(id == VULCAN_ID) + unsigned(aware) + unsigned(id == GAUSS_ID ? 3 : 0);
							d_srand(seed);
							const auto random_calls = d_rand_get_call_count();
							do_physics_sim(&weapon);
							require(point(weapon.pos) == position && point(weapon.mtype.phys_info.velocity) == reflected && bool(weapon.flags & OF_SHOULD_BE_DEAD) == killed,
							        "Native projectile camera physics changed position, response velocity or death");
							if (Num_awareness_events != int(aware) || d_rand_get_call_count() - random_calls != expected_calls)
								std::cerr << "Projectile physics effects id=" << id << " seed=" << seed << " awareness=" << Num_awareness_events << "/" << aware << " RNG=" << d_rand_get_call_count() - random_calls << "/" << expected_calls << '\n';
							require(Num_awareness_events == int(aware) && d_rand_get_call_count() - random_calls == expected_calls, "Native projectile retry changed source camera awareness or RNG");
							if (id == VULCAN_ID) {
								vulcan_aware += aware;
								vulcan_filtered += !aware;
							}
							require(!persistent || (xl_projectile_has_hit(shot, victim) && weapon.shields == (killed ? 0 : 15 * F1_0)), "Native projectile physics lost persistent hit identity or strength");
							xl_compat::CameraHitState actual_hits;
							require(xl_camera_hit_state_get(shot, actual_hits) && !actual_hits.repaired && !actual_hits.shield_time && !actual_hits.next, "Weapon reverse query recorded player hit state");
							const bool splash = Weapon_info[id].damage_radius != 0;
							const auto impact = splash ? xl_compat::camera_splash_impact(point(hit), point(camera.pos), camera.size, weapon.size) : point(hit);
							unsigned flashes = 0;
							for (int i = 0; i <= Highest_object_index; ++i) {
								if (Objects[i].type == OBJ_NONE || (i <= old_highest && signatures[i] == Objects[i].signature)) continue;
								require(Objects[i].type == OBJ_FIREBALL && point(Objects[i].pos) == impact, "Native projectile camera effect lost source impact position");
								++flashes;
								obj_delete(i);
							}
							require(flashes == unsigned(Weapon_info[id].robot_hit_vclip >= 0) + unsigned(splash), "Native projectile retry duplicated or omitted an impact flash");
							if (!killed) {
								weapon.pos = start;
								weapon.mtype.phys_info.velocity = { velocity[0], 0, 0 };
								Num_awareness_events = 0;
								const auto calls = d_rand_get_call_count();
								position = expected.new_position;
								position[0] += offset.x;
								position[1] += offset.y;
								position[2] += offset.z;
								do_physics_sim(&weapon);
								xl_projectile_state history{};
								require(point(weapon.pos) == position && weapon.shields == 15 * F1_0 && !Num_awareness_events && d_rand_get_call_count() == calls &&
								            xl_projectile_state_get(shot, &history) && history.count == 1,
								        "Repeated physical camera contact escaped persistent history");
								++duplicates;
							}
							dead += killed;
							retries += !killed;
							++cases;
							obj_delete(shot);
							require(!xl_camera_hit_state_get(shot, actual_hits), "Deleted projectile retained accessible camera hit state");
						}
	camera = saved_camera;
	*ConsoleObject = saved_parent;
	Players[Player_num] = saved_player;
	FrameTime = saved_frame;
	Num_awareness_events = saved_awareness;
	require(vulcan_aware && vulcan_filtered, "Projectile physics controls missed a source Vulcan awareness branch");
	std::cout << "Native camera projectile physics: " << cases << " frames, " << dead << " deaths, " << retries << " retries, " << duplicates << " repeated-contact frames, Vulcan awareness " << vulcan_aware << " accepted/" << vulcan_filtered << " filtered passed\n";
}
#include "camera_passive_test.h"
#include "camera_flash_test.h"
#include "camera_children_test.h"
#include "camera_missile_test.h"
#include "camera_glass_test.h"
#include "static_camera_test.h"
static void camera_weapon_controls(const std::filesystem::path &root)
{
	if (!std::filesystem::exists(root / "data/d2x-xl.hog")) return;
	require(xl_level_extensions_active(), "Camera projectile controls require a real active XL level");
	int room = -1;
	vms_vector center{};
	for (int segment = 0; segment < Num_segments; ++segment) {
		compute_segment_center(&center, &Segments[segment]);
		if (Segments[segment].objects >= 0 || get_seg_masks(&center, segment, 10 * F1_0, __FILE__, __LINE__).facemask) continue;
		bool clear = true;
		for (int index = 0; index <= Highest_object_index; ++index) {
			if (Objects[index].type == OBJ_NONE || &Objects[index] == ConsoleObject) continue;
			if (vm_vec_dist(&Objects[index].pos, &center) < 20 * F1_0) {
				clear = false;
				break;
			}
		}
		if (clear) {
			room = segment;
			break;
		}
	}
	require(room >= 0, "Camera splash control needs an isolated room with real geometry");
	obj_relink(int(ConsoleObject - Objects), room);
	ConsoleObject->pos = center;
	XlPreparedResources resources("__camera_contact__.rl2");
	auto replacement_resources = std::make_unique<XlPreparedResources>("__camera_contact_replacement__.rl2");
	replacement_resources->prepare_camera_model();
	resources.prepare_camera_model();
	resources.install_hxm();
	const int victim = projectile_robot();
	auto &camera = Objects[victim];
	camera.type = 16;
	camera.ctype.ai_info.behavior = 128;
	resources.initialize_camera_objects();
	unsigned model_cases = 0;
	const auto &camera_sphere = resources.model_sphere(resources.camera_model_id());
	const auto &player_sphere = resources.model_sphere(ConsoleObject->rtype.pobj_info.model_num);
	require(camera_sphere.radius == 108769 && camera_sphere.center == xl_compat::FixedLightPoint{ 33, 16350, 16284 }, "Native camera sphere differs from extracted post-build source");
	require(player_sphere.radius == 297534 && player_sphere.center == xl_compat::FixedLightPoint{ 953, -34355, -16331 }, "Native player sphere differs from extracted post-build source");
	require(resources.model_collision_radius(resources.camera_model_id()) == camera_sphere.radius, "Appended camera retained legacy fallback radius");
	require(resources.model_collision_radius(ConsoleObject->rtype.pobj_info.model_num) == Polygon_models[ConsoleObject->rtype.pobj_info.model_num].rad, "Player collision radius lost source base preference");
	std::cout << "Native post-build model spheres: camera 108769, player 297534, base-radius preference passed\n";
	for (const auto entry : { std::pair<int, const char *>{ resources.camera_model_id(), "camera-model.bin" },
	                          std::pair<int, const char *>{ int{ ConsoleObject->rtype.pobj_info.model_num }, "player-model.bin" } }) {
		auto &cache = resources.model_hitboxes(entry.first);
		require(&cache == &resources.model_hitboxes(entry.first), "Native model did not retain a single hitbox cache");
		const auto &model = resources.dependencies().model_data[entry.first];
		std::ofstream payload(root / entry.second, std::ios::binary);
		payload.write(reinterpret_cast<const char *>(model.model_data), model.model_data_size);
		payload.close();
		require(bool(payload), "Cannot export native-owned model payload");
		++model_cases;
	}
	const std::array<xl_compat::FixedLightPoint, 3> identity{ { { F1_0, 0, 0 }, { 0, F1_0, 0 }, { 0, 0, F1_0 } } };
	unsigned query_hits = 0;
	for (int frame = 1; frame <= 25; ++frame) {
		xl_compat::FixedLightPoint hit{}, normal{}, start{ -16 * F1_0, 0, 0 }, end{ (frame - 13) * F1_0 / 2, 0, 0 };
		int16_t model = -1;
		const auto distance = xl_compat::hitbox_pair_query(hit, normal, model,
		                                                   resources.model_hitboxes(ConsoleObject->rtype.pobj_info.model_num), identity,
		                                                   resources.model_hitboxes(resources.camera_model_id()), identity, {}, frame, 1, start, end);
		query_hits += distance != 0;
	}
	require(query_hits > 0, "Native original camera/player models never intersected");
	const auto previous_vertex = resources.model_hitboxes(resources.camera_model_id()).transform(identity, {}, 25, 1)[0].faces[0][0];
	resources.install_hxm();
	const auto reloaded_vertex = resources.model_hitboxes(resources.camera_model_id()).transform(identity, { 5 * F1_0, 0, 0 }, 25, 1)[0].faces[0][0];
	require(reloaded_vertex[0] == previous_vertex[0] + 5 * F1_0 && reloaded_vertex[1] == previous_vertex[1] && reloaded_vertex[2] == previous_vertex[2],
	        "Model reinstall retained stale same-frame hitbox geometry");
	std::cout << "Native original model hitbox caches: " << model_cases << " models, 25 queries, " << query_hits << " hits\n";
	const auto saved_camera = camera;
	camera.pos = {};
	camera.orient = vmd_identity_matrix;
	object moving = *ConsoleObject;
	moving.orient = vmd_identity_matrix;
	unsigned wrapper_cases = 0, wrapper_hits = 0;
	for (int profile = 0; profile < 3; ++profile) {
		moving.type = profile ? OBJ_WEAPON : OBJ_PLAYER;
		moving.id = profile == 1 ? GAUSS_ID : VULCAN_ID;
		moving.size = profile ? F1_0 / 8 : ConsoleObject->size;
		for (int step = 0; step < 25; ++step) {
			const int frame = 100 + profile * 25 + step;
			vms_vector start{ -16 * F1_0, 0, 0 }, end{ (step - 12) * F1_0 / 2, 0, 0 }, hit{}, normal{};
			const auto result = resources.camera_hitbox_query(camera, moving, start, end, frame, hit, normal);
			xl_compat::FixedLightPoint expected_hit{}, expected_normal{};
			int16_t model = -1;
			const xl_compat::FixedLightPoint p0{ start.x, start.y, start.z }, p1{ end.x, end.y, end.z };
			auto expected = profile ? xl_compat::hitbox_vector_query(expected_hit, expected_normal, model, resources.model_hitboxes(resources.camera_model_id()), identity, {}, frame, 1, p0, p1, moving.size) : xl_compat::hitbox_pair_query(expected_hit, expected_normal, model, resources.model_hitboxes(moving.rtype.pobj_info.model_num), identity, resources.model_hitboxes(resources.camera_model_id()), identity, {}, frame, 1, p0, p1);
			if (expected == INT32_MAX) expected = 0;
			require(result == expected, "Native camera wrapper changed qualified geometry result");
			xl_hitbox_begin_frame();
			vms_vector bridge_hit{}, bridge_normal{};
			const int bridge_result = xl_camera_hitbox_query(&camera, &moving, &start, &end, &bridge_hit, &bridge_normal);
			require(bridge_result == result && bridge_hit.x == hit.x && bridge_hit.y == hit.y && bridge_hit.z == hit.z && bridge_normal.x == normal.x && bridge_normal.y == normal.y && bridge_normal.z == normal.z,
			        "Installed camera query bridge changed contact geometry");
			if (result) {
				require(hit.x == expected_hit[0] && hit.y == expected_hit[1] && hit.z == expected_hit[2] && normal.x == expected_normal[0] && normal.y == expected_normal[1] && normal.z == expected_normal[2], "Native camera wrapper changed contact geometry");
				++wrapper_hits;
			}
			++wrapper_cases;
		}
		for (int side : { -1, 1 }) {
			vms_vector start{ side * 100 * F1_0, 0, 0 }, end{ side * 101 * F1_0, 0, 0 }, hit{ 1, 2, 3 }, normal{ 4, 5, 6 };
			require(resources.camera_hitbox_query(camera, moving, start, end, 200 + profile * 2 + side, hit, normal) == 0 && hit.x == 1 && hit.y == 2 && hit.z == 3 && normal.x == 4 && normal.y == 5 && normal.z == 6, "Camera broadphase did not reject a distant finite segment without publishing a hit");
			++wrapper_cases;
		}
	}
	require(wrapper_hits > 0, "Native camera query wrapper produced no contacts");
	{
		vms_vector start{ -16 * F1_0, 0, 0 }, end{ 16 * F1_0, 0, 0 }, hit{}, normal{};
		resources.install_hxm();
		const int initial = xl_camera_hitbox_query(&camera, &moving, &start, &end, &hit, &normal);
		require(initial != 0, "Camera frame lifecycle requires an initial contact");
		const auto original_hit = hit;
		camera.pos.x = 3 * F1_0;
		require(xl_camera_hitbox_query(&camera, &moving, &start, &end, &hit, &normal) == initial && hit.x == original_hit.x, "Camera query cache did not retain same-tick geometry");
		xl_camera_controller_begin_frame();
		require(xl_camera_hitbox_query(&camera, &moving, &start, &end, &hit, &normal) != 0 && hit.x == original_hit.x + 3 * F1_0, "Camera physics tick did not refresh geometry");
		replacement_resources->install_hxm();
		bool rejected = false;
		try {
			resources.camera_hitbox_query(camera, moving, start, end, hit, normal);
		} catch (const std::exception &) {
			rejected = true;
		}
		require(rejected, "Replaced resource owner retained native query authority");
		require(xl_camera_hitbox_query(&camera, &moving, &start, &end, &hit, &normal) != 0, "Replacement resource owner cannot query camera");
		replacement_resources.reset();
		rejected = false;
		try {
			resources.camera_hitbox_query(camera, moving, start, end, hit, normal);
		} catch (const std::exception &) {
			rejected = true;
		}
		require(rejected, "Destroying installed resources revived an older owner");
		resources.install_hxm();
		require(xl_camera_hitbox_query(&camera, &moving, &start, &end, &hit, &normal) != 0, "Explicit reinstall did not restore query authority");
	}
	camera = saved_camera;
	std::cout << "Native camera query ownership: 75 bridge comparisons, tick/replacement/destruction/reinstall passed\n";
	std::cout << "Native camera query wrapper: " << wrapper_cases << " cases, " << wrapper_hits << " hits\n";
	{
		unsigned pairs = 0, traversals = 0, contacts = 0;
		for (int a = 0; a <= 16; ++a)
			for (int b = 0; b <= 16; ++b) {
				const int other = a == 16 ? b : a;
				const int expected = a != 16 && b != 16 ? -1 : (other == 1 || other == 2 || other == 4 || other == 5);
				require(xl_camera_pair_collision(a, b) == expected, "Camera collision eligibility differs from source InitCollision");
				++pairs;
			}
		const auto saved_player = *ConsoleObject;
		const auto saved_target = camera;
		obj_relink(victim, room);
		camera.pos = center;
		camera.orient = vmd_identity_matrix;
		for (int profile = 0; profile < 3; ++profile) {
			ConsoleObject->type = profile ? OBJ_WEAPON : OBJ_PLAYER;
			ConsoleObject->id = profile == 1 ? GAUSS_ID : profile == 2 ? VULCAN_ID
			                                                           : Player_num;
			ConsoleObject->size = profile ? F1_0 / 8 : saved_player.size;
			ConsoleObject->orient = vmd_identity_matrix;
			for (int step = -8; step <= 8; ++step) {
				vms_vector start = center, end = center, hit{}, normal{};
				start.x -= 7 * F1_0;
				end.x += step * F1_0 / 2;
				ConsoleObject->pos = start;
				xl_hitbox_begin_frame();
				const int expected = xl_camera_query(&camera, ConsoleObject, &start, &end, ConsoleObject->size, &hit, &normal);
				xl_compat::FixedLightPoint sphere_hit{};
				const int sphere_result = xl_compat::model_sphere_query(sphere_hit, { start.x, start.y, start.z }, { end.x, end.y, end.z }, { camera.pos.x, camera.pos.y, camera.pos.z }, camera.size + ConsoleObject->size);
				require(expected == (sphere_result < 0 ? 0 : sphere_result), "Native default camera query did not select the source sphere profile");
				if (sphere_result >= 0) require(hit.x == sphere_hit[0] && hit.y == sphere_hit[1] && hit.z == sphere_hit[2] && normal.x == 0 && normal.y == 0 && normal.z == 0, "Native default camera sphere contact differs from qualified source");
				fvi_query query{};
				query.p0 = &start;
				query.p1 = &end;
				query.startseg = room;
				query.rad = ConsoleObject->size;
				query.thisobjnum = int(ConsoleObject - Objects);
				query.flags = FQ_CHECK_OBJS | FQ_GET_SEGLIST;
				fvi_info result{};
				const int type = find_vector_intersection(&query, &result);
				require(type == (expected ? HIT_OBJECT : HIT_NONE), "Native camera traversal changed hitbox hit/miss");
				if (expected) {
					require(result.hit_object == victim && !std::memcmp(&result.hit_pnt, &hit, sizeof(hit)) &&
					            !std::memcmp(&result.hit_wallnorm, &normal, sizeof(normal)),
					        "Native camera traversal lost contact point or normal");
					++contacts;
				}
				int ignored[] = { victim, -1 };
				query.ignore_obj_list = ignored;
				require(find_vector_intersection(&query, &result) == HIT_NONE, "Native camera traversal ignored its ignore list");
				query.ignore_obj_list = nullptr;
				query.flags = FQ_GET_SEGLIST;
				require(find_vector_intersection(&query, &result) == HIT_NONE, "Native camera traversal checked objects without its flag");
				traversals += 3;
			}
		}
		require(contacts > 0, "Native camera traversal produced no contacts");
		*ConsoleObject = saved_player;
		obj_relink(victim, saved_target.segnum);
		camera = saved_target;
		std::cout << "Native default sphere camera FVI: " << pairs << " eligibility pairs, " << traversals << " traversals, " << contacts << " contacts passed\n";
	}
	camera_physics_native_controls(camera);
	camera_projectile_physics_controls(camera);
	camera_passive_physics_controls(camera);
	camera_player_contact_controls(camera);
	static_camera_native_controls(camera);
	const auto saved_frame = FrameTime;
	FrameTime = 1092;
	camera_controller_native_controls(root, camera);
	camera_rotation_native_controls(root);
	std::ofstream motion_output(root / "camera-motion.txt");
	unsigned motion_cases = 0;
	for (int clip : { VCLIP_MORPHING_ROBOT, VCLIP_POWERUP_DISAPPEARANCE, VCLIP_AFTERBURNER_BLOB, VCLIP_SMALL_EXPLOSION }) {
		auto *effect = object_create_explosion(camera.segnum, &camera.pos, 65537, clip);
		require(effect != nullptr, "Cannot create explosion motion clip control");
		xl_explosion_motion_begin(effect);
		motion_cases += camera_motion_native_controls(motion_output, *effect);
		const int slot = int(effect - Objects), signature = effect->signature;
		obj_delete(slot);
		auto *reused = object_create_explosion(camera.segnum, &camera.pos, 65537, clip);
		require(reused == &Objects[slot] && reused->signature != signature, "Cannot exercise explosion slot reuse");
		const auto position = reused->pos;
		reused->lifeleft /= 2;
		do_explosion_sequence(reused);
		require(!std::memcmp(&position, &reused->pos, sizeof(position)), "Reused fireball inherited prior motion");
		obj_delete(slot);
	}
	unsigned cases = 0;
	for (int id : { LASER_ID_L1, FUSION_ID, GAUSS_ID, CONCUSSION_ID, HOMING_ID, GUIDEDMISS_ID, MERCURY_ID })
		for (bool reverse : { false, true })
			for (bool harmless : { false, true })
				for (bool exploding : { false, true }) {
					const int shot = projectile_test_shot(ConsoleObject - Objects, id);
					auto &weapon = Objects[shot];
					camera.flags = exploding ? OF_EXPLODING : 0;
					camera.shields = 100 * F1_0;
					camera.mtype.phys_info.flags = 0;
					camera.ctype.ai_info.SKIP_AI_COUNT = 0;
					camera.mtype.phys_info.rotthrust = { 0, 0, 0 };
					weapon.flags = harmless ? OF_HARMLESS : 0;
					weapon.mtype.phys_info.velocity = { 100001, -65537, 17 };
					Num_awareness_events = 0;
					ConsoleObject->mtype.phys_info.velocity = {};
					ConsoleObject->mtype.phys_info.rotvel = {};
					ConsoleObject->shields = Players[Player_num].shields = 1000 * F1_0;
					Players[Player_num].flags &= ~PLAYER_FLAGS_INVULNERABLE;
					auto impact = camera.pos;
					const bool splash = Weapon_info[id].damage_radius != 0;
					const auto expected_impact = splash ? xl_compat::camera_splash_impact(
					                                          { impact.x, impact.y, impact.z }, { camera.pos.x, camera.pos.y, camera.pos.z }, camera.size, weapon.size)
					                                    : xl_compat::FixedLightPoint{ impact.x, impact.y, impact.z };
					const auto reflected = xl_compat::camera_reflect_velocity({ 100001, -65537, 17 }, weapon.mtype.phys_info.mass, harmless || exploding);
					std::array<int, MAX_OBJECTS> signatures{};
					const int old_highest = Highest_object_index;
					for (int i = 0; i <= old_highest; ++i) signatures[i] = Objects[i].type == OBJ_NONE ? -1 : Objects[i].signature;
					d_srand(1234);
					if (!exploding) d_rand(); // Actual awareness agitation draw
					int expected_skip = 0;
					std::array<std::int32_t, 3> expected_thrust{};
					if (id == GAUSS_ID && !exploding)
						xl_compat::camera_gauss_spin(expected_skip, FrameTime, expected_thrust, [] { return d_rand(); });
					unsigned expected_rng = 0;
					d_rand_get_state(&expected_rng);
					d_srand(1234);
					const auto calls = d_rand_get_call_count();
					collide_two_objects(reverse ? &weapon : &camera, reverse ? &camera : &weapon, &impact);
					unsigned actual_rng = 0;
					d_rand_get_state(&actual_rng);
					require(actual_rng == expected_rng && d_rand_get_call_count() - calls == unsigned(exploding ? 0 : id == GAUSS_ID ? 4
					                                                                                                                 : 1),
					        "Camera weapon contact RNG ordering differs");
					unsigned flashes = 0, attached = 0;
					for (int i = 0; i <= Highest_object_index; ++i) {
						if (Objects[i].type == OBJ_NONE || (i <= old_highest && signatures[i] == Objects[i].signature)) continue;
						require(Objects[i].type == OBJ_FIREBALL && Objects[i].id == Weapon_info[id].robot_hit_vclip &&
						            Objects[i].size == Weapon_info[id].impact_size &&
						            Objects[i].pos.x == expected_impact[0] && Objects[i].pos.y == expected_impact[1] && Objects[i].pos.z == expected_impact[2],
						        "Camera weapon impact did not use the source weapon flash position/size");
						if (Objects[i].flags & OF_ATTACHED) {
							require(Objects[i].ctype.expl_info.attach_parent == victim, "Camera weapon flash attached to wrong owner");
							++attached;
						}
						++flashes;
						motion_cases += camera_motion_native_controls(motion_output, Objects[i]);
						obj_delete(i);
					}
					require(flashes == unsigned(!exploding) + unsigned(splash) && attached == unsigned(!exploding) &&
					            Num_awareness_events == int(!exploding),
					        "Camera weapon effect gates differ");
					if (splash) {
						const int strength = Weapon_info[id].strength[Difficulty_level];
						const auto sample = xl_compat::camera_splash_sample({ center.x, center.y, center.z }, expected_impact, strength, Weapon_info[id].damage_radius, strength);
						const auto velocity = xl_compat::camera_force_velocity({}, sample.impulse, ConsoleObject->mtype.phys_info.mass, false);
						const int damage = Difficulty_level ? sample.damage : sample.damage / 4;
						require(Players[Player_num].shields == 1000 * F1_0 - damage && ConsoleObject->mtype.phys_info.velocity.x == velocity[0] &&
						            ConsoleObject->mtype.phys_info.velocity.y == velocity[1] && ConsoleObject->mtype.phys_info.velocity.z == velocity[2],
						        "Camera weapon splash lost source radial player damage or force");
					}
					require(camera.shields == 100 * F1_0 && camera.ctype.ai_info.SKIP_AI_COUNT == expected_skip &&
					            camera.mtype.phys_info.rotthrust.x == expected_thrust[0] && camera.mtype.phys_info.rotthrust.y == expected_thrust[1] &&
					            camera.mtype.phys_info.rotthrust.z == expected_thrust[2] && bool(camera.mtype.phys_info.flags & PF_USES_THRUST) == bool(expected_skip),
					        "Camera weapon damage or Gauss state differs");
					require(weapon.mtype.phys_info.velocity.x == reflected[0] && weapon.mtype.phys_info.velocity.y == reflected[1] &&
					            weapon.mtype.phys_info.velocity.z == reflected[2],
					        "Camera weapon reflection/harmless gate differs");
					if (id == FUSION_ID) {
						require(weapon.shields == 15 * F1_0 && !(weapon.flags & OF_SHOULD_BE_DEAD) && xl_projectile_has_hit(shot, victim),
						        "Camera fusion persistence/strength consumption differs");
						const auto unchanged = weapon;
						collide_two_objects(&camera, &weapon, &camera.pos);
						require(!std::memcmp(&unchanged, &weapon, sizeof(weapon)) && Num_awareness_events == int(!exploding),
						        "Repeated camera hit escaped persistent history");
					} else require(weapon.flags & OF_SHOULD_BE_DEAD, "Camera contact did not kill nonpersistent projectile");
					obj_delete(shot);
					++cases;
				}
	// Source stores persistent identity before testing parent signature
	{
		std::map<int, int> original;
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type != OBJ_NONE) original.emplace(i, int{ Objects[i].signature });
		const int beam = projectile_test_shot(int(ConsoleObject - Objects), OMEGA_ID);
		auto &omega = Objects[beam];
		require(omega.movement_type == MT_NONE && !omega.mtype.phys_info.mass, "Production camera Omega fixture is physical");
		camera.flags = 0;
		omega.mtype.phys_info.velocity = { 100001, -65537, 17 };
		collide_two_objects(&camera, &omega, &camera.pos);
		require(omega.mtype.phys_info.velocity.x == 100001 && omega.mtype.phys_info.velocity.y == -65537 &&
		            omega.mtype.phys_info.velocity.z == 17 && camera.control_type == 18 && camera.movement_type == MT_NONE,
		        "Nonphysical Omega changed ordinary camera contact motion");
		for (int i = Highest_object_index; i >= 0; --i)
			if (Objects[i].type != OBJ_NONE && (!original.count(i) || original.at(i) != Objects[i].signature)) obj_delete(i);
		std::cout << "Native original Omega nonphysical camera contact passed\n";
	}
	const int shot = projectile_test_shot(ConsoleObject - Objects);
	auto &weapon = Objects[shot];
	weapon.ctype.laser_info.parent_signature = camera.signature;
	const auto shields = weapon.shields;
	Num_awareness_events = 0;
	collide_two_objects(&camera, &weapon, &camera.pos);
	require(xl_projectile_has_hit(shot, victim) && weapon.shields == shields && !Num_awareness_events && !(weapon.flags & OF_SHOULD_BE_DEAD),
	        "Camera parent signature gate lost source ordering");
	obj_delete(shot);
	motion_output.close();
	require(bool(motion_output), "Cannot write camera motion receipt");
	std::cout << "Native camera explosion motion samples: " << motion_cases << "\n";
	camera_radial_robot_controls(root, camera);
	camera_missile_native_controls(camera);
	camera_glass_native_controls(camera);
	camera_flash_native_controls(camera);
	const auto stock_smart = Weapon_info[SMART_ID];
	Weapon_info[SMART_ID].flash = 5;
	Weapon_info[SMART_ID].damage_radius = 25 * F1_0;
	for (int difficulty = 0; difficulty < NDL; ++difficulty) Weapon_info[SMART_ID].strength[difficulty] = 40 * F1_0;
	xl_camera_validate_weapon_definitions();
	camera_flash_native_controls(camera, SMART_ID);
	Weapon_info[SMART_ID] = stock_smart;
	const auto stock_gauss = Weapon_info[GAUSS_ID];
	const auto stock_child = Weapon_info[41];
	Weapon_info[GAUSS_ID].children = 41;
	Weapon_info[GAUSS_ID].flash = 2;
	Weapon_info[GAUSS_ID].damage_radius = 11 * F1_0;
	for (int difficulty = 0; difficulty < NDL; ++difficulty) Weapon_info[GAUSS_ID].strength[difficulty] = 54 * F1_0;
	Weapon_info[41].children = -1;
	Weapon_info[41].damage_radius = 0;
	Weapon_info[41].flash = 0;
	Weapon_info[41].homing_flag = 0;
	Weapon_info[41].bounce = 1;
	for (int difficulty = 0; difficulty < NDL; ++difficulty) Weapon_info[41].strength[difficulty] = F1_0;
	for (int difficulty = 0; difficulty < NDL; ++difficulty) Weapon_info[41].speed[difficulty] = 750 * F1_0;
	xl_camera_validate_weapon_definitions();
	camera_children_native_controls(camera, true);
	camera_flash_native_controls(camera, GAUSS_ID);
	Weapon_info[GAUSS_ID] = stock_gauss;
	Weapon_info[41] = stock_child;
	camera_mine_chain_controls(camera);
	// Earn the state used by the caller's full disk restore through real physics
	const auto hit_player = *ConsoleObject;
	camera.ctype.ai_info.behavior = 128;
	camera.flags = 0;
	ConsoleObject->pos = camera.pos;
	ConsoleObject->pos.x -= 5 * F1_0;
	ConsoleObject->last_pos = ConsoleObject->pos;
	ConsoleObject->mtype.phys_info = {};
	ConsoleObject->mtype.phys_info.mass = F1_0;
	ConsoleObject->mtype.phys_info.velocity.x = 32 * F1_0;
	FrameTime = F1_0 / 8;
	xl_camera_hit_state_created(int(ConsoleObject - Objects));
	const auto hit_ticks = SDL_GetTicks();
	if (hit_ticks < 500) SDL_Delay(500 - hit_ticks);
	xl_camera_controller_begin_frame();
	do_physics_sim(ConsoleObject);
	xl_compat::CameraHitState earned_hit;
	require(xl_camera_hit_state_get(int(ConsoleObject - Objects), earned_hit) && earned_hit.repaired == int32_t(xl_camera_controller_ticks()) &&
	            earned_hit.shield_time > 0 && earned_hit.times != decltype(earned_hit.times){},
	        "Cannot earn camera hit state for full disk restore");
	*ConsoleObject = hit_player;
	const int robot_owner = projectile_robot();
	auto &hit_robot = Objects[robot_owner];
	hit_robot.pos = camera.pos;
	hit_robot.pos.x -= F1_0;
	hit_robot.last_pos = hit_robot.pos;
	hit_robot.mtype.phys_info = {};
	hit_robot.mtype.phys_info.mass = F1_0;
	hit_robot.mtype.phys_info.velocity.x = 32 * F1_0;
	ConsoleObject->type = OBJ_NONE; // Isolate the robot/camera contact from the test player
	do_physics_sim(&hit_robot);
	*ConsoleObject = hit_player;
	require(xl_camera_hit_state_get(robot_owner, earned_hit) && earned_hit.repaired == int32_t(xl_camera_controller_ticks()) &&
	            earned_hit.shield_time > 0,
	        "Cannot earn robot camera hit state for full disk restore");
	camera_children_native_controls(camera);
	obj_delete(victim);
	FrameTime = saved_frame;
	std::cout << "Native camera projectile contacts: " << cases
	          << " primary/missile cases, real XL history, duplicate and parent-signature ordering, attached weapon effects, signed RNG and health passed\n";
}
