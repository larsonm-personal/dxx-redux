#ifndef GAME_WINDOW_INTROSPECT_ACCESSORS_H
#define GAME_WINDOW_INTROSPECT_ACCESSORS_H

#ifdef INTROSPECT_ON
typedef struct game_window_introspect_snapshot {
	void *data;
	int (*callback)(window *, d_event *, void *);
} game_window_introspect_snapshot;

void game_window_introspect_read(window *wind, game_window_introspect_snapshot *snapshot);
/* Introspection accessors (game_introspect.cpp) */
extern void *window_get_data(window *wind);
extern int (*window_get_callback(window *wind))(window *, d_event *, void *);
#endif

#endif
