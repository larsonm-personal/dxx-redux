#ifndef DXX_XL_RUNTIME_PARTICLE_WORLD_H
#define DXX_XL_RUNTIME_PARTICLE_WORLD_H
#include "../particle_world.h"
namespace xl_compat
{
// Snapshot compiled XL classic geometry once per level, independently of Redux AI
ParticleWorld compile_particle_world(const Inventory &source);
} // namespace xl_compat
#endif
