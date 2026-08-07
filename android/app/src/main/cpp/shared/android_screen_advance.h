/* Android-only admission policy for advancing transient game screens. */
#ifndef ANDROID_SCREEN_ADVANCE_H
#define ANDROID_SCREEN_ADVANCE_H

#ifdef ANDROID

#include "event.h"

typedef enum android_screen_advance_kind {
	ANDROID_SCREEN_ADVANCE_NONE = 0,
	ANDROID_SCREEN_ADVANCE_DEATH,
	ANDROID_SCREEN_ADVANCE_ENDLEVEL,
	ANDROID_SCREEN_ADVANCE_MOVIE,
	ANDROID_SCREEN_ADVANCE_BRIEFING,
	ANDROID_SCREEN_ADVANCE_LEVELCOMPLETE,
	ANDROID_SCREEN_ADVANCE_POSTLEVEL
} android_screen_advance_kind;

void android_screen_advance_begin(android_screen_advance_kind kind, int ready);
void android_screen_advance_set_ready(android_screen_advance_kind kind, int ready);
void android_screen_advance_end(android_screen_advance_kind kind);

/* True only after the transition guard has expired and held input was released. */
int android_screen_advance_can_activate(android_screen_advance_kind kind);

/* Accept only a fresh primary touch/controller press. Screen actions stay local. */
int android_screen_advance_accept_event(android_screen_advance_kind kind, d_event *event);

/* The UI submits a generation-tagged request; the owning handler consumes it. */
int android_screen_advance_request(unsigned int generation);
int android_screen_advance_take_request(android_screen_advance_kind kind);

int android_screen_advance_get_kind(void);
int android_screen_advance_get_ready(void);
unsigned int android_screen_advance_get_generation(void);

#endif

#endif
