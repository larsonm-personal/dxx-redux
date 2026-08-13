#ifndef GAME_MENU_INTROSPECT_ACCESSORS_H
#define GAME_MENU_INTROSPECT_ACCESSORS_H

#ifdef INTROSPECT_ON
typedef struct game_menu_introspect_snapshot {
	const char *title;
	const char *subtitle;
	int scroll_offset;
	int is_scroll_box;
	int android_wrapped_text;
	int android_original_nitems;
} game_menu_introspect_snapshot;

void game_menu_introspect_read(newmenu *menu, game_menu_introspect_snapshot *snapshot);
const char *game_listbox_introspect_read_title(listbox *lb);
const char *newmenu_get_title(newmenu *menu);
const char *newmenu_get_subtitle(newmenu *menu);
int newmenu_get_scroll_offset(newmenu *menu);
int newmenu_get_is_scroll_box(newmenu *menu);
int newmenu_get_android_wrapped_text(newmenu *menu);
int newmenu_get_android_original_nitems(newmenu *menu);
extern const char *listbox_get_title(listbox *lb);
#endif

#endif
