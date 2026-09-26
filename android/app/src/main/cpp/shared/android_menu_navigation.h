#ifndef ANDROID_MENU_NAVIGATION_H
#define ANDROID_MENU_NAVIGATION_H

#include "event.h"
#include "joy.h"
#include "key.h"

/* Virtual button IDs match MainActivity.gamepadButtonIndex and DPAD_JOY_BUTTON_BASE */
typedef struct android_menu_key_event {
	event_type type;
	int keycode;
} android_menu_key_event;

static inline int android_menu_translate_button(d_event *event, android_menu_key_event *key)
{
	if (event->type != EVENT_JOYSTICK_BUTTON_DOWN)
		return 0;
	key->type = EVENT_KEY_COMMAND;
	switch (event_joystick_get_button(event)) {
		case 0: key->keycode = KEY_ENTER; break;
		case 1: key->keycode = KEY_ESC; break;
		case 2: key->keycode = KEY_CTRLED + KEY_D; break;
		case 3: key->keycode = KEY_CTRLED + KEY_R; break;
		case 4: key->keycode = KEY_PAGEUP; break;
		case 5: key->keycode = KEY_PAGEDOWN; break;
		case 22: key->keycode = KEY_UP; break;
		case 23: key->keycode = KEY_DOWN; break;
		case 24: key->keycode = KEY_LEFT; break;
		case 25: key->keycode = KEY_RIGHT; break;
		default: return 0;
	}
	return 1;
}

#endif
