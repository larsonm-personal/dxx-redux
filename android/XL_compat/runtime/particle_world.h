#ifndef DXX_XL_RUNTIME_PARTICLE_WORLD_H
#define DXX_XL_RUNTIME_PARTICLE_WORLD_H
#include "../particle_world.h"
namespace xl_compat
{
struct ClassicLightingSide;
ParticleSide compile_particle_side(int segment, int side, const ClassicLightingSide &geometry);
// Snapshot compiled XL classic geometry once per level, independently of Redux AI
ParticleWorld compile_particle_world(const Inventory &source);
} // namespace xl_compat
#endif
