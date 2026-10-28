#ifndef DXX_XL_SPEED_BOOST_H
#define DXX_XL_SPEED_BOOST_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
typedef struct xl_speed_boost_state {
	int32_t owner, signature, boosted, speed, steps, phase;
	int32_t velocity[3], minimum[3], maximum[3], source[3], destination[3], direction[3];
} xl_speed_boost_state;
#ifdef _MSC_VER
#pragma pack(pop)
#endif
struct trigger;
void xl_speed_boost_clear(void);
void xl_speed_boost_forget(int object_index);
int xl_speed_boost_active(int object_index);
int xl_speed_boost_player_locked(void);
int xl_speed_boost_action(const struct trigger *action, int index, int object_index, int shot);
void xl_speed_boost_stop(int object_index);
void xl_speed_boost_frame(void);
int xl_speed_boost_begin(int object_index);
int xl_speed_boost_retarget(int object_index, int32_t x, int32_t y, int32_t z);
int xl_speed_boost_drag(int object_index, int32_t time);
int xl_speed_boost_exit_blocked(int object_index, int segment, int side);
int xl_speed_boost_segment(int segment);
int xl_speed_boost_destination_valid(const int32_t point[3]);
int xl_speed_boost_last_speed(void);
int xl_speed_boost_state_count(void);
int xl_speed_boost_state_get(int row, xl_speed_boost_state *state);
int xl_speed_boost_restore(const xl_speed_boost_state *states, int count, int last_speed, int apply);
#ifdef __cplusplus
}
#endif
#endif
