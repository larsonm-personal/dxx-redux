#ifndef DXX_XL_RUNTIME_PARTICLE_OBJECTS_H
#define DXX_XL_RUNTIME_PARTICLE_OBJECTS_H
#ifdef __cplusplus
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include "../fire_emitter.h"
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
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
extern "C" {
#endif
void xl_particle_objects_clear(void);
void xl_particle_objects_frame(void);
int xl_particle_object_set_enabled(int object, int enabled);
#ifdef __cplusplus
}
#endif
#endif
