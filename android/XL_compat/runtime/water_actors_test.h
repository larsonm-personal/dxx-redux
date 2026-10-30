#include "water_actors_fixture.h"
extern "C" void collide_robot_and_weapon(object *, object *, vms_vector *);

static void water_actor_contacts()
{
	const int wet = 60;
	int portal = -1, wall = -1;
	for (int side = 0; side < 6; ++side) {
		const int child = Segments[wet].children[side];
		if (child >= 0 && !xl_preview_water_segment(child) && Segments[wet].sides[side].wall_num == -1) portal = side;
		if (child == -1 && Segments[wet].sides[side].wall_num == -1) wall = side;
	}
	require(portal >= 0 && wall >= 0, "wet contact fixture lacks a dry portal or solid wall");
	const int dry = Segments[wet].children[portal];
	vms_vector face;
	compute_center_point_on_side(&face, &Segments[wet], portal);
	const auto normal = Segments[wet].sides[portal].normals[0];
	for (const auto type : { OBJ_ROBOT, OBJ_WEAPON }) {
		const int index = obj_create(type, 0, wet, &face, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_PHYSICS, RT_NONE);
		require(index >= 0, "water contact actor allocation failed");
		auto &actor = Objects[index];
		actor.mtype.phys_info = {};
		actor.mtype.phys_info.mass = F1_0;
		actor.ctype.laser_info.parent_type = OBJ_PLAYER;
		actor.ctype.laser_info.parent_num = Players[Player_num].objnum;
		actor.ctype.laser_info.parent_signature = ConsoleObject->signature;
		for (int direction : { 1, -1 }) {
			obj_relink(index, direction == 1 ? wet : dry);
			vm_vec_scale_add(&actor.pos, &face, &normal, direction * F1_0 / 2);
			vm_vec_copy_scale(&actor.mtype.phys_info.velocity, &normal, -direction * i2f(4));
			const auto before = actor.pos, velocity = actor.mtype.phys_info.velocity;
			FrameTime = F1_0 / 4;
			do_physics_sim(&actor);
			vms_vector expected;
			vm_vec_scale_add(&expected, &before, &velocity, direction == 1 ? 3 * F1_0 / 16 : F1_0 / 4);
			require(actor.segnum == (direction == 1 ? dry : wet) && !std::memcmp(&expected, &actor.pos, sizeof(expected)), "water portal did not use the starting medium for its complete frame");
			const auto after = actor.pos;
			do_physics_sim(&actor);
			vm_vec_scale_add(&expected, &after, &velocity, direction == 1 ? F1_0 / 4 : 3 * F1_0 / 16);
			require(!std::memcmp(&expected, &actor.pos, sizeof(expected)), "water crossing retained the previous medium on its next frame");
		}
		if (type == OBJ_WEAPON) {
			compute_center_point_on_side(&face, &Segments[wet], wall);
			const auto inward = Segments[wet].sides[wall].normals[0];
			obj_relink(index, wet);
			vm_vec_scale_add(&actor.pos, &face, &inward, actor.size + F1_0 / 16);
			const auto before = actor.pos;
			vm_vec_copy_scale(&actor.mtype.phys_info.velocity, &inward, -i2f(4));
			do_physics_sim(&actor);
			vms_vector moved, expected;
			vm_vec_sub(&moved, &actor.pos, &before);
			vm_vec_copy_scale(&expected, &moved, i2f(3));
			require((actor.flags & OF_SHOULD_BE_DEAD) && !std::memcmp(&expected, &actor.mtype.phys_info.velocity, sizeof(expected)), "wet projectile wall contact lost termination or reconstructed velocity");
		}
		obj_delete(index);
	}
}

static void water_actor_combat(int robot)
{
	auto &actor = Objects[robot];
	require(!Robot_info[actor.id].attack_type && Robot_info[actor.id].n_guns > 0, "authored robot is not a projectile combat control");
	obj_relink(robot, 60);
	compute_segment_center(&actor.pos, &Segments[60]);
	obj_relink(Players[Player_num].objnum, 60);
	ConsoleObject->pos = actor.pos;
	actor.pos.z -= i2f(10);
	ConsoleObject->pos.z += i2f(10);
	require(!get_seg_masks(&actor.pos, 60, actor.size, nullptr, 0).sidemask && !get_seg_masks(&ConsoleObject->pos, 60, ConsoleObject->size, nullptr, 0).sidemask, "water combat poses do not fit");
	actor.orient = vmd_identity_matrix;
	actor.mtype.phys_info.velocity = {};
	actor.ctype.ai_info.behavior = AIB_NORMAL;
	actor.ctype.ai_info.CURRENT_STATE = actor.ctype.ai_info.GOAL_STATE = AIS_FIRE;
	auto &local = Ai_local_info[robot];
	local.mode = AIM_CHASE_OBJECT;
	local.next_fire = 0;
	local.player_awareness_type = PA_PLAYER_COLLISION;
	local.player_awareness_time = i2f(10);
	Players[Player_num].flags &= ~(PLAYER_FLAGS_CLOAKED | PLAYER_FLAGS_INVULNERABLE);
	Game_suspended = 0;
	FrameTime = F1_0 / 60;
	int shot = -1;
	for (int frame = 0; frame < 600 && shot < 0; ++frame) {
		GameTime64 += FrameTime;
		++d_tick_count;
		Believed_player_pos = ConsoleObject->pos;
		do_ai_frame_all();
		do_ai_frame(&actor);
		do_physics_sim(&actor);
		for (int i = 0; i <= Highest_object_index; ++i)
			if (Objects[i].type == OBJ_WEAPON && Objects[i].ctype.laser_info.parent_num == robot && Objects[i].ctype.laser_info.parent_signature == actor.signature) {
				shot = i;
				break;
			}
	}
	if (shot < 0) std::cerr << "AI witness: segment=" << actor.segnum << " mode=" << local.mode << " visibility=" << local.previous_visibility << " state=" << int(actor.ctype.ai_info.CURRENT_STATE) << " goal=" << int(actor.ctype.ai_info.GOAL_STATE) << " next_fire=" << local.next_fire << " guns=" << int(Robot_info[actor.id].n_guns) << " attack=" << int(Robot_info[actor.id].attack_type) << " weapon=" << int(Robot_info[actor.id].weapon_type) << " suspended=" << Game_suspended << " dead=" << Player_is_dead << '\n';
	require(shot >= 0 && local.next_fire > 0, "generated wet robot never fired through its ordinary AI frame");
	const auto shields = Players[Player_num].shields;
	collide_player_and_weapon(ConsoleObject, &Objects[shot], &ConsoleObject->pos);
	require(Players[Player_num].shields < shields, "wet robot projectile did not damage the player");
	const auto robot_shields = actor.shields;
	vms_vector direction;
	vm_vec_normalized_dir(&direction, &actor.pos, &ConsoleObject->pos);
	const int laser = Laser_create_new_easy(&direction, &ConsoleObject->pos, Players[Player_num].objnum, LASER_ID_L1, 0);
	require(laser >= 0, "wet player laser allocation failed");
	collide_robot_and_weapon(&actor, &Objects[laser], &actor.pos);
	require(actor.shields < robot_shields, "wet robot ignored ordinary player weapon damage");
	std::cout << "Wet generated robot completed AI firing and bidirectional weapon damage\n";
}

static int water_actors_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		write_water_actors_fixture(root);
		require(load_base_mission("wetbots", error, sizeof(error)), error);
		auto *custom = PHYSFS_openWrite("neptunxl.hxm");
		require(custom && PHYSFS_close(custom), "cannot prepare custom-definition admission control");
		require(!xl_level_supported("neptunxl.rl2", 0) && std::strstr(xl_level_rejection(), "HXM"), "water actor support bypassed custom-definition admission");
		require(PHYSFS_delete("neptunxl.hxm"), "cannot remove custom-definition admission control");
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		require(!xl_level_supported("neptunxl.rl2", 1), "water actors admitted multiplayer");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_level_extensions_active() && xl_equipment_count() == 2 && Num_robot_centers == 1 && xl_preview_water_segment(22), "combined water producer fixture missing");
		const auto equipment = [](int index) { xl_equipment_state state; require(xl_equipment_state_get(index, &state), "missing wet equipment state"); return state; };
		const auto items = [](int segment) {
			std::vector<int> result;
			for (int index = Segments[segment].objects; index >= 0; index = Objects[index].next)
				if (Objects[index].type == OBJ_POWERUP) result.push_back(index);
			return result;
		};
		Game_suspended = 0;
		Difficulty_level = 2;
		require(!check_trigger_sub(0, Player_num, 0), "wet producer activation failed");
		FrameTime = i2f(9);
		fuelcen_update_all();
		require(!equipment(0).pending && equipment(0).timer == i2f(9) - 1, "water scaled the global equipment clock");
		FrameTime = 1;
		fuelcen_update_all();
		require(equipment(0).pending && equipment(1).pending, "wet producer initial boundary failed");
		char pending[] = "water-pending.sav", description[] = "XL water actors";
		stop_time();
		require(state_save_all_sub(pending, description), "wet pending producer save failed");
		require(!game_is_time_paused(), "Native save/restore left the simulation paused");
		FrameTime = Vclip[VCLIP_MORPHING_ROBOT].play_time / 2;
		fuelcen_update_all();
		require(items(20).size() == 1 && items(23).size() == 1, "wet item generation missing");
		int robot = -1;
		for (int index = 0; index <= Highest_object_index; ++index)
			if (Objects[index].type == OBJ_ROBOT && Objects[index].matcen_creator != -1) {
				robot = index;
				break;
			}
		require(robot >= 0 && Objects[robot].id == 0 && Objects[robot].segnum == 22 && Objects[robot].control_type == CT_MORPH, "wet robot generator did not create its authored robot");
		FrameTime = F1_0 / 60;
		for (int frame = 0; frame < 600 && Objects[robot].control_type == CT_MORPH; ++frame) do_morph_frame(&Objects[robot]);
		require(Objects[robot].control_type == CT_AI, "wet robot did not complete morphing");
		char earned[] = "water-actors.sav";
		stop_time();
		require(state_save_all_sub(earned, description), "water actor save failed");
		require(!game_is_time_paused(), "Native save/restore left the simulation paused");
		std::ofstream samples(root.parent_path() / "water-actor-motion.txt");
		unsigned count = 0;
		for (const auto type : { OBJ_ROBOT, OBJ_WEAPON, OBJ_POWERUP }) {
			vms_vector center;
			compute_segment_center(&center, &Segments[60]);
			const int index = obj_create(type, 0, 60, &center, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_PHYSICS, RT_NONE);
			require(index >= 0, "water motion actor allocation failed");
			auto &actor = Objects[index];
			actor.shields = i2f(100);
			actor.mtype.phys_info = {};
			actor.mtype.phys_info.mass = F1_0;
			for (int segment : { 60, 61 })
				for (fix time : { F1_0 / 1000, F1_0 / 60, F1_0 / 20, F1_0 / 4 })
					for (const vms_vector velocity : { vms_vector{ F1_0, 0, 0 }, vms_vector{ -F1_0, F1_0 / 3, -F1_0 / 7 }, vms_vector{ 1001, -2003, 3007 } }) {
						obj_relink(index, segment);
						compute_segment_center(&actor.pos, &Segments[segment]);
						const auto position = actor.pos;
						actor.mtype.phys_info.velocity = velocity;
						actor.mtype.phys_info.thrust = {};
						actor.mtype.phys_info.rotvel = {};
						FrameTime = time;
						const auto random_calls = d_rand_get_stream_call_count(D_RNG_SIM);
						do_physics_sim(&actor);
						require(actor.segnum == segment && !std::memcmp(&velocity, &actor.mtype.phys_info.velocity, sizeof(velocity)), "unobstructed fluid motion changed velocity or left test room");
						require(!xl_preview_water_frame(index) && d_rand_get_stream_call_count(D_RNG_SIM) == random_calls, "robot/item/projectile acquired player-only turbulence");
						samples << type << ' ' << bool(xl_preview_water_segment(segment)) << ' ' << time << ' ' << velocity.x << ' ' << velocity.y << ' ' << velocity.z << ' ' << actor.pos.x - position.x << ' ' << actor.pos.y - position.y << ' ' << actor.pos.z - position.z << '\n';
						++count;
					}
			obj_delete(index);
		}
		require(state_restore_all_sub(earned, 0) && xl_preview_water_segment(Objects[robot].segnum) && Objects[robot].control_type == CT_AI, "wet generated robot restore failed");
		water_actor_contacts();
		water_actor_combat(robot);
		Players[Player_num].energy = i2f(50);
		auto &energy = Objects[items(20)[0]];
		collide_player_and_powerup(ConsoleObject, &energy, &energy.pos);
		require(Players[Player_num].energy > i2f(50) && (energy.flags & OF_SHOULD_BE_DEAD), "wet generated energy did not grant the ordinary pickup reward");
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !xl_equipment_count() && !xl_preview_water_segment(22), "water actor state leaked into Redux");
		require(state_restore_all_sub(earned, 0) && xl_preview_water_segment(22) && xl_equipment_count() == 2 && Objects[robot].id == 0 && Objects[robot].control_type == CT_AI, "water/robot/item disk restoration through Redux failed");
		require(state_restore_all_sub(pending, 0) && equipment(0).pending && equipment(1).pending && items(20).empty() && items(23).empty(), "wet pending production did not restore");
		FrameTime = Vclip[VCLIP_MORPHING_ROBOT].play_time / 2;
		fuelcen_update_all();
		require(items(20).size() == 1 && items(23).size() == 1, "wet pending production did not resume after restore");
		std::cout << "XL water actor controls passed: " << count << " motion samples and authored mixed production\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL water actors failed: " << error.what() << '\n';
		return 1;
	}
}
