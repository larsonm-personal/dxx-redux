// Derived normal/secret mapping on unchanged Neptune geometry; not a campaign exit
#include "../tests/hog_fixture.h"
extern "C" void EnterSecretLevel(void);
static void particle_secret_native_case(const xl_compat::Bytes &original_archive, const std::filesystem::path &root, const std::filesystem::path &installation)
{
	using namespace xl_compat;
	const auto members = read_hog(original_archive);
	const auto found = std::find_if(members.begin(), members.end(), [](const auto &member) { return member.name == "neptunxl.rl2"; });
	require(found != members.end(), "Particle secret fixture lacks original Neptune geometry");
	const auto &source = found->bytes;
	const auto level = inspect_level(source);
	const auto player = std::find_if(level.objects.begin(), level.objects.end(), [](const auto &object) { return object.type == 4; });
	require(level.level_version == 23 && player != level.objects.end(), "Particle secret fixture requires modern Neptune player geometry");
	Bytes bytes = source;
	xl_test::layout_patch(bytes, level.game_offset + 35, unsigned(bytes.size()));
	xl_test::layout_patch(bytes, level.game_offset + 39, unsigned(level.objects.size() + 2));
	for (const auto &object : level.objects)
		bytes.insert(bytes.end(), source.begin() + object.source_offset, source.begin() + object.source_end);
	for (unsigned i = 0; i < 2; ++i) {
		Bytes object(source.begin() + player->source_offset, source.begin() + player->source_offset + 80);
		object[0] = 20;
		object[1] = object[2] = object[3] = object[5] = object[6] = 0;
		object[4] = 11;
		for (int value : { 1, 1, 1, 1, 1, 10 }) xl_test::layout_put(object, value, 4);
		for (int value : { 255, 128, 0, 128, int(i), 3, int(!i) }) xl_test::layout_put(object, value, 1);
		bytes.insert(bytes.end(), object.begin(), object.end());
	}
	Bytes archive{ 'D', 'H', 'F' };
	for (const std::string name : { "pbase", "psecret" }) {
		xl_test::hog_member(archive, name + ".rl2", bytes, false);
		for (const auto &member : members) {
			if (member.name.size() <= 8 || member.name.substr(0, 8) != "neptunxl") continue;
			const auto extension = member.name.substr(8);
			if (extension != ".rl2" && extension != ".mn2") xl_test::hog_member(archive, name + extension, member.bytes, false);
		}
	}
	write_bytes((root / "pfsecret.hog").string().c_str(), archive);
	std::ofstream descriptor(root / "pfsecret.mn2");
	descriptor << "d2x-name = Derived fire secrets\nnum_levels = 1\npbase.rl2\nnum_secrets = 1\npsecret.rl2,1\n";
	descriptor.close();
	Bytes image(18 + 8 * 8 * 3, 71);
	std::fill_n(image.begin(), 18, 0);
	image[2] = 2;
	image[12] = image[14] = 8;
	image[16] = 24;
	const auto image_path = installation / "textures/d2x-xl/smokingfire.tga";
	std::filesystem::create_directories(image_path.parent_path());
	std::filesystem::remove(image_path);
	xl_preview_clear_mission();
	char error[256]{};
	require(load_base_mission("pfsecret", error, sizeof(error)), error);
	require(!xl_level_supported_at("pbase.rl2", 0, 1) && std::strstr(xl_level_rejection(), "Missing XL fire image"),
	        "Normal fire admission bypassed the required atlas");
	write_bytes(image_path.string().c_str(), image);
	require(xl_level_supported_at("pbase.rl2", 0, 1), "Normal fire admission did not recover after restoring its atlas");
	PHYSFS_delete(SECRETB_FILENAME);
	PHYSFS_delete(SECRETC_FILENAME);
	input_demo_set_skip_level_intro(1);
	StartNewGame(1);
	require(xl_prepared_particle_image() != nullptr, "Derived particle base lacks its image");
	const auto image_pixels = xl_prepared_particle_image()->bitmap.pixels;
	const int first = int(level.objects.size()), second = first + 1;
	const auto verify = [&](int ordinal, const nlohmann::json &state) {
		require(Current_level_num == ordinal && xl_level_extensions_active() && particle_objects_snapshot() == state,
		        "Secret transition lost authored particle ownership/activation");
		require(xl_prepared_particle_image() && xl_prepared_particle_image()->bitmap.pixels == image_pixels,
		        "Secret transition lost the prepared particle image");
		require(particle_systems().empty(), "Secret restore retained transient particle systems");
	};
	const auto emit = [&](unsigned expected) {
		particle_objects_frame(0);
		particle_objects_frame(25);
		require(particle_systems().size() == expected, "Secret transition resumed the wrong particle owners");
		unsigned live = 0;
		for (const auto &system : particle_systems()) live += unsigned(system.emitter.count);
		require(live > 0, "Secret transition owners did not resume emission");
	};
	require(Current_level_num == 1 && particle_objects_snapshot().size() == 2, "Derived particle base did not load two owners");
	require(xl_particle_object_set_enabled(first, 0) && xl_particle_object_set_enabled(second, 1), "Cannot establish distinct base activation");
	const auto base = particle_objects_snapshot();
	emit(1);
	EnterSecretLevel();
	require(Current_level_num == -1 && PHYSFSX_exists(SECRETB_FILENAME, 0), "Real secret entry did not create its automatic base save");
	const auto fresh = particle_objects_snapshot();
	require(fresh.size() == 2 && fresh[0][2].get<bool>() && !fresh[1][2].get<bool>() && particle_systems().empty(),
	        "First secret entry inherited base activation or transient clouds");
	require(xl_particle_object_set_enabled(first, 1) && xl_particle_object_set_enabled(second, 1), "Cannot establish distinct secret activation");
	const auto secret = particle_objects_snapshot();
	emit(2);
	ExitSecretLevel();
	verify(1, base);
	require(PHYSFSX_exists(SECRETC_FILENAME, 0), "Real secret exit did not create its automatic secret save");
	emit(1);
	EnterSecretLevel();
	verify(-1, secret);
	emit(2);
	ExitSecretLevel();
	verify(1, base);
	xl_preview_clear_mission();
	std::filesystem::remove(image_path);
	PHYSFS_delete(SECRETB_FILENAME);
	PHYSFS_delete(SECRETC_FILENAME);
	std::cout << "XL particle secret lifecycle: real automatic base/secret saves, two entries/returns, distinct activation, fresh clouds and resumed emission passed on derived geometry\n";
}
