#ifndef INPUT_DEMO_LEVEL_START_H
#define INPUT_DEMO_LEVEL_START_H

#include <stdint.h>

/* Values entering fresh level initialization, not an arbitrary world snapshot
 * Bounds are shared by the file codec and engine capture/apply code */
#define INPUT_DEMO_LEVEL_PLAYER_FIELDS(X)              \
	X(uint32_t, flags, 0, 32767)                       \
	X(int32_t, energy, 0, INT32_MAX)                   \
	X(int32_t, shields, INT32_MIN, INT32_MAX)          \
	X(uint8_t, lives, 0, 255)                          \
	X(uint8_t, laser_level, 0, 5)                      \
	X(int8_t, starting_level, -127, 127)               \
	X(int8_t, level, -127, 127)                        \
	X(int32_t, time_level, 0, INT32_MAX)               \
	X(uint16_t, primary_weapon_flags, 0, UINT16_MAX)   \
	X(uint16_t, secondary_weapon_flags, 0, UINT16_MAX) \
	X(int8_t, primary_weapon, 0, 9)                    \
	X(int8_t, secondary_weapon, 0, 9)                  \
	X(int32_t, score, INT32_MIN, INT32_MAX)            \
	X(int32_t, last_score, INT32_MIN, INT32_MAX)       \
	X(int32_t, time_total, 0, INT32_MAX)               \
	X(int8_t, hours_total, 0, 127)                     \
	X(int8_t, hours_level, 0, 127)                     \
	X(int16_t, num_kills_total, INT16_MIN, INT16_MAX)  \
	X(int16_t, num_robots_total, INT16_MIN, INT16_MAX) \
	X(uint16_t, hostages_total, 0, UINT16_MAX)         \
	X(uint16_t, hostages_rescued_total, 0, UINT16_MAX) \
	X(int64_t, cloak_time, INT64_MIN, INT64_MAX)       \
	X(int64_t, invulnerable_time, INT64_MIN, INT64_MAX)

#define INPUT_DEMO_LEVEL_RUNTIME_FIELDS(X)                 \
	X(int32_t, cheats_enabled, 0, 1)                       \
	X(int32_t, cheat_wowie, 0, 1)                          \
	X(int32_t, cheat_wowie2, 0, 1)                         \
	X(uint32_t, rng_state, 0, UINT32_MAX)                  \
	X(uint32_t, rng_calls, 0, UINT32_MAX)                  \
	X(int64_t, game_time, INT64_MIN, INT64_MAX)            \
	X(int32_t, afterburner_charge, 0, 65536)               \
	X(int32_t, omega_charge, 0, 65536)                     \
	X(int32_t, spreadfire_toggle, 0, 1)                    \
	X(int32_t, missile_gun, 0, INT32_MAX)                  \
	X(int32_t, proximity_dropped, 0, 3)                    \
	X(int32_t, helix_orientation, 0, 7)                    \
	X(int32_t, smartmines_dropped, 0, 3)                   \
	X(int64_t, last_omega_fire_time, INT64_MIN, INT64_MAX) \
	X(int32_t, laser_firing_count, 0, 127)                 \
	X(int32_t, missile_firing_count, 0, 127)               \
	X(int32_t, primary_picked_up, 0, 10)                   \
	X(int32_t, secondary_picked_up, 0, 10)                 \
	X(int32_t, tick_count, 0, 1000000)                     \
	X(int32_t, tick_step, 0, 1)                            \
	X(int32_t, tick_time, 0, 0x7ffeffff)                   \
	X(int64_t, fusion_sound_time, INT64_MIN, INT64_MAX)    \
	X(int64_t, refuel_sound_time, INT64_MIN, INT64_MAX)    \
	X(int64_t, collision_sound_time, INT64_MIN, INT64_MAX) \
	X(int32_t, difficulty_changed, 0, 1)                   \
	X(int32_t, difficulty_min, 0, 4)                       \
	X(int32_t, difficulty_max, 0, 4)                       \
	X(int32_t, matcen_mode, 0, 2)                          \
	X(uint32_t, homer_frame_count, 0, UINT32_MAX)          \
	X(int32_t, homer_frame_time, 0, INT32_MAX)             \
	X(int32_t, do_homer_frame, 0, 1)                       \
	X(int64_t, next_laser, INT64_MIN, INT64_MAX)           \
	X(int64_t, last_laser, INT64_MIN, INT64_MAX)           \
	X(int64_t, next_missile, INT64_MIN, INT64_MAX)         \
	X(int64_t, next_flare, INT64_MIN, INT64_MAX)

#define INPUT_DEMO_LEVEL_ARRAY_FIELDS(X) \
	X(primary_ammo, 10, 65535)           \
	X(secondary_ammo, 10, 65535)         \
	X(primary_last_super, 10, 1)         \
	X(secondary_last_super, 10, 1)

#pragma pack(push, 8)
typedef struct input_demo_level_start {
#define INPUT_DEMO_DECLARE_FIELD(type, name, low, high) type name;
	INPUT_DEMO_LEVEL_PLAYER_FIELDS(INPUT_DEMO_DECLARE_FIELD)
	INPUT_DEMO_LEVEL_RUNTIME_FIELDS(INPUT_DEMO_DECLARE_FIELD)
#undef INPUT_DEMO_DECLARE_FIELD
#define INPUT_DEMO_DECLARE_ARRAY(name, count, high) uint16_t name[count];
	INPUT_DEMO_LEVEL_ARRAY_FIELDS(INPUT_DEMO_DECLARE_ARRAY)
#undef INPUT_DEMO_DECLARE_ARRAY
} input_demo_level_start;
#pragma pack(pop)

#ifdef __cplusplus
extern "C" {
#endif

/* Called only by genuine fresh-level entry points, never save restoration */
void input_demo_begin_new_level(int level);
void input_demo_end_new_level(int level);
int input_demo_new_level_pending(void);
const input_demo_level_start *input_demo_recording_level_start(void);
void input_demo_set_record_each_level(int enabled);

#ifdef __cplusplus
}
#endif

#endif
