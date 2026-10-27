#include "blocked_fixture.h"
#include "blocked.h"
extern "C" void move_object_to_legal_spot(object *);
static void blocked_matrix(const std::filesystem::path &output)
{
	std::ofstream samples(output / "blocked-passability.txt");
	const auto saved = Segments[60];
	vms_vector center;
	compute_segment_center(&center, &Segments[60]);
	const int actor = obj_create(OBJ_ROBOT, 0, 60, &center, &vmd_identity_matrix, F1_0, CT_NONE, MT_NONE, RT_NONE);
	require(actor >= 0, "Blocked matrix allocation failed");
	unsigned count = 0;
	for (int bits = 0; bits < 16; ++bits) {
		Segments[60].children[0] = 40 + bits;
		for (int type : std::array<int, 9>{ -1, OBJ_PLAYER, OBJ_ROBOT, OBJ_POWERUP, OBJ_WEAPON, OBJ_DEBRIS, OBJ_HOSTAGE, OBJ_CNTRLCEN, OBJ_CLUTTER })
			for (int flags = 0; flags < 8; ++flags)
				for (int has_wall : { 0, 1 }) {
					if (type >= 0) Objects[actor].type = static_cast<object_type_t>(type);
					const int passable = xl_blocked_entry(type < 0 ? -1 : actor, 60, 0) ? WID_RENDER_FLAG : has_wall ? flags
					                                                                                                 : WID_FLY_FLAG | WID_RENDPAST_FLAG;
					samples << bits << ' ' << type << ' ' << flags << ' ' << has_wall << ' ' << passable << '\n';
					++count;
				}
	}
	Objects[actor].type = OBJ_ROBOT;
	obj_delete(actor);
	Segments[60] = saved;
	require(xl_blocked_map_color(60, 123) == BM_XRGB(13, 13, 13) && xl_blocked_map_color(22, 123) == 123 && xl_blocked_map_color(61, 123) == 123, "Blocked automap color lost source function/ordinary priority");
	std::cout << count << " blocked passability samples written\n";
}
static void blocked_crossings(const std::filesystem::path &output)
{
	const int blocked = 60;
	int side = -1;
	for (int i = 0; i < 6; ++i) {
		const int child = Segments[blocked].children[i];
		if (child >= 0 && !xl_blocked_segment(child) && Segments[blocked].sides[i].wall_num < 0) side = i;
	}
	require(side >= 0, "Blocked fixture has no dry open portal");
	const int dry = Segments[blocked].children[side], back = find_connect_side(&Segments[blocked], &Segments[dry]);
	require(back >= 0, "Blocked portal is not reciprocal");
	std::ofstream manifest(output / "missions/blocked-fixture.json");
	manifest << nlohmann::json({ { "blocked", blocked }, { "side", side }, { "dry", dry }, { "back", back } }).dump(4) << '\n';
	vms_vector face;
	compute_center_point_on_side(&face, &Segments[blocked], side);
	const auto normal = Segments[blocked].sides[side].normals[0];
	for (const auto type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_POWERUP, OBJ_WEAPON, OBJ_DEBRIS }) {
		const int index = type == OBJ_PLAYER ? ConsoleObject - Objects : obj_create(type, 0, dry, &face, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_PHYSICS, RT_NONE);
		require(index >= 0, "Blocked crossing allocation failed");
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
		for (int entering : { 0, 1 }) {
			obj_relink(index, entering ? dry : blocked);
			vm_vec_scale_add(&actor.pos, &face, &normal, entering ? -F1_0 / 2 : F1_0 / 2);
			vm_vec_copy_scale(&actor.mtype.phys_info.velocity, &normal, entering ? i2f(4) : -i2f(4));
			actor.flags = 0;
			FrameTime = F1_0 / 4;
			vms_vector target;
			vm_vec_scale_add(&target, &actor.pos, &actor.mtype.phys_info.velocity, FrameTime);
			fvi_query query{};
			query.p0 = &actor.pos;
			query.p1 = &target;
			query.startseg = actor.segnum;
			query.rad = actor.size;
			query.thisobjnum = index;
			query.flags = FQ_TRANSWALL;
			fvi_info hit{};
			const bool denied = entering && (type == OBJ_PLAYER || type == OBJ_ROBOT || type == OBJ_POWERUP);
			require((find_vector_intersection(&query, &hit) == HIT_WALL) == denied, "Actor-specific FVI allowed a forbidden entry or blocked exit/projectile");
			query.thisobjnum = -1;
			query.rad = 0;
			require(find_vector_intersection(&query, &hit) == HIT_NONE, "Blocked property obstructed an actorless visibility query");
			object_move_one(&actor);
			require(actor.segnum == (denied ? dry : entering ? blocked
			                                                 : dry),
			        "Actual actor physics ignored blocked entry/exit rules");
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
	require(xl_blocked_projectile_entries() > 0, "Actual projectile transition was not recorded");
	// No-clip retains the source's explicit blocked-region bypass
	const int player = ConsoleObject - Objects;
	const auto saved = *ConsoleObject;
	obj_relink(player, dry);
	vm_vec_scale_add(&ConsoleObject->pos, &face, &normal, -F1_0 / 2);
	ConsoleObject->size = F1_0 / 8;
	ConsoleObject->control_type = CT_NONE;
	ConsoleObject->movement_type = MT_PHYSICS;
	ConsoleObject->mtype.phys_info = {};
	ConsoleObject->mtype.phys_info.mass = F1_0;
	vm_vec_copy_scale(&ConsoleObject->mtype.phys_info.velocity, &normal, i2f(4));
	cheats.ghostphysics = 1;
	object_move_one(ConsoleObject);
	cheats.ghostphysics = 0;
	require(ConsoleObject->segnum == blocked, "No-clip could not enter blocked region");
	const int segment = ConsoleObject->segnum, previous = ConsoleObject->prev, next = ConsoleObject->next;
	*ConsoleObject = saved;
	ConsoleObject->segnum = segment;
	ConsoleObject->prev = previous;
	ConsoleObject->next = next;
	obj_relink(player, saved.segnum);
	// Recovery must not move a robot across the blocked portal even with no alternate side
	const auto dry_segment = Segments[dry];
	for (int i = 0; i < 6; ++i)
		if (i != back) Segments[dry].children[i] = -1;
	vms_vector position;
	vm_vec_scale_add(&position, &face, &normal, -F1_0 / 4);
	const int robot = obj_create(OBJ_ROBOT, 0, dry, &position, &vmd_identity_matrix, F1_0, CT_AI, MT_PHYSICS, RT_NONE);
	require(robot >= 0, "Blocked recovery allocation failed");
	Objects[robot].shields = i2f(100);
	move_object_to_legal_spot(&Objects[robot]);
	require(Objects[robot].segnum != blocked && Objects[robot].shields < 0, "Robot legal-position recovery entered blocked destination");
	obj_delete(robot);
	Segments[dry] = dry_segment;
	std::cout << "Blocked portal " << dry << ':' << back << " -> " << blocked << ':' << side << " actual actor crossings, rays, projectiles, recovery and no-clip passed\n";
}

static void blocked_lifecycle(const std::filesystem::path &output)
{
	const int player = ConsoleObject - Objects;
	obj_relink(player, 61);
	compute_segment_center(&ConsoleObject->pos, &Segments[61]);
	ConsoleObject->mtype.phys_info.velocity = {};
	Game_suspended = 0;
	require(!check_trigger_sub(0, Player_num, 0), "Blocked producer activation failed");
	FrameTime = i2f(9);
	fuelcen_update_all();
	FrameTime = 1;
	fuelcen_update_all();
	FrameTime = Vclip[VCLIP_MORPHING_ROBOT].play_time / 2;
	fuelcen_update_all();
	int robot = -1, item = -1;
	for (int i = 0; i <= Highest_object_index; ++i) {
		if (Objects[i].type == OBJ_ROBOT && Objects[i].matcen_creator != -1) robot = i;
		if (Objects[i].type == OBJ_POWERUP && Objects[i].segnum == 23) item = i;
	}
	require(robot >= 0 && Objects[robot].segnum == 22 && xl_blocked_segment(22) && xl_lava_segment(22) && item >= 0 && xl_preview_water_segment(23), "Blocked property suppressed authored actor generation");
	const auto robot_signature = Objects[robot].signature;
	char save[] = "blocked-earned.sav", description[] = "XL blocked regions";
	stop_time();
	require(state_save_all_sub(save, description), "Blocked disk save failed");
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Blocked stock transition failed");
	const auto stock = *ConsoleObject;
	require(!xl_blocked_segment(60) && !xl_blocked_entry(ConsoleObject - Objects, ConsoleObject->segnum, 0) && xl_blocked_map_color(ConsoleObject->segnum, 123) == 123 && !std::memcmp(&stock, ConsoleObject, sizeof(stock)), "Blocked behavior leaked into Redux");
	require(state_restore_all_sub(save, 0) && xl_blocked_segment(60) && !xl_blocked_segment(61) && Objects[robot].signature == robot_signature && Objects[robot].segnum == 22 && xl_blocked_segment(Objects[item].segnum), "Blocked disk restoration through stock lost properties or generated actors");
	start_time();
	// A real closed wall still rejects a projectile even though blocked volumes alone permit it
	std::ifstream manifest_file(output / "missions/blocked-fixture.json");
	const auto manifest = nlohmann::json::parse(manifest_file);
	const int dry = manifest.at("dry"), back = manifest.at("back"), side = manifest.at("side");
	const auto previous_side = Segments[dry].sides[back];
	const int wall = Num_walls;
	require(wall < MAX_WALLS, "Missing closed-wall control slot");
	const auto previous_wall = Walls[wall];
	Walls[wall] = {};
	Walls[wall].type = WALL_CLOSED;
	Walls[wall].segnum = dry;
	Walls[wall].sidenum = back;
	Segments[dry].sides[back].wall_num = wall;
	vms_vector face;
	compute_center_point_on_side(&face, &Segments[60], side);
	const auto normal = Segments[60].sides[side].normals[0];
	vms_vector start, end;
	vm_vec_scale_add(&start, &face, &normal, -F1_0 / 2);
	vm_vec_scale_add(&end, &face, &normal, F1_0 / 2);
	const int shot = obj_create(OBJ_WEAPON, 0, dry, &start, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_PHYSICS, RT_NONE);
	require(shot >= 0, "Closed-wall projectile allocation failed");
	fvi_query query{};
	query.p0 = &start;
	query.p1 = &end;
	query.startseg = dry;
	query.thisobjnum = shot;
	query.rad = F1_0 / 8;
	fvi_info hit{};
	require(find_vector_intersection(&query, &hit) == HIT_WALL, "Blocked projectile exemption bypassed a real closed wall");
	obj_delete(shot);
	Segments[dry].sides[back] = previous_side;
	Walls[wall] = previous_wall;
	std::cout << "Blocked mixed producers, disk source restoration, stock isolation and closed-wall negative control passed\n";
}
static int blocked_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto output = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		write_blocked_fixture(output / "missions");
		require(load_base_mission("blocked", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		require(!xl_level_supported("neptunxl.rl2", 1), "Blocked fixture admitted multiplayer");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		blocked_matrix(output);
		blocked_crossings(output);
		blocked_lifecycle(output);
		std::cout << "XL blocked segment controls passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "XL blocked failed: " << e.what() << '\n';
		return 1;
	}
}
