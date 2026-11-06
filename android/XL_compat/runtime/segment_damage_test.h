// Actual object frames against independently extracted source samples
#include "../segment_damage.h"
#include "segment_damage.h"
extern "C" void dead_player_frame(void);
static int segment_damage_test(int argc, char **argv)
{
	char error[256]{};
	const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
	std::ofstream samples(root / "segment-damage-samples.txt");
	for (int version : { 21, 23, 25, 27 }) {
		require(load_base_mission("rtlayout", error, sizeof(error)), error);
		const auto filename = "rt" + std::to_string(version) + ".rl2";
		require(xl_level_supported(filename.c_str(), 0) && !xl_level_supported(filename.c_str(), 1), "Damage admission/isolation failed");
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		StartNewGame(version == 21 ? 1 : version == 23 ? 2
		                             : version == 25   ? 3
		                                               : 4);
		Game_suspended = SUSP_ROBOTS;
		auto &actor = *ConsoleObject;
		auto &player = Players[Player_num];
		actor.control_type = CT_NONE;
		actor.movement_type = MT_NONE;
		actor.lifeleft = IMMORTAL_TIME;
		for (int room = 60; room <= 66; ++room) {
			obj_relink(&actor - Objects, room);
			compute_segment_center(&actor.pos, &Segments[room]);
			int16_t sr = 0, er = 0;
			require(xl_segment_damage_rates(room, &sr, &er), "Source rates unavailable");
			for (int difficulty = 0; difficulty < 5; ++difficulty)
				for (bool invulnerable : { false, true })
					for (fix frame : { 0, 1, F1_0 / 60, F1_0 / 30, F1_0 / 4, F1_0 / 5 })
						for (fix resource : { i2f(1), i2f(50) + 1, i2f(100), i2f(200), i2f(250) }) {
							Difficulty_level = difficulty;
							player.flags = invulnerable ? PLAYER_FLAGS_INVULNERABLE : 0;
							player.shields = player.energy = actor.shields = resource;
							FrameTime = frame;
							const auto red = PaletteRedAdd, green = PaletteGreenAdd, blue = PaletteBlueAdd;
							const auto random_calls = d_rand_get_stream_call_count(D_RNG_SIM);
							const auto expected_h = xl_compat::segment_resource_after(resource, xl_compat::segment_damage_amount(frame, sr, resource, true));
							const auto expected_e = xl_compat::segment_resource_after(resource, xl_compat::segment_damage_amount(frame, er, resource, false));
							object_move_one(&actor);
							const auto h = room == 64 ? std::max(expected_h, std::min(i2f(100), expected_h + 11 * frame)) : expected_h;
							const auto e = room == 63 ? std::max(expected_e, std::min(i2f(100), expected_e + 25 * frame)) : expected_e;
							require(player.shields == h && player.energy == e && !Player_is_dead, "Actual damage/center frame differs");
							require(PaletteRedAdd == red && PaletteGreenAdd == green && PaletteBlueAdd == blue, "Nonlethal authored damage flashed palette");
							require(d_rand_get_stream_call_count(D_RNG_SIM) == random_calls, "Stationary authored damage consumed gameplay RNG");
							samples << sr << ' ' << er << ' ' << (room == 63 ? 1 : room == 64 ? 2
							                                                                  : 0)
							        << ' ' << frame << ' ' << resource << ' ' << resource << ' ' << player.shields << ' ' << player.energy << '\n';
						}
		}
		// Real portal crossing in both directions: charge the starting segment once
		vms_vector face;
		compute_center_point_on_side(&face, &Segments[60], 2);
		const auto normal = Segments[60].sides[2].normals[0];
		require(Segments[60].children[2] == 61 && Segments[61].children[0] == 60, "Damage portal changed");
		actor.movement_type = MT_PHYSICS;
		actor.mtype.phys_info = {};
		actor.mtype.phys_info.mass = F1_0;
		actor.size = F1_0 / 8;
		FrameTime = F1_0 / 4;
		for (int enter : { 1, 0 }) {
			obj_relink(&actor - Objects, enter ? 61 : 60);
			vm_vec_scale_add(&actor.pos, &face, &normal, enter ? -F1_0 / 2 : F1_0 / 2);
			vm_vec_copy_scale(&actor.mtype.phys_info.velocity, &normal, enter ? i2f(4) : -i2f(4));
			player.shields = player.energy = i2f(50);
			object_move_one(&actor);
			require(actor.segnum == (enter ? 60 : 61) && player.shields == i2f(50) - (enter ? 0 : 3 * FrameTime) && player.energy == i2f(50) - (enter ? 0 : 5 * FrameTime), "Damage crossing used arrival room or multiple physics charges");
			actor.mtype.phys_info.velocity = {};
			object_move_one(&actor);
			require(player.shields == i2f(50) - 3 * FrameTime && player.energy == i2f(50) - 5 * FrameTime, "Damage did not start/stop on next frame");
		}
		actor.movement_type = MT_NONE;
		obj_relink(&actor - Objects, 60);
		compute_segment_center(&actor.pos, &Segments[60]);
		FrameTime = F1_0 / 60;
		player.shields = player.energy = F1_0;
		for (int i = 0; i < 3; ++i) object_move_one(ConsoleObject);
		const auto saved_h = player.shields, saved_e = player.energy;
		const auto replay_frame = FrameTime;
		const auto identity = xl_level_trigger_identity();
		char save[] = "segdamage.sav", description[] = "Segment damage earned state";
		stop_time();
		require(state_save_all_sub(save, description), "Damage disk save failed");
		auto *file = PHYSFS_openWrite("damage-state.bin");
		require(file && xl_wall_trigger_write(file) && PHYSFS_close(file), "Damage trailer save failed");
		std::ifstream original_trailer(root / "preview-user/damage-state.bin", std::ios::binary);
		const xl_compat::Bytes trailer_bytes{ std::istreambuf_iterator<char>(original_trailer), std::istreambuf_iterator<char>() };
		require(trailer_bytes.size() > 16, "Missing damage identity trailer");
		for (int variant = 0; variant < 4; ++variant) {
			auto block = nlohmann::json::parse(trailer_bytes.begin() + 16, trailer_bytes.end());
			if (variant == 0) block["source"] = true;
			if (variant == 1) block["source"] = std::to_string(identity);
			if (variant == 2) block["source"] = -1;
			if (variant == 3) block.erase("source");
			const auto text = block.dump();
			xl_compat::Bytes corrupt(trailer_bytes.begin(), trailer_bytes.begin() + 16);
			corrupt.insert(corrupt.end(), text.begin(), text.end());
			std::uint32_t checksum = 2166136261u;
			for (unsigned char byte : text) checksum = (checksum ^ byte) * 16777619u;
			for (int byte = 0; byte < 4; ++byte) {
				corrupt[8 + byte] = static_cast<unsigned char>(text.size() >> (byte * 8));
				corrupt[12 + byte] = static_cast<unsigned char>(checksum >> (byte * 8));
			}
			write_bytes((root / "preview-user/damage-corrupt.bin").string().c_str(), corrupt);
			const auto repair_elapsed = xl_repair_elapsed();
			file = PHYSFS_openRead("damage-corrupt.bin");
			require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && xl_level_trigger_identity() == identity && player.shields == saved_h && player.energy == saved_e && xl_repair_elapsed() == repair_elapsed && !Player_is_dead, "Malformed damage identity changed active state");
		}
		ConsoleObject->control_type = CT_FLYING;
		int earned_death = -1;
		for (int i = 0; i < 70; ++i) {
			object_move_one(ConsoleObject);
			if (Player_is_dead && earned_death < 0) earned_death = i;
		}
		require(earned_death >= 0 && player.shields == -1 && player.energy == 0, "Damage sequence omitted its lethal boundary");
		const auto earned_h = player.shields, earned_e = player.energy;
		dead_player_end();
		Dead_player_camera = nullptr;
		for (const std::string variant : { "dr", "ds", "dm", "dz" }) {
			const auto changed = variant + std::to_string(version) + ".rl2";
			require(!load_level(changed.c_str()) && xl_level_trigger_identity() != identity, "Damage rate/sign/location/zero identity collision");
			const auto h = player.shields, e = player.energy;
			const auto fresh = xl_level_trigger_identity();
			file = PHYSFS_openRead("damage-state.bin");
			require(file && !xl_wall_trigger_read(file) && PHYSFS_close(file) && xl_level_trigger_identity() == fresh && player.shields == h && player.energy == e, "Foreign damage identity published partial state");
		}
		require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Damage stock transition failed");
		int16_t sr = 1, er = 1;
		require(!xl_segment_damage_rates(60, &sr, &er) && !sr && !er, "Damage source leaked into stock");
		require(state_restore_all_sub(save, 0) && xl_level_trigger_identity() == identity && player.shields == saved_h && player.energy == saved_e, "Damage restore reset earned resources/source");
		ConsoleObject->control_type = CT_FLYING;
		ConsoleObject->movement_type = MT_NONE;
		FrameTime = replay_frame;
		int replay_death = -1;
		for (int i = 0; i < 70; ++i) {
			object_move_one(ConsoleObject);
			if (Player_is_dead && replay_death < 0) replay_death = i;
		}
		require(player.shields == earned_h && player.energy == earned_e && ConsoleObject->segnum == 60 && replay_death == earned_death, "Damage replay after disk-through-stock differs");
		dead_player_end();
		Dead_player_camera = nullptr;
		ConsoleObject->control_type = CT_NONE;
		int16_t invalid_sr = 1, invalid_er = 1;
		require(!xl_segment_damage_rates(-1, &invalid_sr, &invalid_er) && !invalid_sr && !invalid_er, "Invalid segment exposed rates");
		require(Current_mission, "Damage source has no mission owner");
		const auto descent_version = Current_mission->descent_version;
		Current_mission->descent_version = 1;
		require(!xl_segment_damage_rates(60, &invalid_sr, &invalid_er) && !invalid_sr && !invalid_er && xl_segment_damage_tick(ConsoleObject) && player.shields == earned_h && player.energy == earned_e && !Player_is_dead, "Imported-D1 context consumed segment rates");
		Current_mission->descent_version = descent_version;
		const auto mode = Game_mode;
		Game_mode |= GM_MULTI;
		require(!xl_segment_damage_rates(60, &invalid_sr, &invalid_er) && !invalid_sr && !invalid_er, "Multiplayer exposed rates");
		Game_mode = mode;
		const auto excluded_h = player.shields, excluded_e = player.energy;
		ConsoleObject->id = Player_num + 1;
		object_move_one(ConsoleObject);
		require(player.shields == excluded_h && player.energy == excluded_e, "Nonlocal player consumed segment rates");
		ConsoleObject->id = Player_num;
		ConsoleObject->type = OBJ_ROBOT;
		object_move_one(ConsoleObject);
		require(player.shields == excluded_h && player.energy == excluded_e, "Robot consumed local segment resources");
		ConsoleObject->type = OBJ_PLAYER;
		const auto before = xl_level_trigger_identity();
		require(load_level("badprop.rl2") != 0 && xl_level_trigger_identity() == before, "Failed replacement changed active damage source");
		FrameTime = 1;
		player.shields = 3;
		player.energy = 10;
		object_move_one(ConsoleObject);
		require(player.shields == 0 && player.energy == 5 && !Player_is_dead, "Exactly zero shield boundary killed");
		ConsoleObject->control_type = CT_FLYING;
		obj_relink(ConsoleObject - Objects, 64);
		compute_segment_center(&ConsoleObject->pos, &Segments[64]);
		object_move_one(ConsoleObject);
		require(player.shields == -1 && player.energy == 0 && Player_is_dead && ConsoleObject->shields == INITIAL_SHIELDS, "Lethal tick lost energy update or native object reset");
		auto *camera = Dead_player_camera;
		object_move_one(ConsoleObject);
		require(player.shields == -1 && player.energy == 0 && Dead_player_camera == camera, "Repeated death tick changed native death ownership");
		obj_relink(ConsoleObject - Objects, 62);
		object_move_one(ConsoleObject);
		require(player.shields == 2 && player.energy == 5 && Player_is_dead && Dead_player_camera == camera, "Negative rates invented resurrection or suppressed dead OBJ_PLAYER updates");
		FrameTime = F1_0 / 4;
		for (int i = 0; i < 9; ++i) dead_player_frame();
		require(ConsoleObject->type == OBJ_GHOST && Player_is_dead && Dead_player_camera == camera, "Ordinary death did not retain ghost/camera ownership");
		FrameTime = 1;
		const auto h = player.shields, e = player.energy;
		object_move_one(ConsoleObject);
		require(player.shields == h && player.energy == e, "Ghost consumed authored rates");
		dead_player_end();
		Dead_player_camera = nullptr;
		std::cout << version << "/40 segment damage native frames, portal order, identity, death and disk-through-stock passed\n";
	}
	require(bool(samples), "Cannot write actual segment damage samples");
	std::cout << "Authored segment damage controls passed\n";
	return 0;
}
