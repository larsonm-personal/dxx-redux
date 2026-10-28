#ifndef DXX_XL_SPAWN_POINTS_H
#define DXX_XL_SPAWN_POINTS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
// XL's source table has 16 slots; Redux's multiplayer capacity is unchanged
#define XL_SPAWN_SLOTS 16
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
typedef struct xl_spawn_point {
	int32_t segment, side, position[3], orientation[9];
} xl_spawn_point;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
struct trigger;
void xl_spawn_clear(void);
int xl_spawn_action(const struct trigger *action);
int xl_spawn_apply(int player, int object_index);
int xl_spawn_state_count(void);
int xl_spawn_state_get(int index, xl_spawn_point *point);
int xl_spawn_restore(const xl_spawn_point *points, int count, int apply);
int xl_spawn_targets_valid(const xl_spawn_point *points, int count);
#ifdef __cplusplus
}
#endif
#endif
