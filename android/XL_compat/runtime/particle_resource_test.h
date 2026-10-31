#include "particle_resources.h"
#include "resource_validation.h"
#include "../scoped_texture.h"

static void particle_resource_profile(const std::filesystem::path &root, const std::filesystem::path &installation, int subtype, bool spray = false)
{
	using namespace xl_compat;
	require(xl_level_extensions_active(), "Particle image controls require a loaded XL mission");
	const char *name = subtype == 3 ? "smokingfire" : "smoke";
	const char *missing = subtype == 3 ? "Missing XL fire image" : "Missing XL smoke image";
	const unsigned extent = subtype == 3 ? 8 : 32;
	Inventory level;
	require(!prepare_particle_image(level, subtype), "Level without particles requested an addon image");
	Object object{};
	object.render = 11;
	object.render_data.resize(13);
	object.render_data[11] = spray ? 1 : subtype;
	level.objects.push_back(object);
	const auto mod = addon_mission_texture_path(mission_resource_archive(), name);
	require(!mod.empty(), "Particle image fixture has no selected HOG path");
	const std::string global = std::string("textures/d2x-xl/") + name + ".tga";
	const auto mod_path = root / mod, global_path = root / global;
	require(std::filesystem::equivalent(installation, root / "installation"), "Particle installation fixture must be inside the isolated test output");
	const auto installation_path = installation / global;
	std::filesystem::create_directories(installation_path.parent_path());
	std::filesystem::remove(installation_path);
	std::filesystem::create_directories(mod_path.parent_path());
	std::filesystem::create_directories(global_path.parent_path());
	std::filesystem::remove(mod_path);
	std::filesystem::remove(global_path);
	const auto failure = [&](const char *reason) {
		try {
			(void) prepare_particle_image(level, subtype);
		} catch (const std::exception &error) {
			require(std::strstr(error.what(), reason), "Particle image rejection lost its useful reason");
			return;
		}
		throw std::runtime_error("Particle image preparation unexpectedly accepted a bad resource");
	};
	failure(missing);
	const auto image = [extent](unsigned char red) {
		Bytes bytes(18 + extent * extent * 3, 0);
		bytes[2] = 2;
		bytes[12] = bytes[14] = static_cast<unsigned char>(extent);
		bytes[16] = 24;
		for (std::size_t i = 18; i < bytes.size(); i += 3) {
			bytes[i] = 7;
			bytes[i + 1] = 11;
			bytes[i + 2] = red;
		}
		return bytes;
	};
	write_bytes(global_path.string().c_str(), image(23));
	const auto fallback = prepare_particle_image(level, subtype);
	require(fallback && fallback->path == global && fallback->bitmap.pixels[0] == 23 && fallback->bitmap.pixels[3] == 255, "Owned default particle image was not decoded");
	write_bytes(mod_path.string().c_str(), image(47));
	const auto selected = prepare_particle_image(level, subtype);
	require(selected && selected->path == mod && selected->bitmap.pixels[0] == 47, "Mission particle image did not take precedence over the default");
	require(!prepare_particle_image(level, subtype == 3 ? 0 : 3), "Particle image request selected the other authored subtype");
	if (subtype == 0) {
		Inventory mixed = level;
		auto fire = object;
		fire.render_data[11] = 3;
		mixed.objects.push_back(fire);
		const auto fire_path = root / "textures/d2x-xl/smokingfire.tga";
		write_bytes(fire_path.string().c_str(), image(83));
		const auto fire_image = prepare_particle_image(mixed, 3), smoke_image = prepare_particle_image(mixed, 0);
		require(fire_image && smoke_image && fire_image != smoke_image && fire_image->bitmap.pixels[0] == 83 && smoke_image->bitmap.pixels[0] == 47, "Mixed fire/smoke preparation aliased or selected the wrong pixels");
		std::filesystem::remove(fire_path);
		require(fire_image->bitmap.pixels[0] == 83 && smoke_image->bitmap.pixels[0] == 47, "Mixed particle image ownership depended on live files");
	}
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
	require(prepare_particle_image(level, subtype)->bitmap.pixels[0] == 23, "Particle default image shadow rejection prevented recovery");
	write_bytes(mod_path.string().c_str(), image(47));
	const auto mixed_case = mod_path.parent_path() / (subtype == 3 ? "SmokingFire.TGA" : "Smoke.TGA");
	std::filesystem::rename(mod_path, mixed_case);
	require(prepare_particle_image(level, subtype)->bitmap.pixels[0] == 47, "Particle addon lookup lost source case-insensitive behavior");
	std::filesystem::remove(mixed_case);
	std::filesystem::remove(global_path);
	failure(missing);
	write_bytes(installation_path.string().c_str(), image(61));
	const char *installation_origin = PHYSFS_getRealDir(global.c_str());
	require(installation_origin && xl_resource_owns_installation(installation_origin) && !mission_owns_resource_directory(installation_origin), "Particle fallback did not use a distinct trusted startup installation");
	const auto installed = prepare_particle_image(level, subtype);
	require(installed && installed->path == global && installed->bitmap.pixels[0] == 61, "Startup installation particle image was not decoded");
	write_bytes(global_path.string().c_str(), image(23));
	require(prepare_particle_image(level, subtype)->bitmap.pixels[0] == 23, "Mission-owned default did not override startup installation image");
	write_bytes(mod_path.string().c_str(), image(47));
	require(prepare_particle_image(level, subtype)->bitmap.pixels[0] == 47, "Mission mod did not override startup installation image");
	write_bytes(mod_path.string().c_str(), { 0, 1, 2 });
	failure("truncated");
	std::filesystem::remove(mod_path);
	std::filesystem::remove(global_path);
	require(PHYSFS_addToSearchPath(foreign_name.c_str(), 0), "Cannot mount installation image shadow control");
	try {
		failure("shadowed outside");
	} catch (...) {
		PHYSFS_removeFromSearchPath(foreign_name.c_str());
		throw;
	}
	require(PHYSFS_removeFromSearchPath(foreign_name.c_str()), "Cannot unmount installation image shadow control");
	require(prepare_particle_image(level, subtype)->bitmap.pixels[0] == 61, "Installation shadow rejection prevented recovery");
	write_bytes(installation_path.string().c_str(), { 0, 1, 2 });
	failure("truncated");
	std::filesystem::remove(installation_path);
	failure(missing);
	require(installed->bitmap.pixels[0] == 61, "Installation image storage depended on a live file");
	require(selected->bitmap.pixels.size() == extent * extent * 4 && selected->bitmap.pixels[0] == 47, "Particle image storage depended on a live file");
	require(!xl_prepared_particle_image(), "Particle resource probe replaced the current level's prepared images");
	std::cout << "XL particle image resources: distinct startup installation, owned default/mod precedence, missing/malformed/default-and-mod-shadow rejection, case-insensitive lookup, retained pixels and recovery passed\n";
}

static void particle_resource_native_case(const std::filesystem::path &root, const std::filesystem::path &installation)
{
	particle_resource_profile(root, installation, 3);
	particle_resource_profile(root, installation, 0);
	particle_resource_profile(root, installation, 0, true);
}
