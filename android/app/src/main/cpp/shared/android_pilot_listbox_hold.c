#include "android_pilot_listbox_hold.h"

#ifdef ANDROID

#include <string.h>

#include "key.h"
#include "text.h"
#include "timer.h"

#define ANDROID_PILOT_DELETE_HOLD_TIME ((F1_0 * 3) / 5)
#define ANDROID_LISTBOX_HOLD_NONE      (-1)

typedef struct android_pilot_listbox_hold_state {
	listbox *lb;
	int item;
	fix64 hold_time;
	int button;
	listbox *suppress_release_lb;
	int suppress_release_button;
} android_pilot_listbox_hold_state;

static android_pilot_listbox_hold_state g_android_pilot_listbox_hold = {
	NULL, -1, 0, ANDROID_LISTBOX_HOLD_NONE, NULL, ANDROID_LISTBOX_HOLD_NONE
};

static void android_pilot_listbox_reset_active_hold(void)
{
	g_android_pilot_listbox_hold.lb = NULL;
	g_android_pilot_listbox_hold.item = -1;
	g_android_pilot_listbox_hold.hold_time = 0;
	g_android_pilot_listbox_hold.button = ANDROID_LISTBOX_HOLD_NONE;
}

static void android_pilot_listbox_reset_suppressed_release(void)
{
	g_android_pilot_listbox_hold.suppress_release_lb = NULL;
	g_android_pilot_listbox_hold.suppress_release_button =
	    ANDROID_LISTBOX_HOLD_NONE;
}

static int android_pilot_listbox_is_pilot_select(const char *title)
{
	return title && !strcmp(title, TXT_SELECT_PILOT);
}

static int android_pilot_listbox_hold_can_delete(const char *title, int nitems,
                                                 int item)
{
	return android_pilot_listbox_is_pilot_select(title) && item > 0 &&
	       item < nitems;
}

static int android_pilot_listbox_hold_matches(listbox *lb, const char *title,
                                              int nitems, int item, int button)
{
	return g_android_pilot_listbox_hold.lb == lb &&
	       g_android_pilot_listbox_hold.button == button &&
	       g_android_pilot_listbox_hold.item == item &&
	       android_pilot_listbox_hold_can_delete(title, nitems, item);
}

static int android_pilot_listbox_hold_elapsed(void)
{
	return g_android_pilot_listbox_hold.hold_time &&
	       g_android_pilot_listbox_hold.hold_time +
	               ANDROID_PILOT_DELETE_HOLD_TIME <=
	           timer_query();
}

static int android_pilot_listbox_suppressed_release_matches(listbox *lb,
	                                                        int button)
{
	return g_android_pilot_listbox_hold.suppress_release_lb == lb &&
	       g_android_pilot_listbox_hold.suppress_release_button == button;
}

static int android_pilot_listbox_delete_current(
    listbox *lb,
    int (*listbox_callback)(listbox *lb, d_event *event, void *userdata),
    void *userdata)
{
	struct {
		event_type type;
		int keycode;
	} ke;

	if (!listbox_callback)
		return 0;

	ke.type = EVENT_KEY_COMMAND;
	ke.keycode = KEY_CTRLED + KEY_D;
	return (*listbox_callback)(lb, (d_event *) &ke, userdata);
}

static int android_pilot_listbox_send_enter(
    struct window *wind,
    int (*listbox_key_command)(struct window *wind, d_event *event,
                               listbox *lb),
    listbox *lb)
{
	struct {
		event_type type;
		int keycode;
	} ke;

	ke.type = EVENT_KEY_COMMAND;
	ke.keycode = KEY_ENTER;
	return listbox_key_command(wind, (d_event *) &ke, lb);
}

static int android_pilot_listbox_start_hold(listbox *lb, const char *title,
	                                        int nitems, int item, int button)
{
	if (android_pilot_listbox_suppressed_release_matches(lb, button))
		android_pilot_listbox_reset_suppressed_release();

	if (!android_pilot_listbox_hold_can_delete(title, nitems, item))
		return 0;

	if (android_pilot_listbox_hold_matches(lb, title, nitems, item, button))
		return 1;

	g_android_pilot_listbox_hold.lb = lb;
	g_android_pilot_listbox_hold.item = item;
	g_android_pilot_listbox_hold.hold_time = timer_query();
	g_android_pilot_listbox_hold.button = button;
	return 1;
}

static int android_pilot_listbox_trigger_delete(
	listbox *lb, int button,
	int (*listbox_callback)(listbox *lb, d_event *event, void *userdata),
	void *userdata, int suppress_release)
{
	int rval = android_pilot_listbox_delete_current(lb, listbox_callback, userdata);

	android_pilot_listbox_reset_active_hold();
	if (suppress_release) {
		g_android_pilot_listbox_hold.suppress_release_lb = lb;
		g_android_pilot_listbox_hold.suppress_release_button = button;
	}
	return rval ? rval : 1;
}

void android_pilot_listbox_hold_clear(listbox *lb)
{
	if (lb && g_android_pilot_listbox_hold.lb != lb &&
	    g_android_pilot_listbox_hold.suppress_release_lb != lb)
		return;

	if (!lb || g_android_pilot_listbox_hold.lb == lb)
		android_pilot_listbox_reset_active_hold();
	if (!lb || g_android_pilot_listbox_hold.suppress_release_lb == lb)
		android_pilot_listbox_reset_suppressed_release();
}

int android_pilot_listbox_hold_poll(
	listbox *lb, const char *title, int nitems, int item,
	int (*listbox_callback)(listbox *lb, d_event *event, void *userdata),
	void *userdata)
{
	if (g_android_pilot_listbox_hold.lb != lb)
		return 0;

	if (g_android_pilot_listbox_hold.item != item ||
	    !android_pilot_listbox_hold_can_delete(
	        title, nitems, g_android_pilot_listbox_hold.item)) {
		android_pilot_listbox_reset_active_hold();
		return 0;
	}

	if (!android_pilot_listbox_hold_elapsed())
		return 0;

	return android_pilot_listbox_trigger_delete(
	    lb, g_android_pilot_listbox_hold.button, listbox_callback, userdata, 1);
}

void android_pilot_listbox_mouse_down(listbox *lb, const char *title, int nitems,
                                      int item)
{
	if (!android_pilot_listbox_start_hold(lb, title, nitems, item,
	                                     ANDROID_LISTBOX_HOLD_MOUSE))
		android_pilot_listbox_hold_clear(lb);
}

int android_pilot_listbox_mouse_up(
    listbox *lb, const char *title, int nitems, int item,
    int (*listbox_callback)(listbox *lb, d_event *event, void *userdata),
    void *userdata)
{
	if (android_pilot_listbox_suppressed_release_matches(
	        lb, ANDROID_LISTBOX_HOLD_MOUSE)) {
		android_pilot_listbox_reset_suppressed_release();
		return 1;
	}

	if (!android_pilot_listbox_hold_matches(lb, title, nitems, item,
	                                       ANDROID_LISTBOX_HOLD_MOUSE)) {
		if (g_android_pilot_listbox_hold.lb == lb &&
		    g_android_pilot_listbox_hold.button == ANDROID_LISTBOX_HOLD_MOUSE)
			android_pilot_listbox_reset_active_hold();
		return 0;
	}

	if (android_pilot_listbox_hold_elapsed())
		return android_pilot_listbox_trigger_delete(
		    lb, ANDROID_LISTBOX_HOLD_MOUSE, listbox_callback, userdata, 0);

	android_pilot_listbox_reset_active_hold();
	return 0;
}

int android_pilot_listbox_joy_button_down(
    listbox *lb, const char *title, int nitems, int item, struct window *wind,
    int button,
    int (*listbox_key_command)(struct window *wind, d_event *event,
                               listbox *lb))
{
	if (button != ANDROID_LISTBOX_HOLD_JOY_A)
		return 0;

	(void) wind;
	(void) listbox_key_command;
	return android_pilot_listbox_start_hold(lb, title, nitems, item, button);
}

int android_pilot_listbox_joy_button_up(
    listbox *lb, const char *title, int nitems, int item, struct window *wind,
    int button,
    int (*listbox_key_command)(struct window *wind, d_event *event,
                               listbox *lb),
    int (*listbox_callback)(listbox *lb, d_event *event, void *userdata),
    void *userdata)
{
	int held_item;
	int delete_item;

	if (button != ANDROID_LISTBOX_HOLD_JOY_A)
		return 0;

	if (android_pilot_listbox_suppressed_release_matches(lb, button)) {
		android_pilot_listbox_reset_suppressed_release();
		return 1;
	}

	if (g_android_pilot_listbox_hold.lb != lb ||
	    g_android_pilot_listbox_hold.button != button)
		return 0;

	held_item = g_android_pilot_listbox_hold.item;
	delete_item = held_item == item &&
	              android_pilot_listbox_hold_can_delete(title, nitems, held_item) &&
	              android_pilot_listbox_hold_elapsed();
	android_pilot_listbox_reset_active_hold();
	if (delete_item)
		return android_pilot_listbox_trigger_delete(
		    lb, button, listbox_callback, userdata, 0);
	if (held_item == item)
		return android_pilot_listbox_send_enter(wind, listbox_key_command, lb);
	return 1;
}

#endif