#include "../tests/hog_fixture.h"
#include "../hog.h"
static xl_compat::Bytes hog_test_read(const char *name)
{
	std::unique_ptr<PHYSFS_File, decltype(&PHYSFS_close)> file(PHYSFS_openRead(name), PHYSFS_close);
	require(file && PHYSFS_fileLength(file.get()) >= 0, "Cannot open archive member");
	xl_compat::Bytes bytes(static_cast<std::size_t>(PHYSFS_fileLength(file.get())));
	require(PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) == static_cast<PHYSFS_sint64>(bytes.size()), "Cannot read full archive member");
	return bytes;
}
static int hog_test(int argc, char **argv)
{
	try {
		char error[256]{};
		require(init_headless_metadata_runtime(argc, argv, error, sizeof(error)), error);
		require(mount_extra_dir(find_arg_value(argc, argv, "-extra-dir"), error, sizeof(error)), error);
		const auto root = std::filesystem::path(find_arg_value(argc, argv, "-extra-dir"));
		std::ifstream original(root / "missions" / "neptunxl.hog", std::ios::binary);
		const xl_compat::Bytes source{ std::istreambuf_iterator<char>(original), std::istreambuf_iterator<char>() };
		const auto members = xl_compat::read_hog(source);
		xl_compat::Bytes bytes{ 'D', '2', 'X' };
		for (const auto &member : members) xl_test::hog_member(bytes, member.name, member.bytes, !member.bytes.empty());
		const std::string long_name = "A-long-owned-resource-with-more-than-thirteen-characters.bin";
		const xl_compat::Bytes payload{ 17, 31, 42, 93, 0, 255 };
		xl_test::hog_member(bytes, long_name, payload, true);
		xl_test::hog_member(bytes, "empty", {}, false);
		for (const auto stem : { "xhogs", "hlegacy" }) {
			if (!std::strcmp(stem, "hlegacy")) {
				bytes[1] = 'H';
				bytes[2] = 'F';
			}
			const auto path = root / "missions" / (std::string(stem) + ".hog");
			write_bytes(path.string().c_str(), bytes);
			std::ofstream(root / "missions" / (std::string(stem) + ".mn2")) << "d2x-name = Derived XL " << stem << "\ntype = normal\nnum_levels = 1\nneptunxl.rl2\n";
			require(PHYSFS_mount(path.string().c_str(), "container-check", 0), "Cannot mount extended archive");
			const auto lookup = "container-check/" + xl_compat::hog_key(long_name);
			require(hog_test_read(lookup.c_str()) == payload && !PHYSFS_exists("container-check/alias.bin"), "Long-name selection or alias isolation differs");
			require(std::filesystem::equivalent(path, PHYSFS_getRealDir(lookup.c_str())), "Archive lost resource owner identity");
			std::unique_ptr<PHYSFS_File, decltype(&PHYSFS_close)> a(PHYSFS_openRead(lookup.c_str()), PHYSFS_close), b(PHYSFS_openRead(lookup.c_str()), PHYSFS_close);
			unsigned char value = 0;
			require(a && b && PHYSFS_seek(a.get(), 4) && PHYSFS_readBytes(a.get(), &value, 1) == 1 && !value && PHYSFS_tell(b.get()) == 0 && PHYSFS_readBytes(b.get(), &value, 1) == 1 && value == 17, "Archive streams share cursor state");
			require(!PHYSFS_seek(a.get(), payload.size() + 1) && PHYSFS_seek(a.get(), payload.size()) && PHYSFS_readBytes(a.get(), &value, 1) == 0, "Archive stream escaped its member bounds");
			require(!PHYSFS_unmount(path.string().c_str()), "Archive unmounted with open streams");
			a.reset();
			b.reset();
			char **names = PHYSFS_enumerateFiles("container-check");
			bool found = false;
			for (char **p = names; p && *p; ++p) found |= long_name == *p;
			PHYSFS_freeList(names);
			require(found && PHYSFS_unmount(path.string().c_str()) && !PHYSFS_exists(lookup.c_str()), "Archive enumeration or unmount cleanup failed");
			require(load_base_mission(stem, error, sizeof(error)), error);
			std::strcpy(Players[Player_num].callsign, "XLTest");
			input_demo_set_skip_level_intro(1);
			StartNewGame(1);
			require(xl_level_extensions_active() && Num_segments > 0 && hog_test_read(long_name.c_str()) == payload, "Extended mission failed ordinary startup/resource lookup");
			for (const auto &member : members) require(hog_test_read(member.name.c_str()) == member.bytes, "Mounted extended member differs from original payload");
			const auto identity = xl_level_trigger_identity();
			const auto light = light_snapshot();
			char save[] = "hog-earned.sav", description[] = "XL extended container";
			stop_time();
			require(state_save_all_sub(save, description), "Cannot save extended mission");
			require(load_base_mission("d2", error, sizeof(error)) && !load_level("d2leva-1.rl2") && !PHYSFS_exists(long_name.c_str()), "Extended resource leaked into stock");
			require(state_restore_all_sub(save, 0) && xl_level_trigger_identity() == identity && same_light_snapshot(light, light_snapshot()) && hog_test_read(long_name.c_str()) == payload, "Extended archive restore lost owner, source or light state");
			start_time();
			auto corrupt = bytes;
			corrupt.pop_back();
			const auto bad = root / (std::string(stem) + "-bad.hog");
			write_bytes(bad.string().c_str(), corrupt);
			require(!PHYSFS_mount(bad.string().c_str(), "bad-container", 0) && xl_level_trigger_identity() == identity && hog_test_read(long_name.c_str()) == payload, "Malformed archive altered live mission ownership");
		}
		std::cout << "Extended HOG native lifecycle passed: DHF/D2X mixed entries, independent streams, owner identity, exact member payloads, disk through stock and malformed isolation\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << "Extended HOG failed: " << e.what() << '\n';
		return 1;
	}
}
