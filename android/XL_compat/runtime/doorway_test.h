#include "doorway_fixture.h"
extern "C" int check_transparency(segment *, int);
static void doorway_sample(std::ostream &out, int index)
{
	const auto &w = Walls[index];
	out << int(w.type) << ' ' << int(w.flags) << ' ' << int(w.state) << ' ' << int(w.cloak_value) << ' ' << !!check_transparency(&Segments[w.segnum], w.sidenum) << ' ' << WALL_IS_DOORWAY(&Segments[w.segnum], w.sidenum) << '\n';
}
static void doorway_matrix(std::ostream &out)
{
	const auto saved_wall = Walls[0];
	auto &side = Segments[Walls[0].segnum].sides[Walls[0].sidenum];
	const auto saved_side = side;
	side.tmap_num = 0;
	side.tmap_num2 = 0;
	auto &bitmap = GameBitmaps[Textures[0].index];
	const auto saved_flags = bitmap.bm_flags;
	for (int type = 0; type <= 7; ++type)
		for (int flags : { 0, 1, 2, 3, 8, 16, 32, 255 })
			for (int state = 0; state < 8; ++state)
				for (int cloak : { 0, 1, 9, 30, 31, 127, 128, 255 })
					for (int transparent : { 0, 1 }) {
						Walls[0].type = type;
						Walls[0].flags = flags;
						Walls[0].state = state;
						Walls[0].cloak_value = cloak;
						bitmap.bm_flags = transparent ? BM_FLAG_TRANSPARENT : 0;
						doorway_sample(out, 0);
					}
	bitmap.bm_flags = saved_flags;
	side = saved_side;
	Walls[0] = saved_wall;
	std::cout << "8192 doorway type/flag/state/transparency samples written\n";
}
static void doorway_rays(int expected)
{
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
	require((find_vector_intersection(&query, &hit) == HIT_NONE) == bool(expected & WID_FLY_FLAG), "Doorway physical collision differs from flags");
	query.thisobjnum = -1;
	query.rad = 0;
	query.flags = FQ_TRANSWALL;
	require((find_vector_intersection(&query, &hit) == HIT_NONE) == bool(expected & WID_RENDPAST_FLAG), "Doorway visibility ray differs from flags");
}
static void doorway_animation(std::ostream &out)
{
	obj_relink(ConsoleObject - Objects, 0);
	compute_segment_center(&ConsoleObject->pos, &Segments[0]);
	ConsoleObject->mtype.phys_info.velocity = {};
	const auto sample = [&] {
		for (int index : { 0, 1 }) doorway_sample(out, index);
		const int flags = WALL_IS_DOORWAY(&Segments[24], 5);
		doorway_rays(flags);
		if (Walls[0].state == WALL_DOOR_CLOSING && !(Walls[0].flags & WALL_DOOR_OPENED)) require(flags == WID_TRANSPARENT_WALL, "Closing XL door lost transparency");
	};
	FrameTime = F1_0 / 32;
	require(check_trigger_sub(0, Player_num, 1) == 0, "Authored open-door action failed");
	require(Num_open_doors == 1 && Walls[0].state == WALL_DOOR_OPENING && Walls[1].state == WALL_DOOR_OPENING, "Door animation did not start both sides");
	for (int i = 0; i < 512 && Walls[0].state != WALL_DOOR_OPEN; ++i) {
		wall_frame_process();
		sample();
	}
	require(Walls[0].state == WALL_DOOR_OPEN && (Walls[0].flags & WALL_DOOR_OPENED), "Door opening failed");
	require(check_trigger_sub(1, Player_num, 1) == 0, "Authored close-door action failed");
	for (int i = 0; i < 512 && (Walls[0].flags & WALL_DOOR_OPENED); ++i) {
		wall_frame_process();
		sample();
	}
	require(Walls[0].state == WALL_DOOR_CLOSING && Num_open_doors == 1, "No closing/solid animation witness");
	const auto earned = ActiveDoors[0];
	char save[] = "door-earned.sav", description[] = "XL doorway";
	stop_time();
	require(state_save_all_sub(save, description), "Doorway disk save failed");
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Doorway stock transition failed");
	const auto stock_wall = Walls[0];
	Walls[0].type = WALL_CLOAKED;
	Walls[0].flags = WALL_DOOR_OPENED;
	require(xl_wall_doorway(-1, stock_wall.segnum, stock_wall.sidenum) == -1 && WALL_IS_DOORWAY(&Segments[stock_wall.segnum], stock_wall.sidenum) == WID_TRANSILLUSORY_WALL, "Redux doorway alternate changed");
	Walls[0] = stock_wall;
	require(state_restore_all_sub(save, 0) && Num_open_doors == 1 && ActiveDoors[0].time == earned.time && Walls[0].state == WALL_DOOR_CLOSING && WALL_IS_DOORWAY(&Segments[24], 5) == WID_TRANSPARENT_WALL, "Disk through stock lost closing doorway state");
	require(!game_is_time_paused(), "Native save/restore left the simulation paused");
	FrameTime = F1_0 / 32;
	sample();
	require(check_trigger_sub(0, Player_num, 1) == 0 && Walls[0].state == WALL_DOOR_CLOSING, "Open trigger unexpectedly reversed a closing door");
	// Source ToggleWall opens closed doors only; a player hit can reverse closing
	require(wall_hit_process(&Segments[24], 5, 0, Player_num, ConsoleObject) == WHP_DOOR, "Player hit did not reverse closing door");
	require(Num_open_doors == 1 && Walls[0].state == WALL_DOOR_OPENING, "Reversed door allocated another animation");
	for (int i = 0; i < 512 && Walls[0].state != WALL_DOOR_OPEN; ++i) {
		wall_frame_process();
		sample();
	}
	require(Walls[0].state == WALL_DOOR_OPEN, "Reversed door did not open");
	require(check_trigger_sub(1, Player_num, 1) == 0, "Authored final close-door action failed");
	for (int i = 0; i < 512 && Walls[0].state != WALL_DOOR_CLOSED; ++i) {
		wall_frame_process();
		sample();
	}
	require(Walls[0].state == WALL_DOOR_CLOSED && Num_open_doors == 0 && !(Walls[0].flags & WALL_DOOR_OPENED), "Final door closure retained passage");
	std::cout << "Actual door opening, closing, reversal, physical/visibility rays and disk through stock passed\n";
}
static int doorway_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto output = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		write_doorway_fixture(output / "missions");
		require(load_base_mission("doorway", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		require(!xl_level_supported("neptunxl.rl2", 1), "Doorway fixture admitted multiplayer");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		std::ofstream samples(output / "doorway-samples.txt");
		doorway_matrix(samples);
		doorway_animation(samples);
		std::cout << "XL doorway controls passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "XL doorway failed: " << e.what() << '\n';
		return 1;
	}
}
