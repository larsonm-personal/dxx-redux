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
struct ParticleImageResource {
	std::string path;
	PogBitmap bitmap;
};
std::shared_ptr<const ParticleImageResource> prepare_particle_image(const Inventory &level);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
