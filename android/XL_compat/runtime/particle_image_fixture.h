#ifndef DXX_XL_PARTICLE_IMAGE_FIXTURE_H
#define DXX_XL_PARTICLE_IMAGE_FIXTURE_H
#include "particle_resources.h"
#include "../scoped_texture.h"
#include "../particle_image.h"

static xl_compat::ParticleImages particle_fixture_images(const xl_compat::Inventory &level)
{
	using namespace xl_compat;
	ParticleImages images;
	const auto root = std::filesystem::path(mission_resource_archive()).parent_path().parent_path();
	for (const int subtype : { 0, 2, 3, 5, 6 }) {
		const char *name = subtype == 0 ? smoke_image_name : subtype == 2 ? bubble_image_name
		                                                 : subtype == 3   ? fire_image_name
		                                                 : subtype == 5   ? rain_image_name
		                                                                  : snow_image_name;
		const auto path = root / addon_mission_texture_path(mission_resource_archive(), name);
		require(!std::filesystem::exists(path), "Particle image fixture would replace an existing input");
		std::filesystem::create_directories(path.parent_path());
		const unsigned extent = subtype == 3 ? 8 : 32;
		Bytes bytes(18 + extent * extent * 3, 47);
		std::fill(bytes.begin(), bytes.begin() + 18, 0);
		bytes[2] = 2;
		bytes[12] = bytes[14] = static_cast<unsigned char>(extent);
		bytes[16] = 24;
		write_bytes(path.string().c_str(), bytes);
		struct Remove {
			std::filesystem::path path;
			~Remove()
			{
				std::filesystem::remove(path);
			}
		} remove{ path };
		images[subtype] = prepare_particle_image(level, subtype);
	}
	images[1] = images[4] = images[0];
	return images;
}
#endif
