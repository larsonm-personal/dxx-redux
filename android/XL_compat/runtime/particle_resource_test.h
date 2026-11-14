#include "particle_resources.h"
#include "resource_validation.h"
#include "../scoped_texture.h"
#include "../particle_image.h"
#include "target_sources.h"
#include "mission_prepare.h"

template <bool Shield = false>
static void particle_resource_profile(const std::filesystem::path &root, const std::filesystem::path &installation, int subtype, bool spray = false, bool lightning = false)
{
	using namespace xl_compat;
	const auto prepare = [&](const Inventory &level) {
		if constexpr (Shield) return prepare_shield_image(unsigned(subtype));
		else return lightning ? prepare_lightning_image(level) : prepare_particle_image(level, subtype);
	};
	require(xl_level_extensions_active(), "Particle image controls require a loaded XL mission");
	const char *name = Shield         ? (subtype == 0 ? "forcefield" : subtype == 1 ? "forcefield2"
	                                                                                : "forcefield3")
	                   : lightning    ? "corona"
	                   : subtype == 2 ? "bubble"
	                   : subtype == 3 ? "smokingfire"
	                                  : "smoke";
	const char *missing = Shield ? "Missing XL shield image" : lightning  ? "Missing XL lightning corona image"
	                                                       : subtype == 2 ? "Missing XL bubble image"
	                                                       : subtype == 3 ? "Missing XL fire image"
	                                                                      : "Missing XL smoke image";
	const unsigned extent = Shield || lightning || subtype == 3 ? 8 : 32;
	const unsigned frames = Shield ? 2 : 1;
	Inventory level;
	if constexpr (!Shield) require(!prepare(level), "Level without particles requested an addon image");
	Object object{};
	object.render = 11;
	object.render_data.resize(13);
	object.render_data[11] = spray ? 1 : subtype;
	if (lightning) {
		level.segments.resize(1);
		object.type = 20;
		object.id = object.reader_id = 1;
		object.render = object.reader_render = 12;
		object.render_data = { 300, 50, 18, 5, 1, -1, 3, 0, 0, 8, 0, 2, 20, 0, 0, 0, 0, 1, 1, 1, 0, 0, 52, 149, 128, 1 };
	}
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
			(void) prepare(level);
		} catch (const std::exception &error) {
			require(std::strstr(error.what(), reason), "Particle image rejection lost its useful reason");
			return;
		}
		throw std::runtime_error("Particle image preparation unexpectedly accepted a bad resource");
	};
	failure(missing);
	if (lightning) {
		auto nonglow = level;
		nonglow.objects[0].render_data[17] = 0;
		require(!prepare(nonglow), "Non-glowing lightning required an absent corona image");
	}
	const auto image = [extent, frames](unsigned char red) {
		Bytes bytes(18 + extent * extent * frames * 3, 0);
		bytes[2] = 2;
		bytes[12] = static_cast<unsigned char>(extent);
		bytes[14] = static_cast<unsigned char>(extent * frames);
		bytes[16] = 24;
		for (std::size_t i = 18; i < bytes.size(); i += 3) {
			bytes[i] = 7;
			bytes[i + 1] = 11;
			bytes[i + 2] = red;
		}
		return bytes;
	};
	write_bytes(global_path.string().c_str(), image(23));
	const auto fallback = prepare(level);
	require(fallback && fallback->path == global && fallback->bitmap.pixels[0] == 23 && fallback->bitmap.pixels[3] == 255, "Owned default particle image was not decoded");
	if constexpr (!Shield) {
		if (subtype == 3 && !lightning) {
			// Independent PNG encoding of the same synthetic RGBA rows tests orientation and soft alpha
			const Bytes png{ 137, 80, 78, 71, 13, 10, 26, 10, 0, 0, 0, 13, 73, 72, 68, 82, 0, 0, 0, 8, 0, 0, 0, 8, 8, 6, 0, 0, 0, 196, 15, 190, 139, 0, 0, 0, 133, 73, 68, 65, 84, 120, 156, 21, 202, 49, 13, 3, 32, 16, 0, 192, 183, 80, 1, 44, 181, 208, 13, 150, 14, 181, 64, 112, 128, 2, 18, 214, 90, 192, 8, 70, 16, 213, 30, 227, 37, 23, 17, 37, 30, 81, 222, 207, 40, 223, 87, 148, 223, 135, 43, 119, 158, 28, 145, 132, 36, 36, 33, 9, 92, 185, 243, 76, 55, 100, 33, 11, 89, 200, 2, 87, 238, 60, 243, 13, 77, 104, 66, 19, 154, 192, 149, 59, 207, 118, 195, 16, 134, 48, 132, 33, 112, 229, 206, 115, 220, 176, 132, 37, 44, 97, 9, 92, 185, 243, 92, 55, 108, 97, 11, 91, 216, 2, 87, 238, 60, 247, 13, 71, 56, 194, 17, 142, 192, 149, 59, 79, 254, 3, 15, 187, 82, 241, 215, 198, 190, 58, 0, 0, 0, 0, 73, 69, 78, 68, 174, 66, 96, 130 };
			Bytes tga(18 + 8 * 8 * 4);
			tga[2] = 2;
			tga[10] = 1;
			tga[12] = tga[14] = 8;
			tga[16] = 32;
			const unsigned char alpha[] = { 0, 64, 128, 255 };
			for (unsigned y = 0; y < 8; ++y)
				for (unsigned x = 0; x < 8; ++x) {
					const auto at = 18 + (y * 8 + x) * 4;
					tga[at] = x * 17;
					tga[at + 1] = y * 29;
					tga[at + 2] = 59;
					tga[at + 3] = alpha[x % 4];
				}
			const auto png_path = root / (global.substr(0, global.size() - 4) + ".png");
			std::filesystem::remove(global_path);
			write_bytes(png_path.string().c_str(), png);
			const auto decoded = prepare(level);
			require(decoded && decoded->bitmap.pixels == read_particle_image(tga.data(), tga.size()).pixels,
			        "PNG effect changed source RGBA orientation or soft alpha");
			{
				auto *mission = mission_prepare_by_name(Current_mission_filename);
				require(mission, "Cannot retain PNG target descriptor");
				struct Release {
					mission_preparation **mission;
					~Release()
					{
						mission_discard_prepared(mission);
					}
				} release{ &mission };
				TargetSources target(mission, installation.string());
				const auto shadow = root / "incoming-png-shadow";
				const auto shadow_png = shadow / (global.substr(0, global.size() - 4) + ".png");
				std::filesystem::create_directories(shadow_png.parent_path());
				write_bytes(shadow_png.string().c_str(), { 'b', 'a', 'd' });
				const auto physical = shadow.string();
				require(PHYSFS_mount(physical.c_str(), nullptr, 0), "Cannot mount logical PNG shadow");
				struct Unmount {
					const std::string &physical;
					~Unmount()
					{
						PHYSFS_unmount(physical.c_str());
					}
				} unmount{ physical };
				const auto incoming = prepare_particle_image(level, subtype, &target);
				require(incoming && incoming->bitmap.pixels == decoded->bitmap.pixels,
				        "Incoming PNG decoder reopened a shadowing logical filename");
			}
			write_bytes(mod_path.string().c_str(), image(47));
			require(prepare(level)->bitmap.pixels[0] == 47, "Mission TGA did not override installed PNG");
			std::filesystem::remove(mod_path);
			std::filesystem::remove(png_path);
			write_bytes(global_path.string().c_str(), image(23));
		}
	}
	write_bytes(mod_path.string().c_str(), image(47));
	const auto selected = prepare(level);
	require(selected && selected->path == mod && selected->bitmap.pixels[0] == 47, "Mission particle image did not take precedence over the default");
	{
		const auto directory = root / "incoming-images" / (std::string(name) + (spray ? "-spray" : ""));
		std::filesystem::create_directories(directory);
		const auto archive_name = std::filesystem::path(mission_resource_archive()).filename();
		auto descriptor_name = archive_name;
		descriptor_name.replace_extension(".mn2");
		auto *source = mission_prepare_by_name(Current_mission_filename);
		require(source, "Cannot retain particle source descriptor");
		{
			std::ofstream descriptor(directory / descriptor_name);
			descriptor << mission_prepared_descriptor_text(source);
			require(bool(descriptor), "Cannot write independent image descriptor");
		}
		require(mission_discard_prepared(&source), "Cannot release particle source descriptor");
		std::filesystem::copy_file(mission_resource_archive(), directory / archive_name, std::filesystem::copy_options::overwrite_existing);
		const auto physical = std::filesystem::absolute(directory).string();
		require(PHYSFS_mount(physical.c_str(), "__incoming_images", 1), "Cannot mount image descriptor context");
		auto *mission = mission_prepare_from_current_dir(("__incoming_images/" + descriptor_name.string()).c_str());
		struct Release {
			mission_preparation **mission;
			~Release()
			{
				mission_discard_prepared(mission);
			}
		} release{ &mission };
		require(mission && PHYSFS_unmount(physical.c_str()), "Cannot retain isolated image descriptor");
		const auto incoming = [&] {
			TargetSources target(mission, installation.string());
			if constexpr (Shield) return prepare_shield_image(unsigned(subtype), &target);
			else return lightning ? prepare_lightning_image(level, &target) : prepare_particle_image(level, subtype, &target);
		};
		bool rejected = false;
		try {
			(void) incoming();
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected, "Absent incoming image borrowed outgoing pixels");
		const auto path = directory / mod;
		std::filesystem::create_directories(path.parent_path());
		write_bytes(path.string().c_str(), image(79));
		const auto retained = incoming();
		require(retained && retained->bitmap.pixels[0] == 79 && selected->bitmap.pixels[0] == 47,
		        "Incoming image selected the outgoing equal-name source");
		write_bytes(path.string().c_str(), { 0, 1, 2 });
		rejected = false;
		try {
			(void) incoming();
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		require(rejected && retained->bitmap.pixels[0] == 79 && selected->bitmap.pixels[0] == 47,
		        "Incoming image rejection changed retained pixels or used a fallback");
	}
	require(!prepare_particle_image(level, subtype == 3 ? 0 : 3), "Particle image request selected the other authored subtype");
	if (!Shield && !lightning && subtype == 0) {
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
	require(prepare(level)->bitmap.pixels[0] == 23, "Particle default image shadow rejection prevented recovery");
	write_bytes(mod_path.string().c_str(), image(47));
	const auto mixed_case = mod_path.parent_path() / (Shield         ? (subtype == 0 ? "ForceField.TGA" : subtype == 1 ? "ForceField2.TGA"
	                                                                                                                   : "ForceField3.TGA")
	                                                  : lightning    ? "Corona.TGA"
	                                                  : subtype == 2 ? "Bubble.TGA"
	                                                  : subtype == 3 ? "SmokingFire.TGA"
	                                                                 : "Smoke.TGA");
	std::filesystem::rename(mod_path, mixed_case);
	require(prepare(level)->bitmap.pixels[0] == 47, "Particle addon lookup lost source case-insensitive behavior");
	std::filesystem::remove(mixed_case);
	std::filesystem::remove(global_path);
	failure(missing);
	write_bytes(installation_path.string().c_str(), image(61));
	const char *installation_origin = PHYSFS_getRealDir(global.c_str());
	require(installation_origin && xl_resource_owns_installation(installation_origin) && !mission_owns_resource_directory(installation_origin), "Particle fallback did not use a distinct trusted startup installation");
	const auto installed = prepare(level);
	require(installed && installed->path == global && installed->bitmap.pixels[0] == 61, "Startup installation particle image was not decoded");
	write_bytes(global_path.string().c_str(), image(23));
	require(prepare(level)->bitmap.pixels[0] == 23, "Mission-owned default did not override startup installation image");
	write_bytes(mod_path.string().c_str(), image(47));
	require(prepare(level)->bitmap.pixels[0] == 47, "Mission mod did not override startup installation image");
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
	require(prepare(level)->bitmap.pixels[0] == 61, "Installation shadow rejection prevented recovery");
	write_bytes(installation_path.string().c_str(), { 0, 1, 2 });
	failure("truncated");
	std::filesystem::remove(installation_path);
	failure(missing);
	require(installed->bitmap.pixels[0] == 61, "Installation image storage depended on a live file");
	require(selected->bitmap.pixels.size() == extent * extent * frames * 4 && selected->bitmap.frames == frames && selected->bitmap.pixels[0] == 47, "Particle image storage depended on a live file");
	require(!xl_prepared_particle_image(), "Particle resource probe replaced the current level's prepared images");
	std::cout << (Shield ? "XL shield image resources: " : "XL particle image resources: ") << " distinct startup installation, owned default/mod precedence, missing/malformed/default-and-mod-shadow rejection, case-insensitive lookup, retained pixels and recovery passed\n";
}

static void particle_resource_native_case(const std::filesystem::path &root, const std::filesystem::path &installation)
{
	particle_resource_profile(root, installation, 3);
	particle_resource_profile(root, installation, 0);
	particle_resource_profile(root, installation, 0, true);
	particle_resource_profile(root, installation, 2);
	particle_resource_profile(root, installation, 3, false, true);
	for (int index : { 0, 1, 2 }) particle_resource_profile<true>(root, installation, index);
}
