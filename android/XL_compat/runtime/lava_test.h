#include "lava_fixture.h"
#include "fluids.h"
extern "C" {
#include "automap.h"
}
static void lava_contacts(const std::filesystem::path &output)
{
	std::ofstream samples(output / "lava-contacts.txt");
	const int player = ConsoleObject - Objects;
	const auto saved_texture = TmapInfo[0];
	unsigned count = 0;
	const int bits[] = { 2, 0, 3, 10, 11, 1, 8, 9 };
	for (int offset = 0; offset < 8; ++offset) {
		const int segment = 60 + offset;
		const auto saved = Segments[segment];
		for (int mode = 0; mode < 4; ++mode)
			for (int contact = 0; contact < 8; ++contact)
				for (fix time : { 1, F1_0 / 1000, F1_0 / 60, F1_0 / 4, F1_0 })
					for (int difficulty : { 0, 2 })
						for (int thrust : { 0, 1 })
							for (int invulnerable : { 0, 1 }) {
								Segments[segment] = saved;
								const int texture = contact == 2 || contact == 7 ? 1 : contact == 0 ? 0
								                                                                    : 2;
								const bool solid = contact < 3;
								const int walltype = contact == 4 || contact == 5 ? WALL_ILLUSION : contact == 6 ? WALL_CLOSED
								                                                                                 : -1;
								const int wallflags = contact == 5 ? WALL_ILLUSION_OFF : 0;
								const int testwall = Num_walls;
								require(testwall < MAX_WALLS, "No spare lava test wall");
								const auto saved_wall = Walls[testwall];
								Walls[testwall].type = static_cast<ubyte>(walltype);
								Walls[testwall].flags = wallflags;
								for (auto &side : Segments[segment].sides) {
									side.tmap_num = 0;
									side.tmap_num2 = 0;
									side.wall_num = walltype < 0 ? -1 : testwall;
								}
								for (auto &child : Segments[segment].children) child = solid ? -1 : 61;
								TmapInfo[0].damage = texture == 1 ? i2f(16) : 0;
								TmapInfo[0].flags = texture == 2 ? TMI_WATER : 0;
								obj_relink(player, segment);
								compute_segment_center(&ConsoleObject->pos, &Segments[segment]);
								if (mode == 3) {
									compute_center_point_on_side(&ConsoleObject->pos, &Segments[segment], 0);
									vm_vec_scale_add2(&ConsoleObject->pos, &Segments[segment].sides[0].normals[0], ConsoleObject->size / 2);
								}
								const int mask = get_seg_masks(&ConsoleObject->pos, segment, ConsoleObject->size, nullptr, 0).sidemask;
								require(mode == 3 ? mask != 0 : mask == 0, "Lava contact fixture pose classification differs");
								ConsoleObject->mtype.phys_info.velocity = {};
								ConsoleObject->mtype.phys_info.rotvel = {};
								ConsoleObject->mtype.phys_info.thrust = { thrust ? F1_0 : 0, 0, 0 };
								const fix mass = count % 11 == 0 ? 0 : count % 7 == 0 ? F1_0 / 3
								                                                      : Player_ship->mass;
								ConsoleObject->mtype.phys_info.mass = mass;
								ConsoleObject->movement_type = count % 13 == 0 ? MT_NONE : MT_PHYSICS;
								Automap_active = count % 17 == 0;
								Players[Player_num].shields = i2f(100);
								Players[Player_num].flags = invulnerable ? PLAYER_FLAGS_INVULNERABLE : 0;
								PaletteRedAdd = PaletteGreenAdd = PaletteBlueAdd = 0;
								Difficulty_level = difficulty;
								FrameTime = time;
								GameTime64 = i2f(10);
								const fix64 intervals[] = { 0, F1_0 / 4, F1_0, i2f(2), -1 };
								const auto elapsed = intervals[count % 5];
								require(xl_fluid_restore(elapsed, 1), "Cannot prepare XL scrape clock");
								const auto &normal = Segments[segment].sides[0].normals[0];
								samples << (mode == 3 ? 0 : mode) << ' ' << bits[offset] << ' ' << texture << ' ' << solid << ' ' << walltype << ' ' << wallflags << ' ' << thrust << ' ' << invulnerable << ' ' << difficulty << ' ' << time << ' ' << elapsed << ' ' << mass << ' ' << Automap_active << ' ' << int(ConsoleObject->movement_type) << ' ' << mask << ' ' << TmapInfo[404].damage << ' ' << TmapInfo[0].damage << ' ' << normal.x << ' ' << normal.y << ' ' << normal.z;
								d_srand(1234 + count);
								unsigned random_state = 0;
								d_rand_get_state(&random_state);
								for (int i = 0; i < 16; ++i) samples << ' ' << d_rand();
								d_rand_set_state(random_state);
								const auto before = d_rand_get_stream_call_count(D_RNG_SIM);
								int type = 0, playing = 0;
								if (mode == 0 || mode == 3) type = xl_fluid_frame(player, &playing);
								else if (mode == 1) type = xl_fluid_wall(player, segment, 0);
								else {
									const int32_t point[] = { ConsoleObject->pos.x, ConsoleObject->pos.y, ConsoleObject->pos.z };
									type = xl_fluid_scrape(player, segment, 0, point);
								}
								const auto &rotation = ConsoleObject->mtype.phys_info.rotvel;
								const auto &velocity = ConsoleObject->mtype.phys_info.velocity;
								samples << ' ' << type << ' ' << i2f(100) - Players[Player_num].shields << ' ' << PaletteRedAdd << ' ' << rotation.x << ' ' << rotation.z << ' ' << velocity.x << ' ' << velocity.y << ' ' << velocity.z << ' ' << xl_fluid_scrape_elapsed() << ' ' << d_rand_get_stream_call_count(D_RNG_SIM) - before << '\n';
								Walls[testwall] = saved_wall;
								++count;
							}
		Segments[segment] = saved;
	}
	TmapInfo[0] = saved_texture;
	Automap_active = 0;
	ConsoleObject->movement_type = MT_PHYSICS;
	ConsoleObject->mtype.phys_info.mass = Player_ship->mass;
	std::cout << count << " lava contact samples written\n";
}

static void lava_motion(const std::filesystem::path &output)
{
	std::ofstream samples(output / "lava-motion.txt");
	const int properties[] = { 2, 0, 3, 10, 11, 1, 8, 9 };
	for (const auto type : { OBJ_PLAYER, OBJ_ROBOT, OBJ_WEAPON, OBJ_POWERUP }) {
		vms_vector center;
		compute_segment_center(&center, &Segments[60]);
		const int index = obj_create(type, 0, 60, &center, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_PHYSICS, RT_NONE);
		require(index >= 0, "Lava motion actor allocation failed");
		auto &actor = Objects[index];
		actor.shields = i2f(100);
		actor.mtype.phys_info = {};
		actor.mtype.phys_info.mass = F1_0;
		for (int segment = 60; segment < 68; ++segment)
			for (fix time : { F1_0 / 1000, F1_0 / 60, F1_0 / 20, F1_0 / 4 })
				for (const vms_vector velocity : { vms_vector{ F1_0, 0, 0 }, vms_vector{ -F1_0, F1_0 / 3, -F1_0 / 7 }, vms_vector{ 1001, -2003, 3007 } }) {
					obj_relink(index, segment);
					compute_segment_center(&actor.pos, &Segments[segment]);
					const auto before = actor.pos;
					actor.mtype.phys_info.velocity = velocity;
					FrameTime = time;
					do_physics_sim(&actor);
					require(actor.segnum == segment && !std::memcmp(&velocity, &actor.mtype.phys_info.velocity, sizeof(velocity)), "Free lava actor motion changed velocity or left room");
					samples << type << ' ' << properties[segment - 60] << ' ' << time << ' ' << velocity.x << ' ' << velocity.y << ' ' << velocity.z << ' ' << actor.pos.x - before.x << ' ' << actor.pos.y - before.y << ' ' << actor.pos.z - before.z << '\n';
					if (type != OBJ_PLAYER) {
						const auto calls = d_rand_get_stream_call_count(D_RNG_SIM);
						object_move_one(&actor);
						require(actor.shields == i2f(100) && d_rand_get_stream_call_count(D_RNG_SIM) == calls, "Nonplayer lava frame applied damage/turbulence");
					}
				}
		obj_delete(index);
	}

	// Include ordinary nonzero drag and thrust, not only unaccelerated displacement
	for (const auto type : { OBJ_ROBOT, OBJ_WEAPON, OBJ_POWERUP }) {
		vms_vector center;
		compute_segment_center(&center, &Segments[60]);
		const int index = obj_create(type, 0, 60, &center, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_PHYSICS, RT_NONE);
		require(index >= 0, "Lava force allocation failed");
		auto &actor = Objects[index];
		for (int segment : { 60, 61, 62, 65 })
			for (fix time : { F1_0 / 1000, F1_0 / 60, F1_0 / 20, F1_0 / 4 })
				for (int thrust : { 0, 1, 2 })
					for (int flags : { 0, PF_USES_THRUST }) {
						obj_relink(index, segment);
						compute_segment_center(&actor.pos, &Segments[segment]);
						const auto before = actor.pos;
						actor.mtype.phys_info = {};
						auto &physics = actor.mtype.phys_info;
						physics.mass = Player_ship->mass;
						physics.drag = Player_ship->drag;
						physics.flags = flags;
						physics.velocity = { -F1_0, F1_0 / 3, -F1_0 / 7 };
						const auto initial = physics.velocity;
						physics.thrust = { thrust == 1 ? F1_0 : thrust == 2 ? -F1_0
							                                                : 0,
							               thrust ? F1_0 / 3 : 0, thrust ? -F1_0 / 7 : 0 };
						FrameTime = time;
						do_physics_sim(&actor);
						require(actor.segnum == segment, "Fluid thrust control left room");
						samples << type << ' ' << properties[segment - 60] << ' ' << time << ' ' << initial.x << ' ' << initial.y << ' ' << initial.z << ' ' << actor.pos.x - before.x << ' ' << actor.pos.y - before.y << ' ' << actor.pos.z - before.z << ' ' << flags << ' ' << physics.mass << ' ' << physics.drag << ' ' << physics.thrust.x << ' ' << physics.thrust.y << ' ' << physics.thrust.z << ' ' << physics.velocity.x << ' ' << physics.velocity.y << ' ' << physics.velocity.z << '\n';
					}
		obj_delete(index);
	}
	// Real portal crossing uses the starting medium for this frame, then the new medium
	int side = -1;
	for (int i = 0; i < 6; ++i) {
		const int child = Segments[60].children[i];
		if (child >= 0 && !xl_lava_segment(child) && !xl_preview_water_segment(child) && Segments[60].sides[i].wall_num < 0) side = i;
	}
	require(side >= 0, "Lava fixture lacks a dry portal");
	const int dry = Segments[60].children[side];
	vms_vector face;
	compute_center_point_on_side(&face, &Segments[60], side);
	const auto normal = Segments[60].sides[side].normals[0];
	const int index = obj_create(OBJ_ROBOT, 0, 60, &face, &vmd_identity_matrix, F1_0 / 8, CT_NONE, MT_PHYSICS, RT_NONE);
	require(index >= 0, "Lava crossing allocation failed");
	auto &actor = Objects[index];
	actor.mtype.phys_info = {};
	actor.mtype.phys_info.mass = F1_0;
	for (int direction : { 1, -1 }) {
		obj_relink(index, direction == 1 ? 60 : dry);
		vm_vec_scale_add(&actor.pos, &face, &normal, direction * F1_0 / 2);
		vm_vec_copy_scale(&actor.mtype.phys_info.velocity, &normal, -direction * i2f(4));
		auto before = actor.pos;
		const auto velocity = actor.mtype.phys_info.velocity;
		FrameTime = F1_0 / 4;
		do_physics_sim(&actor);
		const auto expect = [&](const vms_vector &start, fix time) {vms_vector point{start.x+static_cast<fix>(double(velocity.x)*time/F1_0),start.y+static_cast<fix>(double(velocity.y)*time/F1_0),start.z+static_cast<fix>(double(velocity.z)*time/F1_0)};return point; };
		auto expected = expect(before, direction == 1 ? FrameTime * 66 / 100 : FrameTime);
		require(actor.segnum == (direction == 1 ? dry : 60) && !std::memcmp(&actor.pos, &expected, sizeof(expected)), "Lava portal first-frame medium differs");
		before = actor.pos;
		do_physics_sim(&actor);
		expected = expect(before, direction == 1 ? FrameTime : FrameTime * 66 / 100);
		require(!std::memcmp(&actor.pos, &expected, sizeof(expected)), "Lava crossing retained prior medium");
	}
	obj_delete(index);
}
static void lava_lifecycle(const std::filesystem::path &output)
{
	const int player = ConsoleObject - Objects;
	Players[Player_num].flags = 0;
	Difficulty_level = 2;
	FrameTime = F1_0 / 4;
	ConsoleObject->control_type = CT_NONE;
	ConsoleObject->movement_type = MT_PHYSICS;
	ConsoleObject->mtype.phys_info = {};
	ConsoleObject->mtype.phys_info.mass = Player_ship->mass;
	for (int segment : { 60, 63 }) {
		obj_relink(player, segment);
		compute_segment_center(&ConsoleObject->pos, &Segments[segment]);
		Players[Player_num].shields = i2f(100);
		object_move_one(ConsoleObject);
		const fix damage = segment == 60 ? static_cast<fix>(double(TmapInfo[404].damage / 2) * FrameTime / F1_0) / 2 : 0;
		require(Players[Player_num].shields == i2f(100) - damage, "Actual player frame lost lava damage/protection");
	}
	obj_relink(player, 60);
	compute_segment_center(&ConsoleObject->pos, &Segments[60]);
	Players[Player_num].shields = 1;
	Players[Player_num].lives = 5;
	object_move_one(ConsoleObject);
	require(ConsoleObject->flags & OF_SHOULD_BE_DEAD, "Lava frame did not cause lethal damage");
	obj_delete_all_that_should_be_dead();
	require(Player_is_dead, "Lava lethal damage did not start death");
	DoPlayerDead();
	require(!Player_is_dead && Players[Player_num].lives == 4, "Lava death did not respawn normally");
	obj_relink(ConsoleObject - Objects, 63);
	compute_segment_center(&ConsoleObject->pos, &Segments[63]);
	ConsoleObject->mtype.phys_info.velocity = {};
	// The source's non-volumetric renderer colors water, including mixed water/lava, but not lava alone
	for (int segment : { 60, 62 })
		for (int side = 0; side < 6; ++side) {
			int32_t points[4][3];
			float colors[4][4];
			if (segment == 60) require(!xl_preview_water_portal(segment, side, points, colors), "Lava-only geometry acquired an invented water boundary");
		}

	trigger boost{};
	boost.type = 15;
	boost.value = 5;
	boost.num_links = 1;
	boost.seg[0] = 60;
	boost.side[0] = 0;
	require(!xl_speed_boost_action(&boost, -1, ConsoleObject - Objects, 0) && xl_speed_boost_active(ConsoleObject - Objects), "Lava boost interaction setup failed");
	const auto boosted_velocity = ConsoleObject->mtype.phys_info.velocity;
	GameTime64 = i2f(20);
	require(xl_fluid_restore(i2f(2), 1), "Lava boosted scrape clock setup failed");
	const int32_t hit[] = { ConsoleObject->pos.x, ConsoleObject->pos.y, ConsoleObject->pos.z };
	require(xl_fluid_scrape(ConsoleObject - Objects, 63, 0, hit) && xl_fluid_scrape_elapsed() == 0 && !std::memcmp(&boosted_velocity, &ConsoleObject->mtype.phys_info.velocity, sizeof(boosted_velocity)), "Lava scrape changed an actively boosted player velocity");
	xl_speed_boost_clear();
	ConsoleObject->mtype.phys_info.velocity = {};
	GameTime64 = i2f(20);
	require(xl_fluid_restore(F1_0 / 8, 1), "Lava clock setup failed");
	char save[] = "lava-earned.sav", description[] = "XL lava clock";
	stop_time();
	require(state_save_all_sub(save, description), "Lava disk save failed");
	auto *file = PHYSFS_openWrite("lava-state.bin");
	require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Lava trailer write failed");
	const auto user = output / "preview-user";
	std::ifstream input(user / "lava-state.bin", std::ios::binary);
	xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	require(bytes.size() > 16, "Missing lava trailer");
	for (int variant = 0; variant < 8; ++variant) {
		auto block = nlohmann::json::parse(bytes.begin() + 16, bytes.end());
		if (variant == 0) block.erase("fluid_scrape_elapsed");
		if (variant == 1) block["fluid_scrape_elapsed"] = nlohmann::json::parse("true");
		if (variant == 2) block["fluid_scrape_elapsed"] = 1.25;
		if (variant == 3) block["fluid_scrape_elapsed"] = "1";
		if (variant == 4) block["fluid_scrape_elapsed"] = UINT64_MAX;
		if (variant == 5) block["fluid_scrape_elapsed"] = INT64_MIN;
		if (variant == 6) {
			block["fluid_scrape_elapsed"] = 123;
			block["boosts"]["speed"] = 11;
		}
		if (variant == 7) {
			block["fluid_scrape_elapsed"] = 123;
			block["states"][0][1] = 999;
		}
		const auto text = block.dump();
		xl_compat::Bytes bad(bytes.begin(), bytes.begin() + 16);
		bad.insert(bad.end(), text.begin(), text.end());
		std::uint32_t checksum = 2166136261u;
		for (unsigned char byte : text) checksum = (checksum ^ byte) * 16777619u;
		for (int byte = 0; byte < 4; ++byte) {
			bad[8 + byte] = static_cast<unsigned char>(text.size() >> (byte * 8));
			bad[12 + byte] = static_cast<unsigned char>(checksum >> (byte * 8));
		}
		write_bytes((user / "lava-corrupt.bin").string().c_str(), bad);
		file = PHYSFS_openRead("lava-corrupt.bin");
		const int accepted = file ? xl_wall_trigger_read(file) : -1;
		const int closed = file ? PHYSFS_close(file) : 0;
		if (accepted || !closed || xl_fluid_scrape_elapsed() != F1_0 / 8) std::cerr << "Lava corrupt variant=" << variant << " accepted=" << accepted << " closed=" << closed << " elapsed=" << xl_fluid_scrape_elapsed() << " game=" << GameTime64 << '\n';
		require(!accepted && closed && xl_fluid_scrape_elapsed() == F1_0 / 8, "Invalid lava trailer partially published state");
	}
	char error[256]{};
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Lava stock transition failed");
	const auto stock = *ConsoleObject;
	const int32_t point[] = { stock.pos.x, stock.pos.y, stock.pos.z };
	int playing = 0;
	require(!xl_lava_segment(60) && !xl_fluid_frame(ConsoleObject - Objects, &playing) && xl_fluid_wall(ConsoleObject - Objects, ConsoleObject->segnum, 0) == -1 && !xl_fluid_scrape(ConsoleObject - Objects, ConsoleObject->segnum, 0, point) && !std::memcmp(&stock, ConsoleObject, sizeof(stock)), "XL fluids modified Redux state");
	require(state_restore_all_sub(save, 0) && xl_lava_segment(63) && xl_preview_no_damage_segment(63) && xl_fluid_scrape_elapsed() == F1_0 / 8, "Lava disk restore through stock lost source or scrape timing");
	start_time();
	// Backward-clock and extreme malicious elapsed values never overflow the stored clock
	require(xl_fluid_restore(-1, 1) && xl_fluid_scrape_elapsed() == -1, "Backward-clock restore rejected");
	require(!xl_fluid_restore(INT64_MIN, 0) && xl_fluid_scrape_elapsed() == -1, "Overflow clock accepted or validation mutated state");
	require(xl_fluid_restore(INT64_MAX, 1), "Representable long elapsed clock rejected");
	++GameTime64;
	require(xl_fluid_scrape_elapsed() == INT64_MAX, "Long elapsed clock overflowed");
	xl_fluid_clear();
	std::cout << "Lava lifecycle: actual lethal/nonlethal frames, protection, disk clock, eight atomic corrupt saves and Redux isolation passed\n";
}
static int lava_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto output = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		write_lava_fixture(output / "missions");
		require(load_base_mission("lava", error, sizeof(error)), error);
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());
		require(!xl_level_supported("neptunxl.rl2", 1), "Lava admitted multiplayer");
		const int texture_count = NumTextures;
		NumTextures = 404;
		require(!xl_level_supported("neptunxl.rl2", 0), "Lava admitted missing texture-404 resource");
		NumTextures = texture_count;
		require(xl_level_supported("neptunxl.rl2", 0), xl_level_rejection());

		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(1);
		require(xl_lava_segment(60) && !xl_preview_water_segment(60) && xl_lava_segment(62) && xl_preview_water_segment(62), "Lava property decoding failed");
		lava_contacts(output);
		lava_motion(output);
		lava_lifecycle(output);
		std::cout << "XL lava volume controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL lava failed: " << error.what() << '\n';
		return 1;
	}
}
