/* Original D1 presentation resources consumed by the shared engine */
#ifndef _D1_IN_D2_PRESENTATION_H
#define _D1_IN_D2_PRESENTATION_H

#include <stddef.h>

int d1_in_d2_read_text(const char *filename, int encoded);
int d1_in_d2_load_text(void);
int d1_in_d2_free_text(void);
int d1_in_d2_show_titles(void);
/* Own the complete native session; inactive returns without side effects
 * A missing/failed D1 resource is handled here and never falls through to D2 */
int d1_in_d2_show_briefing(char *filename, int level_num);
int d1_in_d2_show_ending(char *filename);
int d1_in_d2_show_order_form(void);
const char *d1_in_d2_presentation_prefix(void);
/* Resolve base resources by game while preserving mounted mission/loose overrides */
int d1_in_d2_presentation_resource(const char *name, char *path, size_t size);
enum d1_menu_resource {
	D1_MENU_MAIN, D1_MENU_STARS, D1_MENU_FRAME, D1_MENU_PALETTE
};
/* Stable storage for menu callers; palette names remain logical engine cache keys */
char *d1_in_d2_menu_resource(enum d1_menu_resource kind, const char *default_name);
void d1_in_d2_note_d2_text_loaded(void);
void d1_in_d2_activate_presentation(void);

#endif /* _D1_IN_D2_PRESENTATION_H */
