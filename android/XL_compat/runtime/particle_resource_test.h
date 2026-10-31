#include "particle_resources.h"
#include "resource_validation.h"
#include "../scoped_texture.h"

static void particle_resource_native_case(const std::filesystem::path &root)
{
	using namespace xl_compat;
	require(xl_level_extensions_active(), "Particle image controls require a loaded XL mission");
	Inventory level;
	require(!prepare_particle_image(level), "Level without particles requested an addon image");
	Object object{};
	object.render = 11;
	object.render_data.resize(13);
	object.render_data[11] = 3;
	level.objects.push_back(object);
	const auto mod = addon_mission_texture_path(mission_resource_archive(), "smokingfire");
	require(!mod.empty(), "Particle image fixture has no selected HOG path");
	const std::string global = "textures/d2x-xl/smokingfire.tga";
	const auto mod_path = root / mod, global_path = root / global;
	std::filesystem::create_directories(mod_path.parent_path());
	std::filesystem::create_directories(global_path.parent_path());
	std::filesystem::remove(mod_path);
	std::filesystem::remove(global_path);
	const auto failure = [&](const char *reason) {
		try {
			(void) prepare_particle_image(level);
		} catch (const std::exception &error) {
			require(std::strstr(error.what(), reason), "Particle image rejection lost its useful reason");
			return;
		}
		throw std::runtime_error("Particle image preparation unexpectedly accepted a bad resource");
	};
	failure("Missing XL fire image");
	const auto image = [](unsigned char red) {
		Bytes bytes(18 + 8 * 8 * 3, 0);
		bytes[2] = 2;
		bytes[12] = bytes[14] = 8;
		bytes[16] = 24;
		for (std::size_t i = 18; i < bytes.size(); i += 3) {
			bytes[i] = 7;
			bytes[i + 1] = 11;
			bytes[i + 2] = red;
		}
		return bytes;
	};
	write_bytes(global_path.string().c_str(), image(23));
	const auto fallback = prepare_particle_image(level);
	require(fallback && fallback->path == global && fallback->bitmap.pixels[0] == 23 && fallback->bitmap.pixels[3] == 255, "Owned default particle image was not decoded");
	write_bytes(mod_path.string().c_str(), image(47));
	const auto selected = prepare_particle_image(level);
	require(selected && selected->path == mod && selected->bitmap.pixels[0] == 47, "Mission particle image did not take precedence over the default");
	// Existing mod files do not silently fall through when malformed
	write_bytes(mod_path.string().c_str(), { 0, 1, 2 });
	failure("truncated");
	require(selected->bitmap.pixels[0] == 47 && fallback->bitmap.pixels[0] == 23, "Failed preparation changed already owned particle pixels");
	write_bytes(mod_path.string().c_str(), image(47));
	const auto foreign = root / "foreign-images";
	std::filesystem::create_directories((foreign / mod).parent_path());
	write_bytes((foreign / mod).string().c_str(), image(99));
	const auto foreign_name = foreign.string();
	require(PHYSFS_addToSearchPath(foreign_name.c_str(), 0), "Cannot mount particle image shadow control");
	try {
		failure("shadowed outside");
		std::filesystem::remove(foreign / mod);
		std::filesystem::remove(mod_path);
		std::filesystem::create_directories((foreign / global).parent_path());
		write_bytes((foreign / global).string().c_str(), image(101));
		failure("shadowed outside");
	} catch (...) {
		PHYSFS_removeFromSearchPath(foreign_name.c_str());
		throw;
	}
	require(PHYSFS_removeFromSearchPath(foreign_name.c_str()), "Cannot unmount particle image shadow control");
	require(prepare_particle_image(level)->bitmap.pixels[0] == 23, "Particle default image shadow rejection prevented recovery");
	write_bytes(mod_path.string().c_str(), image(47));
	const auto mixed_case = mod_path.parent_path() / "SmokingFire.TGA";
	std::filesystem::rename(mod_path, mixed_case);
	require(prepare_particle_image(level)->bitmap.pixels[0] == 47, "Particle addon lookup lost source case-insensitive behavior");
	std::filesystem::remove(mixed_case);
	std::filesystem::remove(global_path);
	failure("Missing XL fire image");
	require(selected->bitmap.pixels.size() == 8 * 8 * 4 && selected->bitmap.pixels[0] == 47, "Particle image storage depended on a live file");
	require(!xl_prepared_particle_image(), "Particle resource probe replaced the current level's prepared images");
	std::cout << "XL particle image resources: owned default/mod precedence, missing/malformed/default-and-mod-shadow rejection, case-insensitive lookup, retained pixels and recovery passed\n";
}
