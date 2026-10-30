#ifndef DXX_XL_REACTOR_COUNTDOWN_H
#define DXX_XL_REACTOR_COUNTDOWN_H
#ifdef __cplusplus
extern "C" {
#endif
struct trigger;
typedef struct xl_countdown_state {
	int destroyed, timer, seconds, total, pending, phase, fraction;
} xl_countdown_state;
void xl_countdown_clear(void);
int xl_countdown_valid(const struct trigger *action);
int xl_countdown_action(const struct trigger *action);
void xl_countdown_frame(void);
void xl_countdown_get(xl_countdown_state *state);
int xl_countdown_restore(const xl_countdown_state *state, int apply);
#if defined(INTROSPECT_ON) || defined(DXX_XL_DEVELOPER_PROBE)
unsigned xl_countdown_sound_queries(int sound);
unsigned xl_countdown_death_queries(void);
#endif
#ifdef __cplusplus
}
#endif
#endif
