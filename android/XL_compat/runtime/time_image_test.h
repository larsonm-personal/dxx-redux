#ifndef DXX_XL_TIME_IMAGE_TEST_H
#define DXX_XL_TIME_IMAGE_TEST_H
#include "../time_equipment.h"

static void time_image_native_case(const std::filesystem::path &root, const std::filesystem::path &installation)
{
	using namespace xl_compat;
	const auto saved_flags = Players[Player_num].flags;
	const auto equipment = XL_PLAYER_FLAGS_SLOW_MOTION | XL_PLAYER_FLAGS_BULLET_TIME;
	const std::string level_name = Gamesave_current_filename;
	const std::string original_mission = Current_mission_filename;
	// Native saves retain eight mission-name characters; use a short derived owner
	std::filesystem::copy_file(mission_resource_archive(), root / "missions/fxclock.hog");
	{
		std::ofstream descriptor(root / "missions/fxclock.mn2");
		descriptor << "d2x-name = Time image controls\nnum_levels = 1\n"
		           << level_name << '\n';
		require(bool(descriptor), "Cannot write time image fixture descriptor");
	}
	char error[256]{};
	require(load_base_mission("fxclock", error, sizeof(error)), error);
	std::array<std::filesystem::path, 2> paths;
	for (unsigned i = 0; i < 2; ++i) {
		paths[i] = root / animation_mission_texture_path(mission_resource_archive(), i ? "bullettime#0" : "slowmotion#0");
		require(!std::filesystem::exists(paths[i]) && !prepare_time_equipment_image(i), "Time image control requires absent original addons");
	}
	std::ofstream receipt(root / "time-images.txt");
	receipt << "XL_TIME_IMAGE 1\n";
	unsigned cases = 0;
	const auto animate = [&](bool present) {
		uint32_t sim;
		require(d_rand_get_state(&sim), "Cannot observe time image SIM state");
		auto calls = d_rand_get_call_count();
		for (unsigned image = 0; image < 2; ++image) {
			std::array<int, 2> owners;
			for (auto &owner : owners) {
				owner = obj_create(OBJ_POWERUP, int(48 + image), ConsoleObject->segnum, &ConsoleObject->pos, &ConsoleObject->orient,
				                   time_equipment_spawn_radius, CT_POWERUP, MT_NONE, RT_POWERUP);
				require(owner >= 0 && xl_time_equipment_initialize(&Objects[owner], 1), "Cannot initialize native time pickup");
			}
			require((owners[0] & 1) != (owners[1] & 1), "Time animation control requires both native slot parities");
			for (const auto owner : owners) {
				auto &actor = Objects[owner];
				for (const int lag : { 20, 180, 1300, -1000 }) {
					input_demo_environment_clock before{}, after{};
					xl_input_demo_current_environment(&before);
					const auto timestamp = int(before.pickup_ms) - lag;
					const int initial_frame = lag == 1300 ? 0 : 7;
					actor.rtype.vclip_info.frametime = timestamp;
					actor.rtype.vclip_info.framenum = sbyte(initial_frame);
					require(xl_time_equipment_animate(&actor), "Native time animation was not dispatched");
					xl_input_demo_current_environment(&after);
					const auto &clip = actor.rtype.vclip_info;
					int observed = int(before.pickup_ms);
					if (lag < 0) {
						require(clip.frametime >= int(before.pickup_ms) && clip.frametime <= int(after.pickup_ms) && clip.framenum == 7,
						        "Backwards time image clock did not reset within its observed native interval");
						observed = clip.frametime;
					} else {
						require((int(after.pickup_ms) - timestamp) / 80 == lag / 80, "Time image control crossed its observed clock interval");
						if (!present) require(clip.frametime == timestamp && clip.framenum == initial_frame, "Missing image advanced native animation state");
					}
					receipt << "case " << owner << ' ' << image << ' ' << (present ? 16 : 0) << ' ' << observed << ' '
					        << timestamp << ' ' << initial_frame << ' ' << clip.frametime << ' ' << int(clip.framenum) << '\n';
					++cases;
				}
				if (!present) require(xl_time_equipment_draw(&actor), "Absent time image fell through to stock drawing");
			}
			uint32_t after_sim;
			require(d_rand_get_state(&after_sim) && after_sim == sim && d_rand_get_call_count() == calls, "Time image animation consumed SIM RNG");
			if (!present) {
				auto &pickup = Objects[owners[0]];
				collide_player_and_powerup(ConsoleObject, &pickup, &pickup.pos);
				require(pickup.flags & OF_SHOULD_BE_DEAD, "Missing image prevented ordinary time pickup contact");
			}
			for (const auto owner : owners) obj_delete(owner);
			// Ordinary pickup sound effects are outside the animation-only RNG check
			require(d_rand_get_state(&sim), "Cannot observe post-pickup SIM state");
			calls = d_rand_get_call_count();
		}
	};
	Players[Player_num].flags |= equipment;
	require(!load_level(level_name.c_str()), "Cannot load carried time equipment without images");
	Players[Player_num].flags &= ~equipment;
	require(xl_prepared_time_equipment_required() && !xl_prepared_time_equipment_image(0) && !xl_prepared_time_equipment_image(1),
	        "Absent time images lost the retained logical equipment requirement");
	const auto *clock_error = xl_omega_input_demo_error();
	require(clock_error && std::strstr(clock_error, "real-time clock timeline"), "Missing images bypassed input-demo environment admission");
	animate(false);
	require((Players[Player_num].flags & equipment) == equipment, "Absent time images prevented both device grants");
	char save[] = "absent-time-images.sav", description[] = "Absent time images";
	stop_time();
	require(state_save_all_sub(save, description), "Cannot save absent time image state");
	require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2"), "Time image stock transition failed");
	require(state_restore_all_sub(save, 0) && (Players[Player_num].flags & equipment) == equipment &&
	            xl_prepared_time_equipment_required() && !xl_prepared_time_equipment_image(0) && !xl_prepared_time_equipment_image(1),
	        "Absent time image disk-through-stock restore changed equipment or invented pixels");
	const auto image_bytes = [](unsigned char red) {
		Bytes bytes(18 + 4 * 64 * 3);
		bytes[2] = 2;
		bytes[12] = 4;
		bytes[14] = 64;
		bytes[16] = 24;
		for (std::size_t at = 18; at < bytes.size(); at += 3) bytes[at + 2] = red;
		return bytes;
	};
	for (unsigned i = 0; i < 2; ++i) {
		std::filesystem::create_directories(paths[i].parent_path());
		write_bytes(paths[i].string().c_str(), image_bytes(47 + i));
	}
	require(!load_level(level_name.c_str()), "Cannot load native time images");
	for (unsigned i = 0; i < 2; ++i)
		require(xl_prepared_time_equipment_image(i) && xl_prepared_time_equipment_image(i)->frames.size() == 16, "Time image frames were not retained");
	{
		const auto directory = root / "incoming-time-images";
		std::filesystem::create_directories(directory);
		const auto archive = std::filesystem::path(mission_resource_archive()).filename();
		auto descriptor = archive;
		descriptor.replace_extension(".mn2");
		auto *source = mission_prepare_by_name(Current_mission_filename);
		require(source, "Cannot retain time image descriptor");
		{
			std::ofstream output(directory / descriptor);
			output << mission_prepared_descriptor_text(source);
			require(bool(output), "Cannot write independent time image descriptor");
		}
		require(mission_discard_prepared(&source), "Cannot release time image descriptor");
		std::filesystem::copy_file(mission_resource_archive(), directory / archive);
		const auto physical = std::filesystem::absolute(directory).string();
		require(PHYSFS_mount(physical.c_str(), "__incoming_time_images", 1), "Cannot mount incoming time image descriptor");
		auto *mission = mission_prepare_from_current_dir(("__incoming_time_images/" + descriptor.string()).c_str());
		struct Release {
			mission_preparation **mission;
			~Release()
			{
				mission_discard_prepared(mission);
			}
		} release{ &mission };
		require(mission && PHYSFS_unmount(physical.c_str()), "Cannot retain isolated time image context");
		TargetSources target(mission, installation.string());
		const auto prepare_incoming = [&](unsigned index) {
			TargetSources candidate(mission, installation.string());
			return prepare_time_equipment_image(index, &candidate);
		};
		for (unsigned i = 0; i < 2; ++i) {
			require(!prepare_incoming(i), "Absent incoming time image borrowed outgoing pixels");
			const auto path = directory / animation_mission_texture_path(target.archive().c_str(), i ? "bullettime#0" : "slowmotion#0");
			std::filesystem::create_directories(path.parent_path());
			write_bytes(path.string().c_str(), image_bytes(79 + i));
			const auto incoming = prepare_incoming(i);
			require(incoming && incoming->frames.size() == 16 && incoming->bitmap.pixels[0] == 79 + i &&
			            xl_prepared_time_equipment_image(i)->bitmap.pixels[0] == 47 + i,
			        "Equal-count incoming time image aliased outgoing pixels");
			write_bytes(path.string().c_str(), { 0, 1, 2 });
			bool rejected = false;
			try {
				(void) prepare_incoming(i);
			} catch (const std::runtime_error &) {
				rejected = true;
			}
			require(rejected && incoming->bitmap.pixels[0] == 79 + i, "Incoming time image rejection changed retained ownership");
		}
	}
	{
		const auto foreign = root / "foreign-time-images";
		const auto path = foreign / animation_mission_texture_path(mission_resource_archive(), "slowmotion#0");
		std::filesystem::create_directories(path.parent_path());
		write_bytes(path.string().c_str(), image_bytes(99));
		const auto physical = foreign.string();
		require(PHYSFS_addToSearchPath(physical.c_str(), 0), "Cannot mount foreign time image control");
		bool rejected = false;
		try {
			(void) prepare_time_equipment_image(0);
		} catch (const std::runtime_error &error) {
			rejected = std::strstr(error.what(), "shadowed outside") != nullptr;
		}
		require(PHYSFS_removeFromSearchPath(physical.c_str()), "Cannot unmount foreign time image control");
		require(rejected && xl_prepared_time_equipment_image(0)->bitmap.pixels[0] == 47, "Foreign time image changed live ownership or was admitted");
	}
	{
		std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead(level_name.c_str()), PHYSFS_close);
		const auto size = file ? PHYSFS_fileLength(file.get()) : -1;
		require(size > 0 && size < 128 * 1024 * 1024, "Cannot inspect time image source level");
		Bytes bytes(static_cast<std::size_t>(size));
		require(PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) == size, "Cannot retain time image source level");
		const auto level = inspect_level(bytes);
		XlPreparedResources prepared(level_name.c_str(), &level, 1);
		const auto *retained = prepared.time_equipment_image(0);
		require(retained && retained->bitmap.pixels[0] == 47, "Time image owner did not decode selected pixels");
		XlPreparedResources moved(std::move(prepared));
		XlPreparedResources assigned(level_name.c_str(), &level, 1);
		assigned = std::move(moved);
		require(assigned.time_equipment_image(0) == retained && assigned.time_equipment_required(), "Time image move lost decoded ownership or logical requirements");
		auto wrong_frames = image_bytes(47);
		wrong_frames[14] = 60;
		write_bytes(paths[0].string().c_str(), wrong_frames);
		bool wrong_layout = false;
		try {
			(void) prepare_time_equipment_image(0);
		} catch (const std::runtime_error &error) {
			wrong_layout = std::strstr(error.what(), "sixteen vertical square frames") != nullptr;
		}
		require(wrong_layout && retained->frames.size() == 16, "Time image preparation accepted an invalid frame layout or changed its retained owner");
		write_bytes(paths[0].string().c_str(), { 0, 1, 2 });
		bool rejected = false;
		try {
			(void) prepare_time_equipment_image(0);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected && retained->bitmap.pixels[0] == 47 && xl_prepared_time_equipment_image(0)->bitmap.pixels[0] == 47, "Malformed time image changed prepared/live pixels or became absence");
		for (const auto &path : paths) std::filesystem::remove(path);
		require(retained->frames.size() == 16 && retained->bitmap.pixels[0] == 47, "Moved time image depended on deleted source bytes");
	}
	for (unsigned i = 0; i < 2; ++i) require(xl_prepared_time_equipment_image(i)->frames.size() == 16, "Discarding candidate time images changed live ownership");
	animate(true);
	Players[Player_num].flags = saved_flags;
	require(load_base_mission(original_mission.c_str(), error, sizeof(error)), error);
	require(!load_level(level_name.c_str()) && !xl_prepared_time_equipment_required() && !xl_prepared_time_equipment_image(0), "Time image cleanup retained carried-only resources");
	receipt << "end " << cases << '\n';
	require(bool(receipt) && cases == 32, "Cannot preserve complete native time image receipts");
	std::cout << "XL time images: absent native animation/draw/pickup, clock admission, disk restore, decoded moves/discard and live continuation passed\n";
}
#endif
