/*
 * game_introspect.h — Debug introspection API for AI-assisted testing.
 *
 * Provides a function that serializes the current game state (menus,
 * player, level, position, etc.) into a JSON string that can be read
 * via JNI / adb without resorting to screenshot analysis.
 *
 * This header is safe to include on all platforms; the implementation
 * compiles to a no-op stub when INTROSPECT_ON is not defined.
 */

#ifndef GAME_INTROSPECT_H
#define GAME_INTROSPECT_H

#ifdef __cplusplus
extern "C" {
#endif

#ifdef INTROSPECT_ON

/*
 * Returns a heap-allocated JSON string describing the current game state.
 * The caller must free() the returned pointer.
 */
char *game_introspect_get_state(void);

/*
 * Set the file path where introspection data should be dumped.
 * Must be called once (from JNI) before the game loop starts.
 */
void game_introspect_set_path(const char *path);

/*
 * Request a dump on the next frame.  Thread-safe (sets a volatile flag).
 * Called from JNI on any thread.
 */
void game_introspect_request(void);

/*
 * Called from the game thread (e.g. event_process) each frame.
 * If a dump was requested, writes the current state to the file and
 * clears the request flag.  Costs almost nothing when no dump is pending.
 */
void game_introspect_check_and_dump(void);

#else /* !INTROSPECT_ON */

/* Stubs — compile to nothing on non-introspection builds. */
static inline char *game_introspect_get_state(void) { return (char *)0; }
static inline void game_introspect_set_path(const char *path) { (void)path; }
static inline void game_introspect_request(void) {}
static inline void game_introspect_check_and_dump(void) {}

#endif /* INTROSPECT_ON */

#ifdef __cplusplus
}
#endif

#endif /* GAME_INTROSPECT_H */
