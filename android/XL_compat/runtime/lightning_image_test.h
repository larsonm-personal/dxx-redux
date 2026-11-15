#include "../tests/hog_fixture.h"
static void lightning_image_native_case(const xl_compat::Bytes &archive, const std::filesystem::path &root)
{
	using namespace xl_compat;
	const auto members = read_hog(archive);
	const auto member = std::find_if(members.begin(), members.end(), [](const auto &entry) { return entry.name == "neptunxl.rl2"; });
	require(member != members.end(), "Lightning image fixture lacks original geometry");
	const auto level = inspect_level(member->bytes);
	const auto player = std::find_if(level.objects.begin(), level.objects.end(), [](const auto &actor) { return actor.type == 4; });
	require(level.level_version == 23 && player != level.objects.end(), "Lightning image fixture requires modern player geometry");
	Bytes bytes = member->bytes;
	xl_test::layout_patch(bytes, level.game_offset + 35, unsigned(bytes.size()));
	xl_test::layout_patch(bytes, level.game_offset + 39, unsigned(level.objects.size() + 1));
	for (const auto &actor : level.objects)
		bytes.insert(bytes.end(), member->bytes.begin() + actor.source_offset, member->bytes.begin() + actor.source_end);
	Bytes actor(member->bytes.begin() + player->source_offset, member->bytes.begin() + player->source_offset + 80);
	actor[0] = 20;
	actor[1] = 1;
	actor[2] = actor[3] = actor[5] = actor[6] = 0;
	actor[4] = 12;
	for (int value : { 300, 50, 18, 5, 1, -1 }) xl_test::layout_put(actor, value, 4);
	for (int value : { 3, 0, 0, 8, 0, 2 }) xl_test::layout_put(actor, value, 2);
	// Authored glow, dynamic light and activation; linked audio has its own owner
	for (int value : { 20, 0, 0, 0, 0, 1, 0, 1, 0, 0, 52, 149, 128, 1 }) xl_test::layout_put(actor, value, 1);
	bytes.insert(bytes.end(), actor.begin(), actor.end());
	const int owner = int(level.objects.size());
	const auto incoming_level = inspect_level(bytes);
	require(incoming_level.objects.at(owner).render_data.size() == 26 && incoming_level.objects.at(owner).render_data[17] == 1,
	        "Lightning image fixture changed its authored glow");
	Bytes package{ 'D', 'H', 'F' };
	xl_test::hog_member(package, "cload.rl2", bytes, false);
	for (const auto &entry : members) {
		if (entry.name.size() <= 8 || entry.name.substr(0, 8) != "neptunxl") continue;
		const auto extension = entry.name.substr(8);
		if (extension != ".rl2" && extension != ".mn2") xl_test::hog_member(package, "cload" + extension, entry.bytes, false);
	}
	write_bytes((root / "cload.hog").string().c_str(), package);
	{
		std::ofstream descriptor(root / "cload.mn2");
		descriptor << "d2x-name = Corona load controls\nnum_levels = 1\ncload.rl2\n";
		require(bool(descriptor), "Cannot write lightning image descriptor");
	}
	char error[256]{};
	require(load_base_mission("cload", error, sizeof(error)), error);
	StartNewGame(1);
	require(!xl_prepared_lightning_image(), "Missing corona control inherited another world's image");
	auto activation = lightning_objects_snapshot();
	const auto live = [&] {
		const auto sim = d_rand_get_stream_call_count(D_RNG_SIM);
		lightning_objects_frame();
		const auto *emitter = lightning_object_emitter(owner);
		require(emitter && emitter->enabled && emitter->bolts.size() == 3 && lightning_objects_snapshot() == activation &&
		            d_rand_get_stream_call_count(D_RNG_SIM) == sim,
		        "Corona availability changed authored lightning simulation or SIM RNG");
	};
	live();
	char save[] = "xl-corona-absent.sav", description[64] = "Missing corona control";
	stop_time();
	require(state_save_all_sub(save, description), "Cannot save missing-corona world");
	require(load_base_mission("fxstock", error, sizeof(error)), error);
	StartNewGame(1);
	require(state_restore_all_sub(save, 0) && !xl_prepared_lightning_image() && lightning_objects_snapshot() == activation,
	        "Disk restore through stock changed missing-corona ownership");
	live();
	PHYSFS_delete(save);
	const auto path = root.parent_path() / addon_mission_texture_path(mission_resource_archive(), "corona");
	require(!std::filesystem::exists(path), "Corona fixture would replace an existing asset");
	std::filesystem::create_directories(path.parent_path());
	Bytes image(18 + 8 * 8 * 3, 71);
	std::fill_n(image.begin(), 18, 0);
	image[2] = 2;
	image[12] = image[14] = 8;
	image[16] = 24;
	write_bytes(path.string().c_str(), image);
	StartNewGame(1);
	activation = lightning_objects_snapshot();
	const auto *published = xl_prepared_lightning_image();
	require(published && published->bitmap.pixels[0] == 71, "Corona commit lost its decoded owner");
	{
		XlPreparedResources candidate("cload.rl2", &incoming_level, 1);
		const auto *retained = candidate.lightning_image();
		XlPreparedResources moved(std::move(candidate));
		candidate = std::move(moved);
		require(retained && candidate.lightning_image() == retained && retained->bitmap.pixels == published->bitmap.pixels,
		        "Corona moves lost decoded ownership");
		std::filesystem::remove(path);
		XlPreparedResources absent("cload.rl2", &incoming_level, 1);
		require(!absent.lightning_image() && retained->bitmap.pixels[0] == 71 && xl_prepared_lightning_image() == published,
		        "Missing candidate borrowed outgoing corona or changed live ownership");
	}
	require(xl_prepared_lightning_image() == published && published->bitmap.pixels[0] == 71, "Discard changed live corona ownership");
	live();
	require(load_base_mission("fxstock", error, sizeof(error)), error);
	StartNewGame(1);
	require(!xl_prepared_lightning_image(), "Stock world retained prior corona ownership");
	std::cout << "XL corona images: real missing load, disk through stock, authored live emission, decoded commit/moves/discard and deleted-source continuation passed\n";
}
