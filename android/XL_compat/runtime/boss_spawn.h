#ifndef XL_BOSS_SPAWN_H
#define XL_BOSS_SPAWN_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
struct vms_vector;
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
typedef struct xl_boss_spawn_state {
	int64_t elapsed;
	int64_t cloak_start, cloak_end, teleport, hit, death_elapsed;
	int owner, signature, id, interval;
	int dying, sound;
} xl_boss_spawn_state;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
void xl_boss_spawn_load(void);
void xl_boss_spawn_clear(void);
int xl_boss_spawn_count(void);
int xl_boss_spawn_state_get(int index, xl_boss_spawn_state *state);
int xl_boss_spawn_restore(const xl_boss_spawn_state *states, int count, int apply);
int xl_boss_spawn_robot(int owner, int type, const struct vms_vector *position, int attached);
int xl_boss_spawn_action(int owner, int segment);
void xl_boss_spawn_allow_second(int owner);
int xl_boss_spawn_default_type(int boss_flag, int index);
void xl_boss_weapon_spawn(int owner, int weapon, const struct vms_vector *position);
void xl_boss_ai_frame(int owner, int visible);
void xl_boss_note_hit(int owner);
void xl_boss_teleport(int owner);
void xl_boss_start_death(int owner);
int xl_boss_is_dying(int owner);
void xl_boss_deaths_frame(void);
int xl_boss_cloak_times(int owner, int64_t *start, int64_t *end);
#ifdef __cplusplus
}
#endif
#endif
