// Real loader rejection/recovery control; decoded effects must never replace live state
#include "particle_world_test.h"
#include "particle_objects_test.h"
#include "particle_resource_test.h"
#include "particle_original_test.h"
#include "particle_secret_test.h"
static int effect_inspection_test(int argc, char **argv)
{
	try {
		char error[256] = {};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		require(load_base_mission("d2", error, sizeof(error)), error);
		PHYSFS_file *stock_file = PHYSFS_openRead("d2leva-1.rl2");
		require(stock_file && PHYSFS_fileLength(stock_file) > 0, "stock rejection control source missing");
		xl_compat::Bytes stock(static_cast<std::size_t>(PHYSFS_fileLength(stock_file)));
		const auto read = PHYSFS_readBytes(stock_file, stock.data(), stock.size());
		PHYSFS_close(stock_file);
		require(read == static_cast<PHYSFS_sint64>(stock.size()), "stock rejection control source truncated");
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir")) / "missions";
		const auto fixtures = std::filesystem::path(find_arg_value(argc, argv, "-effect-inspection-test"));
		std::ifstream original(root / "neptunxl.hog", std::ios::binary);
		xl_compat::Bytes archive{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() };
		require(archive.size() > 3 && archive[0] == 'D' && archive[1] == 'H' && archive[2] == 'F', "effect rejection source HOG missing");
		for (int version : { 23, 25, 27 }) {
			std::ifstream effect_file(fixtures / ("e" + std::to_string(version) + "r11p0.rl2"), std::ios::binary);
			const xl_compat::Bytes effect{ std::istreambuf_iterator<char>(effect_file), std::istreambuf_iterator<char>() };
			const auto decoded = xl_compat::inspect_level(effect);
			require(decoded.level_version == version && decoded.objects.size() == 1 && decoded.objects[0].render == 11 && decoded.objects[0].render_data.size() == 13, "effect rejection fixture did not decode");
			char name[13]{};
			std::snprintf(name, sizeof(name), "badefx%d.rl2", version);
			archive.insert(archive.end(), name, name + sizeof(name));
			for (int shift = 0; shift < 32; shift += 8) archive.push_back(static_cast<unsigned char>(effect.size() >> shift));
			archive.insert(archive.end(), effect.begin(), effect.end());
		}
		char stock_name[13] = "stock.rl2";
		archive.insert(archive.end(), stock_name, stock_name + sizeof(stock_name));
		for (int shift = 0; shift < 32; shift += 8) archive.push_back(static_cast<unsigned char>(stock.size() >> shift));
		archive.insert(archive.end(), stock.begin(), stock.end());
		write_bytes((root / "fxinspect.hog").string().c_str(), archive);
		write_bytes((root / "fxstock.hog").string().c_str(), archive);
		std::ofstream descriptor(root / "fxinspect.mn2");
		descriptor << "d2x-name = Derived effect inspection\nnum_levels = 1\nneptunxl.rl2\n";
		descriptor.close();
		std::ofstream stock_descriptor(root / "fxstock.mn2");
		stock_descriptor << "name = Derived stock effect rejection\nnum_levels = 1\nstock.rl2\n";
		stock_descriptor.close();
		std::strcpy(Players[Player_num].callsign, "XLTest");
		input_demo_set_skip_level_intro(1);
		std::ofstream particle_receipt;
		if (const auto *path = find_arg_value(argc, argv, "-particle-world-export")) {
			particle_receipt.open(path);
			require(bool(particle_receipt), "Cannot open particle world receipt");
			particle_receipt << "XL_PARTICLE_WORLD 1\n";
		}
		for (int version : { 23, 25, 27 }) {
			const auto effect_name = "badefx" + std::to_string(version) + ".rl2";
			for (int level : { 1, 2 }) {
				require(load_base_mission(level == 1 ? "fxstock" : "fxinspect", error, sizeof(error)), error);
				StartNewGame(1);
				const int active = xl_level_extensions_active();
				require(!!active == (level == 2), "stock/XL positive source did not load before rejection control");
				if (version == 23 && particle_receipt.is_open()) particle_world_native_case(particle_receipt, level == 1 ? "stock.rl2" : "neptunxl.rl2", unsigned(level - 1));
				if (version == 23) particle_objects_native_case(level == 2);
				if (version == 23 && level == 2) {
					const auto *installation = find_arg_value(argc, argv, "-particle-installation-fixture");
					require(installation && *installation, "Particle resource controls require an isolated installation fixture");
					particle_resource_native_case(root.parent_path(), installation);
				}
				const auto vertex = Vertices[0], position = ConsoleObject->pos;
				const int segments = Num_segments, objects = Highest_object_index, player = ConsoleObject - Objects;
				const auto identity = xl_level_trigger_identity();
				require(!xl_level_supported(effect_name.c_str(), 0) && std::strstr(xl_level_rejection(), "effect_particles"), "decoded effect was admitted or lacked its specific requirement");
				require(load_level(effect_name.c_str()) != 0 && std::strstr(xl_level_rejection(), "effect_particles"), "production load bypassed effect capability rejection");
				require(xl_level_extensions_active() == active && xl_level_trigger_identity() == identity && Num_segments == segments && Highest_object_index == objects && ConsoleObject - Objects == player && !std::memcmp(&Vertices[0], &vertex, sizeof(vertex)) && !std::memcmp(&ConsoleObject->pos, &position, sizeof(position)), "effect rejection replaced active level or player state");
			}
			require(xl_level_supported("neptunxl.rl2", 0) && load_level("neptunxl.rl2") == 0, "effect rejection prevented supported-source recovery");
			std::cout << "XL effect inspection version " << version << ": stock/XL rejection and supported recovery passed\n";
		}
		if (particle_receipt.is_open()) particle_receipt << "end 2\n";
		particle_secret_native_case(archive, root, find_arg_value(argc, argv, "-particle-installation-fixture"));
		if (const auto *package = find_arg_value(argc, argv, "-pumo-package"))
			particle_original_native_case(package, find_arg_value(argc, argv, "-particle-original-image"), find_arg_value(argc, argv, "-particle-installation-fixture"));
		std::cout << "XL effect inspection gate passed: decoded payload rejected with a useful reason, live state preserved, supported source recovers\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << "XL effect inspection gate failed: " << error.what() << '\n';
		return 1;
	}
}
