#ifndef DXX_XL_RUNTIME_SEGMENT_DAMAGE_H
#define DXX_XL_RUNTIME_SEGMENT_DAMAGE_H
#ifdef __cplusplus
extern "C" {
#endif
struct object;
// False suppresses only centers on a lethal authored tick; inactive selects Redux
int xl_segment_damage_tick(struct object *actor);
#ifdef __cplusplus
}
#endif
#endif
