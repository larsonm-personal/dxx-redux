/*
 * game_automate.h -- Automated input scripting for AI-assisted testing.
 *
 * Runs JSON-based scripts that inject key presses, wait for game states,
 * and trigger introspection dumps -- allowing automated navigation through
 * menus, briefings, and gameplay without manual interaction.
 *
 * This header is safe to include on all platforms; the implementation
 * compiles to a no-op stub when INTROSPECT_ON is not defined.
 */

#ifndef GAME_AUTOMATE_H
#define GAME_AUTOMATE_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef INTROSPECT_ON

/*
 * Source-correlated result captured by Android's synchronous send_axis probe.
 * The snapshot is taken before queued device/sensor events resume, so tests can
 * assert control routing without sampling a later frame's transient Controls.
 */
typedef struct game_automate_axis_probe {
	unsigned int generation;
	int valid;
	int axis;
	int raw_value;
	int touch_source;
	int processed;
	int pitch_time;
	int heading_time;
	int slide_lr_time;
	int slide_ud_time;
	int bank_time;
	int throttle_time;
} game_automate_axis_probe;

/*
 * Set the directory where automation output files go.
 * Must be called once (from JNI) before the game loop starts.
 */
void game_automate_set_path(const char *dir_path);

/*
 * Request loading a JSON automation script. The path, starting step, and run
 * identity are published as one synchronized request and latched by the game
 * thread in game_automate_tick().
 */
void game_automate_load_script(const char *script_path, int start_step, const char *run_id);

/*
 * Called from the game thread (event_process) each frame.
 * Advances the automation script by one tick, injecting keys or
 * checking conditions as needed.  Costs almost nothing when no
 * script is active.
 */
void game_automate_tick(void);

/* Copies the latest synchronous axis probe. Returns nonzero when valid. */
int game_automate_get_axis_probe(game_automate_axis_probe *out_probe);

#else /* !INTROSPECT_ON */

/* Stubs -- compile to nothing on non-introspection builds. */
static inline void game_automate_set_path(const char *p)
{
	(void) p;
}
static inline void game_automate_load_script(const char *p, int step, const char *run_id)
{
	(void) p;
	(void) step;
	(void) run_id;
}
static inline void game_automate_tick(void) {}

#endif /* INTROSPECT_ON */

#ifdef __cplusplus
}
#endif

#endif /* GAME_AUTOMATE_H */
