#ifndef ANDROID_NEWMENU_TEXT_WRAP_H
#define ANDROID_NEWMENU_TEXT_WRAP_H

#include "newmenu.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*android_newmenu_measure_text)(const char *text);

int android_newmenu_wrap_text_items(const newmenu_item *source, int source_count,
	int wrap_width, android_newmenu_measure_text measure_text,
	newmenu_item **wrapped, int *wrapped_count);
void android_newmenu_free_text_items(newmenu_item *items, int count);

#ifdef __cplusplus
}
#endif

#endif
