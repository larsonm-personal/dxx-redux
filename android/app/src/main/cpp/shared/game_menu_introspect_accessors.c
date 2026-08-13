#include "newmenu.h"

#ifdef INTROSPECT_ON
static game_menu_introspect_snapshot read_menu(newmenu *menu)
{
	game_menu_introspect_snapshot snapshot;
	game_menu_introspect_read(menu, &snapshot);
	return snapshot;
}

const char *newmenu_get_title(newmenu *menu)
{
	return read_menu(menu).title;
}

const char *newmenu_get_subtitle(newmenu *menu)
{
	return read_menu(menu).subtitle;
}

int newmenu_get_scroll_offset(newmenu *menu)
{
	return read_menu(menu).scroll_offset;
}

int newmenu_get_is_scroll_box(newmenu *menu)
{
	return read_menu(menu).is_scroll_box;
}

int newmenu_get_android_wrapped_text(newmenu *menu)
{
	return read_menu(menu).android_wrapped_text;
}

int newmenu_get_android_original_nitems(newmenu *menu)
{
	return read_menu(menu).android_original_nitems;
}

const char *listbox_get_title(listbox *lb)
{
	return game_listbox_introspect_read_title(lb);
}
#endif
