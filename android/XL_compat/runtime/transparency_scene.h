#ifndef DXX_XL_RUNTIME_TRANSPARENCY_SCENE_H
#define DXX_XL_RUNTIME_TRANSPARENCY_SCENE_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct uvl;
void xl_transparency_begin(const short *segments, int count);
void xl_transparency_clear(void);
int xl_transparency_face(int segment, int side, int count, const int *vertices, int base, int overlay, const struct uvl *uvls, int doorway);
int xl_transparency_object(int object);
int xl_transparency_water(int segment, int side, const int32_t points[4][3], const float colors[4][4]);
void xl_transparency_flush(void);
#ifdef DXX_XL_DESCRIPTOR_TEST
void xl_transparency_test_controls(int active);
#endif
#ifdef __cplusplus
}
namespace xl_compat
{
struct ParticleSceneItem;
class ParticleTexture;
void transparency_particle(const ParticleSceneItem &item, ParticleTexture *texture);
} // namespace xl_compat
#endif
#endif
