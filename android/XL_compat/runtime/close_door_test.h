#include "close_door_fixture.h"
#include "door_lifecycle_test.h"
extern "C" int is_door_free(segment *, int);
static void close_door_matrix(std::ostream &out)
{
	const auto front = Walls[0], back = Walls[1];
	const auto clip = WallAnims[front.clip_num];
	const auto player = *ConsoleObject;
	const int saved_count = Num_open_doors;
	std::array<active_door, MAX_DOORS> saved_doors;
	std::memcpy(saved_doors.data(), ActiveDoors, sizeof(ActiveDoors));
	for (int state = 0; state < 8; ++state)
		for (int mode = 0; mode < 4; ++mode)
			for (int elapsed : { 0, F1_0 / 4, F1_0, 2 * F1_0, 5 * F1_0, -F1_0 / 4 })
				for (int play : { F1_0 / 4, F1_0, 2 * F1_0, 10 * F1_0 })
					for (int blocked : { 0, 1 })
						for (int full : { 0, 1 }) {
							Walls[0] = front;
							Walls[1] = back;
							Walls[0].state = Walls[1].state = state;
							WallAnims[front.clip_num].play_time = play;
							for (auto &entry : ActiveDoors) {
								entry = {};
								for (int i = 0; i < 2; ++i) entry.front_wallnum[i] = entry.back_wallnum[i] = -1;
							}
							Num_open_doors = full ? MAX_DOORS : mode ? 1
							                                         : 0;
							if (mode) {
								auto &entry = ActiveDoors[Num_open_doors - 1];
								entry.time = elapsed;
								entry.n_parts = mode == 3 ? 2 : 1;
								if (mode == 2) entry.back_wallnum[0] = 0;
								else entry.front_wallnum[mode == 3 ? 1 : 0] = 0;
							}
							obj_relink(ConsoleObject - Objects, blocked ? 24 : 0);
							compute_segment_center(&ConsoleObject->pos, &Segments[ConsoleObject->segnum]);
							ConsoleObject->size = F1_0 / 8;
							if (blocked) {
								compute_center_point_on_side(&ConsoleObject->pos, &Segments[24], 5);
								vm_vec_scale_add2(&ConsoleObject->pos, &Segments[24].sides[5].normals[0], F1_0 / 16);
							}
							require(bool(is_door_free(&Segments[24], 5)) == !blocked, "Close-door obstruction fixture disagrees");
							require(xl_authored_close_door(24, 5) == 1, "XL close action selected stock fallback");
							out << state << ' ' << mode << ' ' << elapsed << ' ' << play << ' ' << blocked << ' ' << full << ' ' << int(Walls[0].state) << ' ' << int(Walls[1].state) << ' ' << Num_open_doors << ' ' << (Num_open_doors ? ActiveDoors[Num_open_doors - 1].time : -1) << '\n';
						}
	Walls[0] = front;
	Walls[1] = back;
	WallAnims[front.clip_num] = clip;
	Num_open_doors = saved_count;
	std::memcpy(ActiveDoors, saved_doors.data(), sizeof(ActiveDoors));
	const int seg = ConsoleObject->segnum, previous = ConsoleObject->prev, next = ConsoleObject->next;
	*ConsoleObject = player;
	ConsoleObject->segnum = seg;
	ConsoleObject->prev = previous;
	ConsoleObject->next = next;
	obj_relink(ConsoleObject - Objects, player.segnum);
	std::cout << "3072 close-door transition/allocation/obstruction samples written\n";
}
static void close_door_lifecycle()
{
	obj_relink(ConsoleObject - Objects, 0);
	compute_segment_center(&ConsoleObject->pos, &Segments[0]);
	ConsoleObject->mtype.phys_info.velocity = {};
	FrameTime = F1_0 / 32;
	wall_open_door(&Segments[24], 5);
	for (int i = 0; i < 512 && Walls[0].state != WALL_DOOR_WAITING; ++i) wall_frame_process();
	require(Walls[0].state == WALL_DOOR_WAITING && Num_open_doors == 1, "Automatic door never entered waiting");
	std::cout << "Door clip=" << int(Walls[0].clip_num) << " play_ticks=" << WallAnims[Walls[0].clip_num].play_time << '\n';
	// Stay below the normal five-second automatic deadline
	for (int i = 0; i < 48; ++i) wall_frame_process();
	require(Walls[0].state == WALL_DOOR_WAITING, "Door left waiting before authored close");
	const auto waiting = ActiveDoors[0];
	// Block on either side with real actor geometry; do not overwrite earned state
	for (auto type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_POWERUP, OBJ_MARKER, OBJ_WEAPON, OBJ_FIREBALL })
		for (int from : { 24, 21 }) {
			vms_vector face;
			const int side = from == 24 ? 5 : 4;
			compute_center_point_on_side(&face, &Segments[from], side);
			vm_vec_scale_add2(&face, &Segments[from].sides[side].normals[0], F1_0 / 16);
			const int index = obj_create(type, 0, from, &face, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_NONE, RT_NONE);
			require(index >= 0, "Door obstruction allocation failed");
			Walls[0].state = Walls[1].state = WALL_DOOR_WAITING;
			ActiveDoors[0] = waiting;
			require(xl_authored_close_door(24, 5) == 1, "Obstruction action selected stock");
			const bool ignored = type == OBJ_WEAPON || type == OBJ_FIREBALL;
			require((Walls[0].state == WALL_DOOR_CLOSING) == ignored, "Two-sided actor obstruction differs");
			if (!ignored) require(ActiveDoors[0].time == waiting.time, "Blocked action changed earned waiting time");
			obj_delete(index);
		}
	Walls[0].state = Walls[1].state = WALL_DOOR_WAITING;
	ActiveDoors[0] = waiting;
	const auto earned_wall = Walls[0];
	xl_authored_close_door(-1, 0);
	xl_authored_close_door(24, 6);
	Walls[0].clip_num = -1;
	xl_authored_close_door(24, 5);
	Walls[0].clip_num = earned_wall.clip_num;
	require(Walls[0].state == WALL_DOOR_WAITING && ActiveDoors[0].time == waiting.time, "Malformed close mutated earned state");
	ActiveDoors[0].time = INT32_MIN;
	xl_authored_close_door(24, 5);
	require(Walls[0].state == WALL_DOOR_WAITING && ActiveDoors[0].time == INT32_MIN, "Overflowing close timer partially committed");
	ActiveDoors[0] = waiting;
	char save[] = "close-earned.sav", description[] = "XL waiting close";
	stop_time();
	require(state_save_all_sub(save, description), "Waiting door save failed");
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Close-door stock transition failed");
	const auto stock = Walls[0];
	Walls[0].state = WALL_DOOR_WAITING;
	require(xl_authored_close_door(stock.segnum, stock.sidenum) == 0, "Close-door action leaked into stock");
	wall_close_door(&Segments[stock.segnum], stock.sidenum);
	require(Walls[0].state == WALL_DOOR_WAITING, "Redux waiting-close behavior changed");
	Walls[0] = stock;
	require(state_restore_all_sub(save, 0) && Walls[0].state == WALL_DOOR_WAITING && Num_open_doors == 1 && ActiveDoors[0].time == waiting.time, "Waiting door state lost through stock");
	start_time();
	require(check_trigger_sub(1, Player_num, 1) == 0 && Walls[0].state == WALL_DOOR_CLOSING && Walls[1].state == WALL_DOOR_CLOSING && Num_open_doors == 1, "Authored waiting close failed");
	require(ActiveDoors[0].time == std::max<fix>(0, WallAnims[Walls[0].clip_num].play_time - waiting.time), "Forced closing timer differs from source");
	const auto closing = ActiveDoors[0];
	stop_time();
	require(state_save_all_sub(save, description), "Closing door save failed");
	wall_open_door(&Segments[24], 5);
	require(state_restore_all_sub(save, 0) && Walls[0].state == WALL_DOOR_CLOSING && ActiveDoors[0].time == closing.time, "Closing restore lost active timer");
	start_time();
	FrameTime = F1_0 / 32;
	for (int i = 0; i < 512 && Walls[0].state != WALL_DOOR_CLOSED; ++i) {
		wall_frame_process();
		doorway_rays(WALL_IS_DOORWAY(&Segments[24], 5));
	}
	require(Walls[0].state == WALL_DOOR_CLOSED && Num_open_doors == 0, "Forced close animation did not finish");
	std::cout << "Authored waiting close, twelve actor obstructions, actual animation/rays, saves and Redux isolation passed\n";
}
static int close_door_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto output = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		write_close_door_fixture(output / "missions");
		require(load_base_mission("close", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0) && !xl_level_supported("neptunxl.rl2", 1), "Close-door admission mismatch");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		std::ofstream samples(output / "close-door-samples.txt");
		close_door_matrix(samples);
		std::ofstream animation_samples(output / "door-animation-samples.txt");
		door_animation_matrix(animation_samples);
		close_door_lifecycle();
		door_pair_variants();
		door_concurrency(output);
		std::cout << "XL close-door controls passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "XL close-door failed: " << e.what() << '\n';
		return 1;
	}
}
