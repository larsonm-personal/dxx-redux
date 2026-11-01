#ifndef DXX_XL_NATIVE_EXPLOSION_MOTION_H
#define DXX_XL_NATIVE_EXPLOSION_MOTION_H
struct object;
#ifdef __cplusplus
extern "C" {
#endif
void xl_explosion_motion_clear(void);
void xl_explosion_motion_begin(struct object *effect);
void xl_explosion_motion_frame(struct object *effect);
#ifdef __cplusplus
}
#endif
#endif
