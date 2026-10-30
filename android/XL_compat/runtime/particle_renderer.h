#ifndef DXX_XL_RUNTIME_PARTICLE_RENDERER_H
#define DXX_XL_RUNTIME_PARTICLE_RENDERER_H
#include <cstddef>
struct _grs_bitmap;
namespace xl_compat
{
struct ParticleQuad;
// Vertices use the current renderer's camera coordinates and projection
void draw_particle_quads(_grs_bitmap &image, const ParticleQuad *quads, std::size_t count);
} // namespace xl_compat
#endif
