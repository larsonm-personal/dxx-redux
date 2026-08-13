#include "window.h"

#ifdef INTROSPECT_ON
static game_window_introspect_snapshot read_window(window *wind)
{
	game_window_introspect_snapshot snapshot;
	game_window_introspect_read(wind, &snapshot);
	return snapshot;
}

void *window_get_data(window *wind)
{
	return read_window(wind).data;
}

int (*window_get_callback(window *wind))(window *, d_event *, void *)
{
	return read_window(wind).callback;
}
#endif
