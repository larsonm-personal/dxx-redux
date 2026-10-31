#include "particle_resources.h"
#include "particle_renderer.h"
#include "resource_validation.h"
#include "../inventory.h"
#include "../particle_image.h"
#include "../scoped_texture.h"
#ifdef __ANDROID__
#include "android_mission_assets.h"
#endif
extern "C" {
#include "physfsx.h"
#include "mission.h"
}

namespace xl_compat
{
ParticleImageResource::ParticleImageResource() = default;
ParticleImageResource::~ParticleImageResource() = default;
namespace
{
bool locate(std::string &path, bool installation)
{
	if (PHYSFSEXT_locateCorrectCase(path.data()) != 0) return false;
	const char *origin = PHYSFS_getRealDir(path.c_str());
	bool owned = mission_owns_resource_directory(origin) != 0;
#ifdef __ANDROID__
	if (*android_mission_assets_key()) {
		char resolved[PATH_MAX];
		owned = android_mission_assets_find_owned_file(path.c_str(), resolved, sizeof(resolved)) != 0;
		if (owned) path = resolved;
	}
#endif
	if (!owned && !(installation && xl_resource_owns_installation(origin)))
		throw std::runtime_error("XL particle image is shadowed outside the selected mission or base installation: " + path);
	PHYSFS_Stat stat;
	if (!PHYSFS_stat(path.c_str(), &stat) || stat.filetype != PHYSFS_FILETYPE_REGULAR)
		throw std::runtime_error("XL particle image is not a regular file: " + path);
	return true;
}
} // namespace

std::shared_ptr<const ParticleImageResource> prepare_particle_image(const Inventory &level, int subtype)
{
	if (subtype != 0 && subtype != 3) throw std::runtime_error("Unsupported XL particle image subtype");
	const char *name = subtype == 3 ? fire_image_name : smoke_image_name;
	const unsigned atlas_side = subtype == 3 ? fire_image_atlas_side : smoke_image_atlas_side;
	bool required = false;
	for (const auto &object : level.objects) {
		if (object.render != 11) continue;
		if (object.render_data.size() != 13 || (object.render_data[11] != 3 && object.render_data[11] != 0))
			throw std::runtime_error("XL particle image preparation requires a supported authored fire or smoke payload");
		required |= object.render_data[11] == subtype;
	}
	if (!required) return {};
	auto mission = addon_mission_texture_path(mission_resource_archive(), name);
	if (mission.empty()) throw std::runtime_error("XL particle images require a selected mission HOG identity");
	std::string selected = mission;
	if (!locate(selected, false)) {
		selected = std::string("textures/d2x-xl/") + name + ".tga";
		if (!locate(selected, true)) throw std::runtime_error(std::string("Missing XL ") + (subtype == 3 ? "fire" : "smoke") + " image; checked " + mission + " and " + selected + " in the selected mission or base installation");
	}
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead(selected.c_str()), PHYSFS_close);
	const auto length = file ? PHYSFS_fileLength(file.get()) : -1;
	if (length < 18 || std::uint64_t(length) > pog_file_limit + 65536)
		throw std::runtime_error("XL particle image file is missing, oversized or truncated: " + selected);
	Bytes bytes(static_cast<std::size_t>(length));
	if (PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) != length)
		throw std::runtime_error("Cannot read complete XL particle image: " + selected);
	auto image = std::make_shared<ParticleImageResource>();
	image->path = std::move(selected);
	image->bitmap = read_particle_image(bytes.data(), bytes.size());
	const unsigned width = image->bitmap.width;
	if (width < 2 * atlas_side || image->bitmap.height != width || (width & (width - 1)))
		throw std::runtime_error("XL particle image requires a square power-of-two " + std::to_string(atlas_side) + "-by-" + std::to_string(atlas_side) + " atlas with room for its one-pixel cell borders: " + image->path);
	image->texture = std::make_unique<ParticleTexture>(image->bitmap, image->path.c_str());
	return image;
}
} // namespace xl_compat
