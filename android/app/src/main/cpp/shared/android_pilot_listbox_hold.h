#ifndef ANDROID_PILOT_LISTBOX_HOLD_H
#define ANDROID_PILOT_LISTBOX_HOLD_H

#ifdef ANDROID

#include "newmenu.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ANDROID_LISTBOX_HOLD_MOUSE 0
#define ANDROID_LISTBOX_HOLD_JOY_A 1

void android_pilot_listbox_hold_clear(listbox *lb);
int android_pilot_listbox_hold_poll(
    listbox *lb, const char *title, int nitems, int item,
    int (*listbox_callback)(listbox *lb, d_event *event, void *userdata),
    void *userdata);
void android_pilot_listbox_mouse_down(listbox *lb, const char *title, int nitems,
                                      int item);
int android_pilot_listbox_mouse_up(
    listbox *lb, const char *title, int nitems, int item,
    int (*listbox_callback)(listbox *lb, d_event *event, void *userdata),
    void *userdata);
int android_pilot_listbox_joy_button_down(
    listbox *lb, const char *title, int nitems, int item, struct window *wind,
    int button,
    int (*listbox_key_command)(struct window *wind, d_event *event,
                               listbox *lb));
int android_pilot_listbox_joy_button_up(
    listbox *lb, const char *title, int nitems, int item, struct window *wind,
    int button,
    int (*listbox_key_command)(struct window *wind, d_event *event,
                               listbox *lb),
    int (*listbox_callback)(listbox *lb, d_event *event, void *userdata),
    void *userdata);

#ifdef __cplusplus
}
#endif

#endif

#endif