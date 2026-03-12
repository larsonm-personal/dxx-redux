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
 * Set the directory where automation output files go.
 * Must be called once (from JNI) before the game loop starts.
 */
void game_automate_set_path(const char *dir_path);

/*
 * Request loading a JSON automation script.
 * Thread-safe: sets a volatile path; actual loading happens on the
 * game thread in game_automate_tick().
 */
void game_automate_load_script(const char *script_path);

/*
 * Called from the game thread (event_process) each frame.
 * Advances the automation script by one tick, injecting keys or
 * checking conditions as needed.  Costs almost nothing when no
 * script is active.
 */
void game_automate_tick(void);

#else /* !INTROSPECT_ON */

/* Stubs -- compile to nothing on non-introspection builds. */
static inline void game_automate_set_path(const char *p) { (void)p; }
static inline void game_automate_load_script(const char *p) { (void)p; }
static inline void game_automate_tick(void) {}

#endif /* INTROSPECT_ON */

#ifdef __cplusplus
}
#endif

#endif /* GAME_AUTOMATE_H */
