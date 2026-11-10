#ifndef DXX_XL_RUNTIME_PARTICLE_RESOURCES_H
#define DXX_XL_RUNTIME_PARTICLE_RESOURCES_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "../pog.h"
#include <memory>
#include <string>
namespace xl_compat
{
struct Inventory;
class ParticleTexture;
class ShieldTexture;
struct ShieldImageResource {
	std::string path;
	PogBitmap bitmap;
	std::unique_ptr<ShieldTexture> texture;
	ShieldImageResource();
	~ShieldImageResource();
};
std::shared_ptr<const ShieldImageResource> prepare_shield_image(unsigned index);
struct ParticleImageResource {
	std::string path;
	PogBitmap bitmap;
	std::unique_ptr<ParticleTexture> texture;
	ParticleImageResource();
	~ParticleImageResource();
};
struct AnimatedImageResource {
	std::string path;
	PogBitmap bitmap;
	std::vector<PogBitmap> frames;
	std::vector<std::unique_ptr<ParticleTexture>> textures;
	AnimatedImageResource();
	~AnimatedImageResource();
};
std::shared_ptr<const ParticleImageResource> prepare_particle_image(const Inventory &level, int subtype = 3);
std::shared_ptr<const ParticleImageResource> prepare_lightning_image(const Inventory &level);
std::shared_ptr<const ParticleImageResource> prepare_powerup_glare_image();
std::shared_ptr<const AnimatedImageResource> prepare_time_equipment_image(unsigned index);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
