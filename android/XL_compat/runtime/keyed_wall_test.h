#include "doorway.h"
#include "keyed_wall_fixture.h"
static void keyed_wall_matrix(const std::filesystem::path &output)
{
	std::ofstream samples(output / "keyed-wall-samples.txt");
	const auto saved_wall = Walls[0];
	auto &side = Segments[Walls[0].segnum].sides[Walls[0].sidenum];
	const auto saved_side = side;
	side.tmap_num = 0;
	side.tmap_num2 = 0;
	auto &bitmap = GameBitmaps[Textures[0].index];
	const auto saved_bitmap_flags = bitmap.bm_flags;
	const auto saved_keys = Players[0].flags;
	vms_vector center;
	compute_segment_center(&center, &Segments[Walls[0].segnum]);
	const int index = obj_create(OBJ_ROBOT, 0, Walls[0].segnum, &center, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
	require(index >= 0, "Keyed-wall matrix allocation failed");
	unsigned count = 0;
	for (int type : std::array<int, 9>{ -1, OBJ_PLAYER, OBJ_ROBOT, OBJ_POWERUP, OBJ_WEAPON, OBJ_DEBRIS, OBJ_HOSTAGE, OBJ_CNTRLCEN, OBJ_CLUTTER })
		for (int keys = 0; keys < 256; ++keys)
			for (int owned = 0; owned < 16; owned += 2)
				for (int cloak : { 0, 1, 9, 30, 31, 255 })
					for (int transparent : { 0, 1 })
						for (int flags : { 0, WALL_DOOR_OPENED, WALL_DOOR_LOCKED, WALL_ILLUSION_OFF }) {
							// Other raw bytes share the exact-key decision; keep the broad
							// existing rendering matrix without repeating it 240 times
							if (keys >= 16 && (cloak || transparent || flags)) continue;
							if (type >= 0) Objects[index].type = static_cast<object_type_t>(type);
							Objects[index].id = 0;
							Players[0].flags = owned;
							Walls[0].keys = keys;
							Walls[0].cloak_value = cloak;
							Walls[0].flags = flags;
							bitmap.bm_flags = transparent ? BM_FLAG_TRANSPARENT : 0;
							const int actual = xl_wall_doorway(type < 0 ? -1 : index, Walls[0].segnum, Walls[0].sidenum);
							samples << type << ' ' << keys << ' ' << owned << ' ' << cloak << ' ' << transparent << ' ' << flags << ' ' << actual << '\n';
							++count;
						}
	Objects[index].type = OBJ_ROBOT;
	obj_delete(index);
	bitmap.bm_flags = saved_bitmap_flags;
	side = saved_side;
	Walls[0] = saved_wall;
	Players[0].flags = saved_keys;
	require(xl_wall_doorway(ConsoleObject - Objects, -1, 0) == -1 && xl_wall_doorway(ConsoleObject - Objects, 24, 6) == -1 && xl_wall_doorway(Highest_object_index + 1, 24, 5) == WID_TRANSPARENT_WALL, "Keyed-wall invalid index handling failed");
	std::cout << count << " keyed-wall passability samples written\n";
}
static void keyed_wall_hit_matrix(const std::filesystem::path &output)
{
	std::ofstream samples(output / "key-hit-samples.txt");
	const auto saved = Walls[0];
	const auto owned = Players[Player_num].flags;
	const auto velocity = ConsoleObject->mtype.phys_info.velocity;
	ConsoleObject->mtype.phys_info.velocity = {};
	unsigned count = 0;
	for (int key = 0; key < 256; ++key)
		for (int flags = 0; flags < 16; flags += 2)
			for (int profile = 0; profile < 3; ++profile) {
				Walls[0].keys = key;
				Walls[0].type = profile ? WALL_DOOR : WALL_NORMAL;
				Walls[0].flags = profile == 1 ? WALL_DOOR_LOCKED : 0;
				Walls[0].state = profile == 2 ? WALL_DOOR_OPENING : WALL_DOOR_CLOSED;
				Players[Player_num].flags = flags;
				const int result = wall_hit_process(&Segments[saved.segnum], saved.sidenum, 0, Player_num, ConsoleObject);
				samples << key << ' ' << flags << ' ' << profile << ' ' << result << '\n';
				++count;
			}
	Walls[0] = saved;
	Players[Player_num].flags = owned;
	ConsoleObject->mtype.phys_info.velocity = velocity;
	require(bool(samples) && count == 6144, "Incomplete raw wall-key hit matrix");
	std::cout << count << " actual wall-key hit samples written\n";
}
static void keyed_wall_robot_matrix(const std::filesystem::path &output)
{
	std::ofstream samples(output / "key-robot-samples.txt");
	const auto saved = Walls[0];
	const auto owned = Players[Player_num].flags;
	vms_vector center;
	compute_segment_center(&center, &Segments[saved.segnum]);
	const int robot = obj_create(OBJ_ROBOT, ROBOT_BRAIN, saved.segnum, &center, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
	require(robot >= 0 && !Robot_info[ROBOT_BRAIN].companion, "Raw-key robot fixture lacks an ordinary brain");
	Walls[0].type = WALL_DOOR;
	Walls[0].state = WALL_DOOR_CLOSED;
	unsigned count = 0;
	for (int key = 0; key < 256; ++key)
		for (int flags : { 0, 1, 2, 4, 8, 14, 16, 32, 64, 128, 256, 65536 })
			for (int locked : { 0, 1 }) {
				Walls[0].keys = key;
				Walls[0].flags = locked ? WALL_DOOR_LOCKED : 0;
				Players[Player_num].flags = flags;
				samples << key << ' ' << flags << ' ' << locked << ' ' << ai_door_is_openable(&Objects[robot], &Segments[saved.segnum], saved.sidenum) << '\n';
				++count;
			}
	obj_delete(robot);
	Walls[0] = saved;
	Players[Player_num].flags = owned;
	require(bool(samples) && count == 6144, "Incomplete raw wall-key robot matrix");
	std::cout << count << " actual raw-key robot door samples written\n";
}
static void keyed_wall_crossings()
{
	unsigned count = 0;
	for (int wall_index = 0; wall_index < 6; ++wall_index) {
		const auto &wall = Walls[wall_index];
		const int from = wall.segnum, side = wall.sidenum, to = Segments[from].children[side];
		require(to >= 0, "Keyed fixture wall is not a portal");
		vms_vector face;
		compute_center_point_on_side(&face, &Segments[from], side);
		const auto normal = Segments[from].sides[side].normals[0];
		for (auto type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_POWERUP, OBJ_WEAPON }) {
			const int index = type == OBJ_PLAYER ? ConsoleObject - Objects : obj_create(type, 0, from, &face, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_PHYSICS, RT_NONE);
			require(index >= 0, "Keyed crossing allocation failed");
			auto &actor = Objects[index];
			const auto saved = actor;
			actor.size = F1_0 / 8;
			actor.control_type = CT_NONE;
			actor.movement_type = MT_PHYSICS;
			actor.mtype.phys_info = {};
			actor.mtype.phys_info.mass = F1_0;
			actor.shields = i2f(100);
			if (type == OBJ_WEAPON) {
				actor.ctype.laser_info.parent_num = ConsoleObject - Objects;
				actor.ctype.laser_info.parent_signature = ConsoleObject->signature;
				actor.ctype.laser_info.parent_type = OBJ_PLAYER;
			}
			for (int owned : { 0, PLAYER_FLAGS_BLUE_KEY, PLAYER_FLAGS_RED_KEY, PLAYER_FLAGS_GOLD_KEY, 14 }) {
				Players[Player_num].flags = owned;
				obj_relink(index, from);
				actor.flags = 0;
				actor.lifeleft = IMMORTAL_TIME;
				vm_vec_scale_add(&actor.pos, &face, &normal, F1_0 / 2);
				vm_vec_copy_scale(&actor.mtype.phys_info.velocity, &normal, -i2f(4));
				FrameTime = F1_0 / 4;
				const bool has_key = type == OBJ_PLAYER && ((wall.keys == KEY_BLUE && (owned & PLAYER_FLAGS_BLUE_KEY)) || (wall.keys == KEY_RED && (owned & PLAYER_FLAGS_RED_KEY)));
				const bool pass = has_key && !xl_blocked_segment(to);
				vms_vector end;
				vm_vec_scale_add(&end, &actor.pos, &actor.mtype.phys_info.velocity, FrameTime);
				fvi_query query{};
				query.p0 = &actor.pos;
				query.p1 = &end;
				query.startseg = from;
				query.thisobjnum = index;
				query.rad = actor.size;
				fvi_info hit{};
				require((find_vector_intersection(&query, &hit) == HIT_NONE) == pass, "Keyed-wall actor FVI differs from source key/block rules");
				query.thisobjnum = -1;
				query.rad = 0;
				query.flags = FQ_TRANSWALL;
				require(find_vector_intersection(&query, &hit) == HIT_NONE, "Keyed closed wall hid actorless transparent visibility");
				object_move_one(&actor);
				if (actor.segnum != (pass ? to : from)) std::cerr << "Keyed crossing wall=" << wall_index << " type=" << type << " keys=" << owned << " actual=" << actor.segnum << " expected=" << (pass ? to : from) << '\n';
				require(actor.segnum == (pass ? to : from), "Actual keyed-wall physics/containment disagreed");
				++count;
			}
			if (type == OBJ_PLAYER) {
				const int segment = actor.segnum, previous = actor.prev, next = actor.next;
				actor = saved;
				actor.segnum = segment;
				actor.prev = previous;
				actor.next = next;
				obj_relink(index, saved.segnum);
			} else obj_delete(index);
		}
	}
	// A malformed object player ID cannot read a foreign player slot
	const auto id = ConsoleObject->id;
	ConsoleObject->id = 255;
	require(xl_wall_doorway(ConsoleObject - Objects, 24, 5) == WID_TRANSPARENT_WALL, "Malformed player ID acquired a key");
	ConsoleObject->id = id;
	std::cout << count << " actual keyed-wall actor crossings and visibility rays passed\n";
}
static void keyed_wall_lifecycle()
{
	const int player = ConsoleObject - Objects;
	obj_relink(player, 24);
	compute_segment_center(&ConsoleObject->pos, &Segments[24]);
	ConsoleObject->mtype.phys_info.velocity = {};
	Players[Player_num].flags = PLAYER_FLAGS_BLUE_KEY;
	char save[] = "keyed-earned.sav", description[] = "XL keyed wall";
	stop_time();
	require(state_save_all_sub(save, description), "Keyed-wall disk save failed");
	Players[Player_num].flags = PLAYER_FLAGS_RED_KEY;
	require(!(xl_wall_doorway(player, 24, 5) & WID_FLY_FLAG), "Removing matching key did not close permission");
	const auto old_type = Walls[0].type;
	Walls[0].type = WALL_OPEN;
	require(xl_wall_doorway(player, 24, 5) == WID_NO_WALL && (WALL_IS_DOORWAY(&Segments[24], 5) & WID_FLY_FLAG), "Live wall transition retained closed-wall dispatch");
	Walls[0].type = old_type;
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Keyed-wall stock transition failed");
	const auto stock = *ConsoleObject;
	require(xl_wall_doorway(ConsoleObject - Objects, ConsoleObject->segnum, 0) == -1 && !std::memcmp(&stock, ConsoleObject, sizeof(stock)), "Keyed-wall rule leaked into Redux");
	require(Num_walls > 0, "Stock control lacks walls");
	const auto wall = Walls[0];
	Walls[0].type = WALL_CLOSED;
	Walls[0].flags = WALL_DOOR_OPENED;
	require(WALL_IS_DOORWAY(&Segments[wall.segnum], wall.sidenum) == WID_TRANSILLUSORY_WALL, "Redux doorway semantics changed");
	Walls[0] = wall;
	require(state_restore_all_sub(save, 0) && (Players[Player_num].flags & PLAYER_FLAGS_BLUE_KEY) && xl_wall_doorway(ConsoleObject - Objects, 24, 5) == WID_ILLUSORY_WALL && WALL_IS_DOORWAY(&Segments[24], 5) == WID_TRANSPARENT_WALL, "Disk restore lost keys or mixed actor/visibility state");
	require(Walls[6].keys == 255 && Walls[7].keys == 255, "Disk-through-stock restore changed raw wall-key bytes");
	require(!game_is_time_paused(), "Native save/restore left the simulation paused");
	// Owning a key does not bypass the source boost-area exit constraint
	obj_relink(ConsoleObject - Objects, 24);
	compute_segment_center(&ConsoleObject->pos, &Segments[24]);
	trigger boost{};
	boost.type = 15;
	boost.value = 1;
	boost.num_links = 1;
	boost.seg[0] = 21;
	boost.side[0] = 4;
	require(xl_speed_boost_action(&boost, -1, ConsoleObject - Objects, 1) == 0 && xl_speed_boost_exit_blocked(ConsoleObject - Objects, 24, 5), "Keyed boost-boundary setup failed");
	vms_vector face, start, end;
	compute_center_point_on_side(&face, &Segments[24], 5);
	const auto normal = Segments[24].sides[5].normals[0];
	vm_vec_scale_add(&start, &face, &normal, F1_0 / 2);
	vm_vec_scale_add(&end, &face, &normal, -F1_0 / 2);
	fvi_query query{};
	query.p0 = &start;
	query.p1 = &end;
	query.startseg = 24;
	query.thisobjnum = ConsoleObject - Objects;
	query.rad = F1_0 / 8;
	fvi_info hit{};
	require(find_vector_intersection(&query, &hit) == HIT_WALL, "Matching key bypassed active boost boundary");
	xl_speed_boost_stop(ConsoleObject - Objects);
	require(find_vector_intersection(&query, &hit) == HIT_NONE, "Key passage did not resume after boost release");
	std::cout << "Keyed-wall live permission/wall changes, disk through stock and Redux isolation passed\n";
}
static int keyed_wall_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto output = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		write_keyed_wall_fixture(output / "missions");
		require(load_base_mission("keywall", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		require(!xl_level_supported("neptunxl.rl2", 1), "Keyed-wall fixture admitted multiplayer");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(Num_walls > 7 && Walls[6].keys == 255 && Walls[7].keys == 255, "Ordinary loading changed raw wall-key bytes");
		keyed_wall_matrix(output);
		keyed_wall_hit_matrix(output);
		keyed_wall_robot_matrix(output);
		keyed_wall_crossings();
		for (int key : { 16, 128, 255 }) {
			const auto first = Walls[0].keys, second = Walls[1].keys;
			Walls[0].keys = Walls[1].keys = key;
			keyed_wall_crossings();
			Walls[0].keys = first;
			Walls[1].keys = second;
		}
		keyed_wall_lifecycle();
		std::cout << "XL keyed-wall controls passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "XL keyed-wall failed: " << e.what() << '\n';
		return 1;
	}
}
