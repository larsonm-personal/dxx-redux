#include "particle_resources.h"
#include "particle_renderer.h"
#include "shield_renderer.h"
#include "resource_validation.h"
#include "../inventory.h"
#include "../particle_image.h"
#include "../lightning_effect.h"
#include "../scoped_texture.h"
#ifdef __ANDROID__
#include "android_mission_assets.h"
#include "stb_image.h"
#else
extern "C" {
#include "pngfile.h"
}
#endif
#include <cstdlib>
#include <cstring>
extern "C" {
#include "physfsx.h"
#include "mission.h"
}

namespace xl_compat
{
ParticleImageResource::ParticleImageResource() = default;
ParticleImageResource::~ParticleImageResource() = default;
ShieldImageResource::ShieldImageResource() = default;
ShieldImageResource::~ShieldImageResource() = default;
namespace
{
PogBitmap read_effect_png(const Bytes &bytes, const std::string &path)
{
#if !defined(__ANDROID__) && !defined(HAVE_LIBPNG)
	(void) bytes;
	throw std::runtime_error("PNG support is unavailable for XL effect image: " + path);
#else
	const unsigned char signature[] = { 137, 80, 78, 71, 13, 10, 26, 10 };
	if (bytes.size() < 33 || std::memcmp(bytes.data(), signature, 8) ||
	    std::memcmp(bytes.data() + 12, "IHDR", 4) || bytes[24] != 8 || (bytes[25] != 2 && bytes[25] != 6))
		throw std::runtime_error("XL effect PNG requires 8-bit RGB or RGBA: " + path);
	const auto word = [&](unsigned at) {
		return std::uint32_t(bytes[at]) << 24 | std::uint32_t(bytes[at + 1]) << 16 |
		       std::uint32_t(bytes[at + 2]) << 8 | bytes[at + 3];
	};
	const unsigned width = word(16), height = word(20);
	if (!width || width > 4096 || !height || height > 32767 ||
	    std::uint64_t(width) * height > pog_file_limit / 4 ||
	    (height > width && (height % width || height / width > 128)))
		throw std::runtime_error("XL effect PNG exceeds image limits: " + path);
#ifdef __ANDROID__
	int w = 0, h = 0, channels = 0;
	std::unique_ptr<unsigned char, decltype(&stbi_image_free)> pixels(
	    stbi_load_from_memory(bytes.data(), int(bytes.size()), &w, &h, &channels, 0), stbi_image_free);
#else
	png_data decoded{};
	const bool loaded = read_png(path.c_str(), &decoded) != 0;
	std::unique_ptr<unsigned char, decltype(&std::free)> pixels(decoded.data, std::free);
	std::free(decoded.palette);
	const unsigned w = decoded.width, h = decoded.height, channels = decoded.channels;
	if (!loaded) throw std::runtime_error("Cannot decode XL effect PNG: " + path);
#endif
	if (!pixels || unsigned(w) != width || unsigned(h) != height || (channels != 3 && channels != 4))
		throw std::runtime_error("Invalid decoded XL effect PNG: " + path);
	// Feed identical raw rows into the existing CTGA alpha/atlas policy. No cosmetic fitting
	Bytes raw(18 + std::size_t(width) * height * channels);
	raw[2] = 2;
	raw[10] = 1; // CTGA's source-order marker: raw RGB, top row first
	raw[12] = width & 255;
	raw[13] = width >> 8;
	raw[14] = height & 255;
	raw[15] = height >> 8;
	raw[16] = channels * 8;
	std::memcpy(raw.data() + 18, pixels.get(), raw.size() - 18);
	pixels.reset();
	return read_particle_image(raw.data(), raw.size());
#endif
}

bool locate(std::string &path, bool installation, const std::string &kind)
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
		throw std::runtime_error("XL " + kind + " image is shadowed outside the selected mission or base installation: " + path);
	PHYSFS_Stat stat;
	if (!PHYSFS_stat(path.c_str(), &stat) || stat.filetype != PHYSFS_FILETYPE_REGULAR)
		throw std::runtime_error("XL " + kind + " image is not a regular file: " + path);
	return true;
}
template <typename Resource>
std::shared_ptr<Resource> read_addon_image(const char *name, const std::string &kind, bool optional)
{
	auto mission = addon_mission_texture_path(mission_resource_archive(), name);
	if (mission.empty()) throw std::runtime_error("XL " + kind + " images require a selected mission HOG identity");
	std::string selected;
	for (bool installation : { false, true }) {
		const auto base = installation ? std::string("textures/d2x-xl/") + name : mission.substr(0, mission.size() - 4);
		for (const char *extension : { ".tga", ".png" }) {
			auto candidate = base + extension;
			if (locate(candidate, installation, kind)) {
				selected = std::move(candidate);
				break;
			}
		}
		if (!selected.empty()) break;
	}
	if (selected.empty()) {
		if (optional) return {};
		throw std::runtime_error("Missing XL " + kind + " image in the selected mission or enabled resource mods: " + name);
	}
	std::unique_ptr<PHYSFS_file, decltype(&PHYSFS_close)> file(PHYSFS_openRead(selected.c_str()), PHYSFS_close);
	const auto length = file ? PHYSFS_fileLength(file.get()) : -1;
	if (length < 18 || std::uint64_t(length) > pog_file_limit + 65536)
		throw std::runtime_error("XL " + kind + " image file is missing, oversized or truncated: " + selected);
	Bytes bytes(static_cast<std::size_t>(length));
	if (PHYSFS_readBytes(file.get(), bytes.data(), bytes.size()) != length)
		throw std::runtime_error("Cannot read complete XL " + kind + " image: " + selected);
	auto image = std::make_shared<Resource>();
	image->path = std::move(selected);
	image->bitmap = bytes[0] == 137 && bytes[1] == 'P' ? read_effect_png(bytes, image->path) : read_particle_image(bytes.data(), bytes.size());
	return image;
}
std::shared_ptr<const ParticleImageResource> prepare_addon_image(const char *name, const std::string &kind, unsigned atlas_side, bool optional = false)
{
	auto image = read_addon_image<ParticleImageResource>(name, kind, optional);
	if (!image) return {};
	const unsigned width = image->bitmap.width;
	if (atlas_side && (width < 2 * atlas_side || image->bitmap.height != width || (width & (width - 1))))
		throw std::runtime_error("XL particle image requires a square power-of-two " + std::to_string(atlas_side) + "-by-" + std::to_string(atlas_side) + " atlas with room for its one-pixel cell borders: " + image->path);
	image->texture = std::make_unique<ParticleTexture>(image->bitmap, image->path.c_str(), !optional);
	return image;
}
} // namespace

std::shared_ptr<const ShieldImageResource> prepare_shield_image(unsigned index)
{
	if (index > 2) throw std::runtime_error("Unsupported XL shield image index");
	const char *names[] = { "forcefield", "forcefield2", "forcefield3" };
	auto image = read_addon_image<ShieldImageResource>(names[index], "shield", false);
	image->texture = std::make_unique<ShieldTexture>(image->bitmap);
	return image;
}

std::shared_ptr<const ParticleImageResource> prepare_particle_image(const Inventory &level, int subtype)
{
	if (subtype != 0 && subtype != 2 && subtype != 3) throw std::runtime_error("Unsupported XL particle image subtype");
	bool required = false;
	for (const auto &object : level.objects) {
		if (object.render != 11) continue;
		if (object.render_data.size() != 13 || object.render_data[11] < 0 || object.render_data[11] > 3)
			throw std::runtime_error("XL particle image preparation requires a supported authored fire, smoke, spray or bubble payload");
		// Source spray uses the smoke particle image and fade profile
		required |= (object.render_data[11] == 1 ? 0 : object.render_data[11]) == subtype;
		// The non-array bubble shader also binds the active smoke/spark images
		required |= subtype == 0 && object.render_data[11] == 2;
	}
	if (!required) return {};
	if (subtype == 2) return prepare_addon_image(bubble_image_name, "bubble", bubble_image_atlas_side);
	return prepare_addon_image(subtype == 3 ? fire_image_name : smoke_image_name, subtype == 3 ? "fire" : "smoke", subtype == 3 ? fire_image_atlas_side : smoke_image_atlas_side);
}

std::shared_ptr<const ParticleImageResource> prepare_lightning_image(const Inventory &level)
{
	bool required = false;
	for (const auto &object : level.objects)
		if (object.render == 12) required |= lightning_definition(object, 2).glow != 0;
	return required ? prepare_addon_image("corona", "lightning corona", 0) : nullptr;
}
std::shared_ptr<const ParticleImageResource> prepare_powerup_glare_image()
{
	// Source omits optional powerup coronas when glare.Load fails
	return prepare_addon_image("glare", "powerup glare", 0, true);
}
} // namespace xl_compat
