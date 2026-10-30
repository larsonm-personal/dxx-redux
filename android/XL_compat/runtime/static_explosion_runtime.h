#ifndef DXX_XL_STATIC_EXPLOSION_RUNTIME_H
#define DXX_XL_STATIC_EXPLOSION_RUNTIME_H
#ifdef __cplusplus
extern "C" {
#endif
struct object;
int xl_static_explosion_sound(void);
void xl_static_explosions_start(void);
int xl_static_explosion_animate(struct object *object);
int xl_static_explosion_draw(struct object *object);
#ifdef __cplusplus
}
#endif
#endif
