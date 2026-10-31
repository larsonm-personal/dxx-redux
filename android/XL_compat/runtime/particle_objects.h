#ifndef DXX_XL_RUNTIME_PARTICLE_OBJECTS_H
#define DXX_XL_RUNTIME_PARTICLE_OBJECTS_H
#ifdef __cplusplus
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "../fire_emitter.h"
#include "../particle_render.h"
#include <list>
namespace xl_compat
{
struct ParticleSystem {
	int object, signature;
	FireEmitter emitter;
};
void bind_particle_objects(const Inventory &level);
// The same frame consumer accepts a controlled clock in native integration tests
void particle_objects_frame(int milliseconds);
const std::list<ParticleSystem> &particle_systems();
std::vector<ParticleQuad> particle_scene(const std::vector<int> &visible, const ParticlePoint &eye, const ParticleBasis &basis, int milliseconds, unsigned image_width, unsigned image_height);
struct ParticleSceneItem {
	ParticlePoint position;
	ParticleQuad quad;
	float radius = 0;
};
// Retain emitter insertion order for the shared transparency queue
std::vector<ParticleSceneItem> particle_scene_items(const std::vector<int> &visible, const ParticlePoint &eye, const ParticleBasis &basis, int milliseconds, unsigned image_width, unsigned image_height);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
extern "C" {
#endif
void xl_particle_objects_clear(void);
void xl_particle_objects_frame(void);
void xl_particle_render_scene(const short *segments, int count);
int xl_particle_object_set_enabled(int object, int enabled);
int xl_particle_depth_projection(void);
#ifdef __cplusplus
}
#endif
#endif
