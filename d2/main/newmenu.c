/*
THE COMPUTER CODE CONTAINED HEREIN IS THE SOLE PROPERTY OF PARALLAX
SOFTWARE CORPORATION ("PARALLAX").  PARALLAX, IN DISTRIBUTING THE CODE TO
END-USERS, AND SUBJECT TO ALL OF THE TERMS AND CONDITIONS HEREIN, GRANTS A
ROYALTY-FREE, PERPETUAL LICENSE TO SUCH END-USERS FOR USE BY SUCH END-USERS
IN USING, DISPLAYING,  AND CREATING DERIVATIVE WORKS THEREOF, SO LONG AS
SUCH USE, DISPLAY OR CREATION IS FOR NON-COMMERCIAL, ROYALTY OR REVENUE
FREE PURPOSES.  IN NO EVENT SHALL THE END-USER USE THE COMPUTER CODE
CONTAINED HEREIN FOR REVENUE-BEARING PURPOSES.  THE END-USER UNDERSTANDS
AND AGREES TO THE TERMS HEREIN AND ACCEPTS THE SAME BY USE OF THIS FILE.
COPYRIGHT 1993-1999 PARALLAX SOFTWARE CORPORATION.  ALL RIGHTS RESERVED.
*/

/*
 *
 * Routines for menus.
 *
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include <ctype.h>

#include "pstypes.h"
#include "dxxerror.h"
#include "gr.h"
#include "grdef.h"
#include "window.h"
#include "songs.h"
#include "key.h"
#include "mouse.h"
#include "palette.h"
#include "game.h"
#include "gamepal.h"
#include "text.h"
#include "menu.h"
#include "newmenu.h"
#include "gamefont.h"
#include "iff.h"
#include "pcx.h"
#include "u_mem.h"
#include "mouse.h"
#include "joy.h"
#include "digi.h"
#include "multi.h"
#include "endlevel.h"
#include "screens.h"
#include "config.h"
#ifdef OGL
#include "ogl_init.h"
#endif
#include "player.h"
#include "state.h"
#include "newdemo.h"
#include "kconfig.h"
#include "strutil.h"
#include "vers_id.h"
#include "timer.h"
#include "playsave.h"
#include "automap.h"
#include "rbaudio.h"
#include "args.h"
#include "gamepal.h"

#ifdef OGL
#include "ogl_init.h"
#endif

#ifdef ANDROID
#include "android_crash_handler.h"
#include "android_log.h"
#include "android_menu_reorder.h"
#include "android_menu_scale.h"
#include "android_pilot_listbox_hold.h"
#endif


#define MAXDISPLAYABLEITEMS 14
#define MAXDISPLAYABLEITEMSTINY 21
#define MESSAGEBOX_TEXT_SIZE 2176  // How many characters in messagebox
#define MAX_TEXT_WIDTH FSPACX(120) // How many pixels wide a input box can be
#define LB_ITEMS_ON_SCREEN 8
struct newmenu
{
	window			*wind;
	int				x,y,w,h;
	short			swidth, sheight; float fntscalex, fntscaley; // with these we check if resolution or fonts have changed so menu structure can be recreated
	char			*title;
	char			*subtitle;
	int				nitems;
	newmenu_item	*items;
	int				(*subfunction)(newmenu *menu, d_event *event, void *userdata);
	int				citem;
	char			*filename;
	int				tiny_mode;
	int			tabs_flag;
	int			reorderitems;
	int				scroll_offset, last_scroll_check, max_displayable;
	int				all_text;		//set true if all text items
	int				is_scroll_box;   // Is this a scrolling box? Set to false at init
	int				max_on_menu;
	int				scroll_line_spacing;
	int				mouse_state, dblclick_flag;
	int				drag_start_y;	// Y coord at touch start for drag-to-scroll, -1 if inactive
	int				drag_happened;	// Set when a drag-scroll actually occurred (suppresses tap activation)
	int				*rval;			// Pointer to return value (for polling newmenus)
	void			*userdata;		// For whatever - like with window system
#ifdef ANDROID
	newmenu_item	*android_original_items;
	int				android_original_nitems;
	int				android_readable_tiny;
	android_menu_reorder_state reorder;
#endif
};
grs_bitmap nm_background, nm_background1;
grs_bitmap *nm_background_sub = NULL;
#ifdef ANDROID
static ubyte nm_background1_palette[768];  // saved palette from the background PCX
enum { ANDROID_TINY_TEXT_MAX_VISIBLE = 9 };

static int android_tap_outside_game_menu(newmenu *menu, int mx, int my)
{
	if (!menu || !menu->subtitle || strcmp(menu->subtitle, "Game Menu") != 0)
		return 0;
	return mx < menu->x - BORDERX || mx > menu->x + menu->w + BORDERX ||
		my < menu->y - BORDERY || my > menu->y + menu->h + BORDERY;
}

static void android_newmenu_trim_copy(char *dst, size_t dst_size,
                                      const char *src, size_t src_len)
{
	while (src_len > 0 && isspace((unsigned char)*src)) {
		src++;
		src_len--;
	}
	while (src_len > 0 && isspace((unsigned char)src[src_len - 1]))
		src_len--;
	if (src_len >= dst_size)
		src_len = dst_size - 1;
	memcpy(dst, src, src_len);
	dst[src_len] = 0;
}

static int android_newmenu_text_width(const char *text)
{
	int w, h, aw;
	gr_get_string_size(text, &w, &h, &aw);
	return w;
}

static int android_newmenu_uses_readable_tiny(newmenu *menu)
{
	return menu && menu->tiny_mode && menu->android_readable_tiny;
}

static int android_newmenu_append_wrapped_line(newmenu_item **items, int *count,
                                               int *capacity, const char *text)
{
	newmenu_item *grown;

	if (*count >= *capacity) {
		*capacity *= 2;
		grown = (newmenu_item *)d_realloc(*items, sizeof(newmenu_item) * *capacity);
		if (!grown)
			return 0;
		*items = grown;
	}

	memset(&(*items)[*count], 0, sizeof(newmenu_item));
	(*items)[*count].type = NM_TYPE_TEXT;
	(*items)[*count].text = d_strdup((char *)text);
	if (!(*items)[*count].text)
		return 0;
	(*count)++;
	return 1;
}

static int android_newmenu_wrap_words(newmenu_item **items, int *count,
                                      int *capacity, const char *first_prefix,
                                      const char *next_prefix, const char *text,
                                      int wrap_width)
{
	const char *p = text;
	const char *prefix = first_prefix;
	char line[NM_MAX_TEXT_LEN + 1];
	char candidate[NM_MAX_TEXT_LEN + 1];
	char word[NM_MAX_TEXT_LEN + 1];

	if (!text || !*text)
		return android_newmenu_append_wrapped_line(items, count, capacity, first_prefix);

	snprintf(line, sizeof(line), "%s", prefix);
	while (*p) {
		size_t len = 0;
		while (*p && isspace((unsigned char)*p))
			p++;
		while (p[len] && !isspace((unsigned char)p[len]) && len < sizeof(word) - 1) {
			word[len] = p[len];
			len++;
		}
		word[len] = 0;
		p += len;
		if (!word[0])
			break;

		snprintf(candidate, sizeof(candidate), "%s%s%s", line,
		         strlen(line) > strlen(prefix) ? " " : "", word);
		if (android_newmenu_text_width(candidate) <= wrap_width ||
		    strlen(line) == strlen(prefix)) {
			snprintf(line, sizeof(line), "%s", candidate);
		} else {
			if (!android_newmenu_append_wrapped_line(items, count, capacity, line))
				return 0;
			prefix = next_prefix;
			snprintf(line, sizeof(line), "%s%s", prefix, word);
		}
	}

	return android_newmenu_append_wrapped_line(items, count, capacity, line);
}

static int android_newmenu_wrap_text_item(newmenu_item **items, int *count,
                                          int *capacity, const char *text,
                                          int wrap_width)
{
	char key[NM_MAX_TEXT_LEN + 1];
	char body[NM_MAX_TEXT_LEN + 1];
	char first_prefix[NM_MAX_TEXT_LEN + 1];
	const char *tab;

	if (!text || !*text)
		return android_newmenu_append_wrapped_line(items, count, capacity, "");

	tab = strchr(text, '\t');
	if (!tab)
		return android_newmenu_wrap_words(items, count, capacity, "", "  ",
		                                  text, wrap_width);

	android_newmenu_trim_copy(key, sizeof(key), text, tab - text);
	android_newmenu_trim_copy(body, sizeof(body), tab + 1, strlen(tab + 1));
	if (!body[0])
		return android_newmenu_append_wrapped_line(items, count, capacity, key);

	snprintf(first_prefix, sizeof(first_prefix), "%s  ", key);
	return android_newmenu_wrap_words(items, count, capacity, first_prefix,
	                                  "    ", body, wrap_width);
}

static void android_newmenu_free_wrapped_items(newmenu *menu)
{
	int i;

	if (!menu || !menu->android_original_items)
		return;

	for (i = 0; i < menu->nitems; i++)
		d_free(menu->items[i].text);
	d_free(menu->items);
	menu->items = menu->android_original_items;
	menu->nitems = menu->android_original_nitems;
	menu->android_original_items = NULL;
	menu->android_original_nitems = 0;
}

static void android_newmenu_expand_tiny_text(newmenu *menu)
{
	int i, count = 0, capacity, wrap_width;
	newmenu_item *wrapped;

	if (!menu || !menu->tiny_mode || menu->nitems <= 0 || menu->filename)
		return;
	if (menu->title && !strcmp(menu->title, "NETGAMES")) {
		menu->android_readable_tiny = 1;
		menu->tabs_flag = 0;
		menu->max_on_menu = ANDROID_TINY_TEXT_MAX_VISIBLE;
	}
	for (i = 0; i < menu->nitems; i++)
		if (menu->items[i].type != NM_TYPE_TEXT)
			return;

	menu->android_readable_tiny = 1;
	gr_set_curfont(MEDIUM1_FONT);
	wrap_width = (SWIDTH * 48) / 100;
	if (wrap_width < FSPACX(95))
		wrap_width = FSPACX(95);
	capacity = menu->nitems * 2 + 8;
	wrapped = (newmenu_item *)d_malloc(sizeof(newmenu_item) * capacity);
	if (!wrapped)
		return;

	for (i = 0; i < menu->nitems; i++)
		if (!android_newmenu_wrap_text_item(&wrapped, &count, &capacity,
		                                    menu->items[i].text, wrap_width)) {
			newmenu temp;
			memset(&temp, 0, sizeof(temp));
			temp.items = wrapped;
			temp.nitems = count;
			temp.android_original_items = menu->items;
			android_newmenu_free_wrapped_items(&temp);
			return;
		}

	menu->android_original_items = menu->items;
	menu->android_original_nitems = menu->nitems;
	menu->items = wrapped;
	menu->nitems = count;
	menu->tabs_flag = 0;
	menu->max_on_menu = ANDROID_TINY_TEXT_MAX_VISIBLE;
	menu->max_displayable = count;
}
#else
static int android_newmenu_uses_readable_tiny(newmenu *menu)
{
	(void)menu;
	return 0;
}
#endif

static grs_font *newmenu_get_body_font(newmenu *menu)
{
	return android_newmenu_uses_readable_tiny(menu) ? MEDIUM1_FONT : (menu && menu->tiny_mode ? GAME_FONT : MEDIUM1_FONT);
}

static grs_font *newmenu_get_scroll_marker_font(newmenu *menu)
{
	return android_newmenu_uses_readable_tiny(menu) ? MEDIUM1_FONT : (menu && menu->tiny_mode ? GAME_FONT : MEDIUM2_FONT);
}

newmenu *newmenu_do4( char * title, char * subtitle, int nitems, newmenu_item * item, int (*subfunction)(newmenu *menu, d_event *event, void *userdata), void *userdata, int citem, char * filename, int TinyMode, int TabsFlag );

void newmenu_free_background()	{
	if (nm_background.bm_data)
	{
		if (nm_background_sub)
		{
			gr_free_sub_bitmap(nm_background_sub);
			nm_background_sub = NULL;
		}
		gr_free_bitmap_data (&nm_background);
	}
	if (nm_background1.bm_data)
		gr_free_bitmap_data (&nm_background1);
}

// Draws the custom menu background pcx, if available
void nm_draw_background1(char * filename)
{
	int pcx_error;
#ifdef ANDROID
	extern int g_ogl_render_context;
	int prev_context = g_ogl_render_context;
	/* newmenu_create_structure() can draw the fullscreen background before
	 * the normal menu window draw wrapper sets menu context. */
	g_ogl_render_context = 0;
#endif

	if (filename != NULL)
	{
		if (nm_background1.bm_data == NULL)
		{
			gr_init_bitmap_data (&nm_background1);
			pcx_error = pcx_read_bitmap( filename, &nm_background1, BM_LINEAR, gr_palette );
			Assert(pcx_error == PCX_ERROR_NONE);
			(void)pcx_error;
#ifdef ANDROID
			// Save the PCX palette so we can restore it on cached draws.
			// On Android (SDL software rendering), gr_palette may be
			// overwritten by load_palette() between frames due to the
			// activity lifecycle, causing wrong colors on the cached bitmap.
			memcpy(nm_background1_palette, gr_palette, sizeof(nm_background1_palette));
#endif
		}
#ifdef ANDROID
		else
		{
			memcpy(gr_palette, nm_background1_palette, sizeof(nm_background1_palette));
		}
#endif
		/* Startup menus can touch this cached bitmap before the final menu
		 * palette is active. Force the OGL texture to re-upload so the visible
		 * draw uses the current palette instead of the first cached upload. */
#ifdef OGL
		ogl_freebmtexture(&nm_background1);
#endif
		gr_palette_load( gr_palette );
		show_fullscr(&nm_background1);
	}

	strcpy(last_palette_loaded,"");		//force palette load next time
#ifdef ANDROID
	g_ogl_render_context = prev_context;
#endif
}

#define MENU_BACKGROUND_BITMAP_HIRES (PHYSFSX_exists("scoresb.pcx",1)?"scoresb.pcx":"scores.pcx")
#define MENU_BACKGROUND_BITMAP_LORES (PHYSFSX_exists("scores.pcx",1)?"scores.pcx":"scoresb.pcx")
#define MENU_BACKGROUND_BITMAP (HIRESMODE?MENU_BACKGROUND_BITMAP_HIRES:MENU_BACKGROUND_BITMAP_LORES)

// Draws the frame background for menus
void nm_draw_background(int x1, int y1, int x2, int y2 )
{
	int w,h,init_sub=0;
	int canvas_w, canvas_h;
	static float BGScaleX=1,BGScaleY=1;
	grs_canvas *tmp,*old;
	#ifdef ANDROID
	/* Fullscreen startup menus use their own PCX palette. Switch framed
	 * startup menus back to the standard menu palette before remapping the
	 * cached scores background. */
	if (Game_wind == NULL)
		load_palette(MENU_PALETTE,0,1);
	#endif

	if (nm_background.bm_data == NULL)
	{
		int pcx_error;
		ubyte background_palette[768];
		gr_init_bitmap_data (&nm_background);
		pcx_error = pcx_read_bitmap(MENU_BACKGROUND_BITMAP,&nm_background,BM_LINEAR,background_palette);
		Assert(pcx_error == PCX_ERROR_NONE);
		(void)pcx_error;
		gr_palette_load( gr_palette );
		gr_remap_bitmap_good( &nm_background, background_palette, -1, -1 );
		BGScaleX=((float)SWIDTH/nm_background.bm_w);
		BGScaleY=((float)SHEIGHT/nm_background.bm_h);
		init_sub=1;
	}

	canvas_w = grd_curcanv ? grd_curcanv->cv_bitmap.bm_w : SWIDTH;
	canvas_h = grd_curcanv ? grd_curcanv->cv_bitmap.bm_h : SHEIGHT;
	if ( x1 < 0 ) x1 = 0;
	if ( y1 < 0 ) y1 = 0;
	if ( x2 > canvas_w - 1) x2 = canvas_w - 1;
	if ( y2 > canvas_h - 1) y2 = canvas_h - 1;

	w = x2-x1;
	h = y2-y1;
	if (w <= 0 || h <= 0)
		return;

	if (w > canvas_w) w = canvas_w;
	if (h > canvas_h) h = canvas_h;

	old=grd_curcanv;
	tmp=gr_create_sub_canvas(old,x1,y1,w,h);
	gr_set_current_canvas(tmp);
	gr_palette_load( gr_palette );

	show_fullscr( &nm_background ); // show so we load all necessary data for the sub-bitmap
	if (!init_sub && ((nm_background_sub->bm_w != w*(((float) nm_background.bm_w)/SWIDTH)) || (nm_background_sub->bm_h != h*(((float) nm_background.bm_h)/SHEIGHT))))
	{
		init_sub=1;
		gr_free_sub_bitmap(nm_background_sub);
		nm_background_sub = NULL;
	}
	if (init_sub)
		nm_background_sub = gr_create_sub_bitmap(&nm_background,0,0,w*(((float) nm_background.bm_w)/SWIDTH),h*(((float) nm_background.bm_h)/SHEIGHT));
	show_fullscr( nm_background_sub );

	gr_set_current_canvas(old);
	gr_free_sub_canvas(tmp);

	gr_settransblend(14, GR_BLEND_NORMAL);
	gr_setcolor( BM_XRGB(1,1,1) );
	for (w=5*BGScaleX;w>0;w--)
		gr_urect( x2-w, y1+w*(BGScaleY/BGScaleX), x2-w, y2-w*(BGScaleY/BGScaleX) );//right edge
	gr_setcolor( BM_XRGB(0,0,0) );
	for (h=5*BGScaleY;h>0;h--)
		gr_urect( x1+h*(BGScaleX/BGScaleY), y2-h, x2-h*(BGScaleX/BGScaleY), y2-h );//bottom edge
	gr_settransblend(GR_FADE_OFF, GR_BLEND_NORMAL);
}

// Draw a left justfied string
void nm_string( int w1,int x, int y, char * s, int tabs_flag)
{
	int w,h,aw,tx=0,t=0,i;
	char *p,*s1,*s2,measure[2];
	int XTabs[]={18,90,127,165,231,256};

	p=s1=NULL;
	s2 = d_strdup(s);

	for (i=0;i<6;i++) {
		XTabs[i]=FSPACX(XTabs[i]);
		XTabs[i]+=x;
	}

	measure[1]=0;

	if (!tabs_flag) {
		p = strchr( s2, '\t' );
		if (p && (w1>0) ) {
			*p = '\0';
			s1 = p+1;
		}
	}

	gr_get_string_size(s2, &w, &h, &aw  );

	if (w1 > 0)
		w = w1;

	if (tabs_flag) {
		for (i=0;s2[i];i++) {
			if (s2[i]=='\t' && tabs_flag) {
				x=XTabs[t];
				t++;
				continue;
			}
			measure[0]=s2[i];
			gr_get_string_size(measure,&tx,&h,&aw);
			gr_string(x,y,measure);
			x+=tx;
		}
	}
	else
		gr_string (x,y,s2);

	if (!tabs_flag && p && (w1>0) ) {
		gr_get_string_size(s1, &w, &h, &aw  );

		gr_string( x+w1-w, y, s1 );

		*p = '\t';
	}
	d_free(s2);
}

// Draw a slider and it's string
void nm_string_slider( int w1,int x, int y, char * s )
{
	int w,h,aw;
	char *p,*s1;

	s1=NULL;

	p = strchr( s, '\t' );
	if (p)	{
		*p = '\0';
		s1 = p+1;
	}

	gr_get_string_size(s, &w, &h, &aw  );
	gr_string( x, y, s );

	if (p)	{
		gr_get_string_size(s1, &w, &h, &aw  );
		gr_string( x+w1-w, y, s1 );

		*p = '\t';
	}
}


// Draw a left justfied string with black background.
void nm_string_black( int w1,int x, int y, char * s )
{
	int w,h,aw;
	gr_get_string_size(s, &w, &h, &aw  );

	if (w1 == 0) w1 = w;

	gr_setcolor( BM_XRGB(5,5,5));
	gr_rect( x - FSPACX(2), y-FSPACY(1), x+w1, y + h);
	gr_setcolor( BM_XRGB(2,2,2));
	gr_rect( x - FSPACX(2), y - FSPACY(1), x, y + h );
	gr_setcolor( BM_XRGB(0,0,0));
	gr_rect( x - FSPACX(1), y - FSPACY(1), x+w1 - FSPACX(1), y + h);

	gr_string( x, y, s );
}


// Draw a right justfied string
void nm_rstring( int w1,int x, int y, char * s )
{
	int w,h,aw;
	gr_get_string_size(s, &w, &h, &aw  );
	x -= FSPACX(3);

	if (w1 == 0) w1 = w;
	gr_string( x-w, y, s );
}

void nm_string_inputbox( int w, int x, int y, char * text, int current )
{
	int w1,h1,aw;

	// even with variable char widths and a box that goes over the whole screen, we maybe never get more than 75 chars on the line
	if (strlen(text)>75)
		text+=strlen(text)-75;
	while( *text )	{
		gr_get_string_size(text, &w1, &h1, &aw  );
		if ( w1 > w-FSPACX(10) )
			text++;
		else
			break;
	}
	if ( *text == 0 )
		w1 = 0;

	nm_string_black( w, x, y, text );

	if ( current && timer_query() & 0x8000 )
		gr_string( x+w1, y, CURSOR_STRING );
}

static int newmenu_get_scroll_line_spacing(newmenu *menu)
{
	if (menu && menu->scroll_line_spacing > 0) 
		return menu->scroll_line_spacing;
	return (int)LINE_SPACING;
}
void draw_item( newmenu_item *item, int is_current, int is_grabbed, int tiny, int tabs_flag, int scroll_offset, int scroll_line_spacing )
{
	int visible_y = item->y - (scroll_line_spacing * scroll_offset);
	if (tiny)
	{
		if (is_current)
			gr_set_fontcolor(gr_find_closest_color_current(57,49,20),-1);
		else
			gr_set_fontcolor(gr_find_closest_color_current(29,29,47),-1);
		if (item->text[0]=='\t')
			gr_set_fontcolor (gr_find_closest_color_current(63,63,63),-1);
		if (is_grabbed)
			gr_set_fontcolor(gr_find_closest_color_current(63,57,20),-1);
	}
	else
	{
		gr_set_curfont((is_current || is_grabbed)?MEDIUM2_FONT:MEDIUM1_FONT);
		if (is_grabbed)
			gr_set_fontcolor(BM_XRGB(31,28,10), -1);
		else
			gr_set_fontcolor(BM_XRGB(21,21,21), -1);
	}
	if (is_grabbed)
		gr_string(item->x - FSPACX(10), visible_y, ">");
	switch( item->type )	{
		case NM_TYPE_TEXT:
		case NM_TYPE_MENU:
			nm_string( item->w, item->x, visible_y, item->text, tabs_flag );
			break;
		case NM_TYPE_SLIDER:
		{
			int i,j;
			if (item->value < item->min_value) item->value=item->min_value;
			if (item->value > item->max_value) item->value=item->max_value;
			i = sprintf( item->saved_text, "%s\t%s", item->text, SLIDER_LEFT );
			for (j=0; j<(item->max_value-item->min_value+1); j++ )	{
				i += sprintf( item->saved_text + i, "%s", SLIDER_MIDDLE );
			}
			sprintf( item->saved_text + i, "%s", SLIDER_RIGHT );
			item->saved_text[item->value+1+strlen(item->text)+1] = SLIDER_MARKER[0];
			nm_string_slider( item->w, item->x, visible_y, item->saved_text );
		}
			break;
		case NM_TYPE_INPUT_MENU:
			if ( item->group==0 )
			{
				nm_string( item->w, item->x, visible_y, item->text, tabs_flag );
			} else {
				nm_string_inputbox( item->w, item->x, visible_y, item->text, is_current );
			}
			break;
		case NM_TYPE_INPUT:
			nm_string_inputbox( item->w, item->x, visible_y, item->text, is_current );
			break;
		case NM_TYPE_CHECK:
			nm_string( item->w, item->x, visible_y, item->text, tabs_flag );
			if (item->value)
				nm_rstring( item->right_offset,item->x, visible_y, CHECKED_CHECK_BOX );
			else
				nm_rstring( item->right_offset,item->x, visible_y, NORMAL_CHECK_BOX );
			break;
		case NM_TYPE_RADIO:
			nm_string( item->w, item->x, visible_y, item->text, tabs_flag );
			if (item->value)
				nm_rstring( item->right_offset, item->x, visible_y, CHECKED_RADIO_BOX );
			else
				nm_rstring( item->right_offset, item->x, visible_y, NORMAL_RADIO_BOX );
			break;
		case NM_TYPE_NUMBER:
		{
			char text[10];
			if (item->value < item->min_value) item->value=item->min_value;
			if (item->value > item->max_value) item->value=item->max_value;
			nm_string( item->w, item->x, visible_y, item->text, tabs_flag );
			sprintf( text, "%d", item->value );
			nm_rstring( item->right_offset,item->x, visible_y, text );
		}
			break;
	}
}

const char *Newmenu_allowed_chars=NULL;

//returns true if char is allowed
int char_allowed(char c)
{
	const char *p = Newmenu_allowed_chars;

	if (!p)
		return 1;

	while (*p) {
		Assert(p[1]);

		if (c>=p[0] && c<=p[1])
			return 1;

		p += 2;
	}

	return 0;
}

void strip_end_whitespace( char * text )
{
	int i,l;
	l = strlen( text );
	for (i=l-1; i>=0; i-- )	{
		if ( isspace(text[i]) )
			text[i] = 0;
		else
			return;
	}
}

int newmenu_do( char * title, char * subtitle, int nitems, newmenu_item * item, int (*subfunction)(newmenu *menu, d_event *event, void *userdata), void *userdata )
{
	return newmenu_do2( title, subtitle, nitems, item, subfunction, userdata, 0, NULL );
}

newmenu *newmenu_dotiny( char * title, char * subtitle, int nitems, newmenu_item * item, int TabsFlag, int (*subfunction)(newmenu *menu, d_event *event, void *userdata), void *userdata )
{
        return newmenu_do4( title, subtitle, nitems, item, subfunction, userdata, 0, NULL, 1, TabsFlag );
}


int newmenu_do1( char * title, char * subtitle, int nitems, newmenu_item * item, int (*subfunction)(newmenu *menu, d_event *event, void *userdata), void *userdata, int citem )
{
	return newmenu_do2( title, subtitle, nitems, item, subfunction, userdata, citem, NULL );
}


int newmenu_do2( char * title, char * subtitle, int nitems, newmenu_item * item, int (*subfunction)(newmenu *menu, d_event *event, void *userdata), void *userdata, int citem, char * filename )
{
	newmenu *menu;
	window *wind;
	int rval = -1;

	menu = newmenu_do3( title, subtitle, nitems, item, subfunction, userdata, citem, filename );

	if (!menu)
		return -1;
	menu->rval = &rval;
	wind = menu->wind;	// avoid dereferencing a freed 'menu'

	// newmenu_do2 and simpler get their own event loop
	// This is so the caller doesn't have to provide a callback that responds to EVENT_NEWMENU_SELECTED
	while (window_exists(wind))
		event_process();

	return rval;
}

// Basically the same as do2 but sets reorderitems flag for weapon priority menu a bit redundant to get lose of a global variable but oh well...
int newmenu_doreorder( char * title, char * subtitle, int nitems, newmenu_item * item, int (*subfunction)(newmenu *menu, d_event *event, void *userdata), void *userdata )
{
	newmenu *menu;
	window *wind;
	int rval = -1;

	menu = newmenu_do3( title, subtitle, nitems, item, subfunction, userdata, 0, NULL );

	if (!menu)
		return -1;
	menu->reorderitems = 1;
	menu->rval = &rval;
	wind = menu->wind;	// avoid dereferencing a freed 'menu'

	// newmenu_do2 and simpler get their own event loop
	// This is so the caller doesn't have to provide a callback that responds to EVENT_NEWMENU_SELECTED
	while (window_exists(wind))
		event_process();

	return rval;
}

newmenu *newmenu_do3( char * title, char * subtitle, int nitems, newmenu_item * item, int (*subfunction)(newmenu *menu, d_event *event, void *userdata), void *userdata, int citem, char * filename )
{
	return newmenu_do4( title, subtitle, nitems, item, subfunction, userdata, citem, filename, 0, 0 );
}

newmenu *newmenu_do_fixedfont( char * title, char * subtitle, int nitems, newmenu_item * item, int (*subfunction)(newmenu *menu, d_event *event, void *userdata), void *userdata, int citem, char * filename){
	return newmenu_do4( title, subtitle, nitems, item, subfunction, userdata, citem, filename, 0, 0);
}


#ifdef NEWMENU_MOUSE
ubyte Hack_DblClick_MenuMode=0;
#endif

newmenu_item *newmenu_get_items(newmenu *menu)
{
	return menu->items;
}

int newmenu_get_nitems(newmenu *menu)
{
	return menu->nitems;
}

int newmenu_get_citem(newmenu *menu)
{
	return menu->citem;
}

window *newmenu_get_window(newmenu *menu)
{
	return menu->wind;
}

void newmenu_set_rval(newmenu *menu, int rval)
{
	*menu->rval = rval;
}

#ifdef INTROSPECT_ON
const char *newmenu_get_title(newmenu *menu)
{
	return menu->title;
}

const char *newmenu_get_subtitle(newmenu *menu)
{
	return menu->subtitle;
}

int newmenu_get_scroll_offset(newmenu *menu)
{
	return menu->scroll_offset;
}

int newmenu_get_is_scroll_box(newmenu *menu)
{
	return menu->is_scroll_box;
}

int newmenu_get_android_wrapped_text(newmenu *menu)
{
#ifdef ANDROID
	return menu->android_readable_tiny;
#else
	return 0;
#endif
}

int newmenu_get_android_original_nitems(newmenu *menu)
{
#ifdef ANDROID
	return menu->android_original_items ? menu->android_original_nitems : menu->nitems;
#else
	return menu->nitems;
#endif
}
#endif

void newmenu_scroll(newmenu *menu, int amount)
{
	int i = 0, first = 0, last = 0;

	if (amount == 0) // nothing to do for us
		return;

	if (menu->all_text)
	{
		menu->scroll_offset += amount;
		if (menu->scroll_offset < 0)
			menu->scroll_offset = 0;
		if (menu->max_on_menu+menu->scroll_offset > menu->nitems)
			menu->scroll_offset = menu->nitems-menu->max_on_menu;
		return;
	}

	for (i = 0; i < menu->nitems; i++) // find first "usable" item
	{
		if (menu->items[i].type != NM_TYPE_TEXT)
		{
			first = i;
			break;
		}
	}
	for (i = menu->nitems-1; i >= first; i--) // find last "usable" item
	{
		if (menu->items[i].type != NM_TYPE_TEXT)
		{
			last = i;
			break;
		}
	}

	if (first == last) // nothing to do for us
		return;

	if (menu->citem == last && amount == 1) // if citem == last item and we want to go down one step, go to first item
	{
		newmenu_scroll(menu, -menu->nitems);
		return;
	}
	if (menu->citem == first && amount == -1) // if citem == first item and we want to go up one step, go to last item
	{
		newmenu_scroll(menu, menu->nitems);
		return;
	}

	i = 0;
	if (amount > 0) // down the list
	{
		do // count down until we reached a non NM_TYPE_TEXT item and reached our amount
		{
			if (menu->citem == last) // stop if we reached the last item
				return;
			i++;
			menu->citem++;
			if (menu->is_scroll_box) // update scroll_offset as we go down the menu
			{
				menu->last_scroll_check=-1;
				if (menu->citem+4>=menu->max_on_menu+menu->scroll_offset && menu->scroll_offset < menu->nitems-menu->max_on_menu)
					menu->scroll_offset++;
			}
		} while (menu->items[menu->citem].type == NM_TYPE_TEXT || i < amount);
	}
	else if (amount < 0) // up the list
	{
		do // count up until we reached a non NM_TYPE_TEXT item and reached our amount
		{
			if (menu->citem == first)  // stop if we reached the first item
				return;
			i--;
			menu->citem--;
			if (menu->is_scroll_box) // update scroll_offset as we go up the menu
			{
				menu->last_scroll_check=-1;
				if (menu->citem-4<menu->scroll_offset && menu->scroll_offset > 0)
					menu->scroll_offset--;
			}
		} while (menu->items[menu->citem].type == NM_TYPE_TEXT || i > amount);
	}
}

static void newmenu_get_item_bounds(newmenu *menu, int item_index,
	                                int *x1, int *y1, int *x2, int *y2)
{
	newmenu_item *item = &menu->items[item_index];
	int row_height = item->h;
	int scroll_line_spacing = newmenu_get_scroll_line_spacing(menu);
	int visible_y = item->y - (scroll_line_spacing * menu->scroll_offset);

	if (row_height < scroll_line_spacing)
		row_height = scroll_line_spacing;

	*x1 = grd_curcanv->cv_bitmap.bm_x + item->x - FSPACX(13);
	*x2 = *x1 + item->w + FSPACX(13);
	*y1 = grd_curcanv->cv_bitmap.bm_y + visible_y;
	*y2 = *y1 + row_height;
}

static void newmenu_reorder_ensure_visible(newmenu *menu)
{
	if (!menu->is_scroll_box)
		return;
	if (menu->citem < menu->scroll_offset)
		menu->scroll_offset = menu->citem;
	else if (menu->citem >= menu->scroll_offset + menu->max_displayable)
		menu->scroll_offset = menu->citem - menu->max_displayable + 1;
	if (menu->scroll_offset < 0)
		menu->scroll_offset = 0;
	if (menu->scroll_offset > menu->nitems - menu->max_on_menu)
		menu->scroll_offset = menu->nitems - menu->max_on_menu;
	menu->last_scroll_check = -1;
}

static int newmenu_reorder_move(newmenu *menu, int direction)
{
	int target;
	char *Temp;
	int TempVal;

	if (!menu->reorderitems || !menu->items || menu->citem < 0)
		return 0;
	target = menu->citem + direction;
	if (target < 0 || target >= menu->nitems)
		return 0;
	Temp = menu->items[menu->citem].text;
	TempVal = menu->items[menu->citem].value;
	menu->items[menu->citem].text = menu->items[target].text;
	menu->items[menu->citem].value = menu->items[target].value;
	menu->items[target].text = Temp;
	menu->items[target].value = TempVal;
	menu->citem = target;
	newmenu_reorder_ensure_visible(menu);
	return 1;
}

#ifdef ANDROID
static int newmenu_reorder_item_at_pos(newmenu *menu, int mx, int my)
{
	int i, x1, x2, y1, y2;
	int last = menu->scroll_offset + menu->max_on_menu;

	if (last > menu->nitems)
		last = menu->nitems;
	for (i = menu->scroll_offset; i < last; i++) {
		newmenu_get_item_bounds(menu, i, &x1, &y1, &x2, &y2);
		if ((mx > x1) && (mx < x2) && (my > y1) && (my < y2))
			return i;
	}
	return -1;
}

static void newmenu_reorder_drop(newmenu *menu)
{
	android_menu_reorder_drop(&menu->reorder);
}

static int newmenu_reorder_grab(newmenu *menu)
{
	if (!menu->reorderitems || menu->citem < 0 || menu->citem >= menu->nitems)
		return 0;
	android_menu_reorder_mark_grabbed(&menu->reorder, menu->citem);
	newmenu_reorder_ensure_visible(menu);
	return 1;
}

static int newmenu_reorder_poll(newmenu *menu)
{
	fix64 now;
	if (!menu->reorderitems || menu->reorder.grabbed)
		return 0;
	now = timer_query();
	if (android_menu_reorder_button_ready(&menu->reorder, now))
		return newmenu_reorder_grab(menu);
	if (android_menu_reorder_touch_ready(&menu->reorder, menu->mouse_state, now)) {
		menu->citem = menu->reorder.touch_candidate;
		return newmenu_reorder_grab(menu);
	}
	return 0;
}

static const char *android_mouse_event_phase(d_event *event)
{
	if (!event)
		return "unknown";
	if (event->type == EVENT_MOUSE_BUTTON_DOWN)
		return "down";
	if (event->type == EVENT_MOUSE_BUTTON_UP)
		return "up";
	return "other";
}

static int android_newmenu_item_at_point(newmenu *menu, int mx, int my,
                                         int *rx1, int *ry1, int *rx2,
                                         int *ry2)
{
	int i, x1, y1, x2, y2;

	for (i = menu->scroll_offset; i < menu->max_on_menu + menu->scroll_offset; i++) {
		if (i >= menu->nitems)
			break;
		newmenu_get_item_bounds(menu, i, &x1, &y1, &x2, &y2);
		if ((mx > x1) && (mx < x2) && (my > y1) && (my < y2)) {
			if (rx1) *rx1 = x1;
			if (ry1) *ry1 = y1;
			if (rx2) *rx2 = x2;
			if (ry2) *ry2 = y2;
			return i;
		}
	}
	return -1;
}

static void android_log_newmenu_touch(newmenu *menu, d_event *event,
                                      int mx, int my)
{
	static int diag_count;
	android_menu_scale_result scale;
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	int hit, type = -1;
	const char *text = "";

	if (!menu || diag_count >= 160)
		return;
	diag_count++;
	hit = android_newmenu_item_at_point(menu, mx, my, &x1, &y1, &x2, &y2);
	if (hit >= 0 && hit < menu->nitems) {
		type = menu->items[hit].type;
		text = menu->items[hit].text ? menu->items[hit].text : "";
	}
	android_menu_scale_get_state(&scale);
	debug_log(DLOG_GAME,
	          "[newmenu-touch] phase=%s mx=%d my=%d hit=%d citem=%d n=%d scroll=%d max=%d bounds=(%d,%d %dx%d) type=%d title='%s' subtitle='%s' item='%s' scale=%d src=(%d,%d %dx%d) dst=(%d,%d %dx%d)\n",
	          android_mouse_event_phase(event), mx, my, hit, menu->citem,
	          menu->nitems, menu->scroll_offset, menu->max_on_menu, x1, y1,
	          x2 - x1, y2 - y1, type, menu->title ? menu->title : "",
	          menu->subtitle ? menu->subtitle : "", text, scale.active,
	          scale.src.x, scale.src.y, scale.src.w, scale.src.h,
	          scale.dst.x, scale.dst.y, scale.dst.w, scale.dst.h);
}
#endif

int newmenu_mouse(window *wind, d_event *event, newmenu *menu, int button)
{
	int old_choice, i, mx=0, my=0, mz=0, x1 = 0, x2, y1, y2, changed = 0;
	grs_canvas *menu_canvas = window_get_canvas(wind), *save_canvas = grd_curcanv;

	switch (button)
	{
		case MBTN_LEFT:
		{
			gr_set_current_canvas(menu_canvas);

			old_choice = menu->citem;

#ifdef ANDROID
			if (event->type == EVENT_MOUSE_BUTTON_DOWN ||
			    event->type == EVENT_MOUSE_BUTTON_UP) {
				mouse_get_pos(&mx, &my, &mz);
				android_log_newmenu_touch(menu, event, mx, my);
			}
#endif

			if ((event->type == EVENT_MOUSE_BUTTON_DOWN) && !menu->all_text)
			{
				mouse_get_pos(&mx, &my, &mz);
#ifdef ANDROID
				if (menu->reorderitems) {
					i = newmenu_reorder_item_at_pos(menu, mx, my);
					if (i >= 0) {
						menu->citem = i;
						android_menu_reorder_start_touch(&menu->reorder, i, timer_query());
						menu->drag_happened = 0;
						gr_set_current_canvas(save_canvas);
						return 1;
					}
				}
#endif
				for (i=menu->scroll_offset; i<menu->max_on_menu+menu->scroll_offset; i++ )	{
					newmenu_get_item_bounds(menu, i, &x1, &y1, &x2, &y2);
					if (((mx > x1) && (mx < x2)) && ((my > y1) && (my < y2))) {
						if (i != menu->citem) {
							if(Hack_DblClick_MenuMode) menu->dblclick_flag = 0;
						}

						menu->citem = i;

						switch( menu->items[menu->citem].type )	{
							case NM_TYPE_CHECK:
#ifdef ANDROID
								if (menu->is_scroll_box) break; // defer toggle to BUTTON_UP
#endif
								if ( menu->items[menu->citem].value )
									menu->items[menu->citem].value = 0;
								else
									menu->items[menu->citem].value = 1;

								if (menu->is_scroll_box)
									menu->last_scroll_check=-1;
								changed = 1;
								break;
							case NM_TYPE_RADIO:
#ifdef ANDROID
								if (menu->is_scroll_box) break; // defer toggle to BUTTON_UP
#endif
								for (i=0; i<menu->nitems; i++ )	{
									if ((i!=menu->citem) && (menu->items[i].type==NM_TYPE_RADIO) && (menu->items[i].group==menu->items[menu->citem].group) && (menu->items[i].value) )	{
										menu->items[i].value = 0;
										changed = 1;
									}
								}
								menu->items[menu->citem].value = 1;
								break;
							case NM_TYPE_TEXT:
								menu->citem=old_choice;
								menu->mouse_state=0;
								break;
						}
						break;
					}
				}
			}

			if ( menu->mouse_state ) {
				mouse_get_pos(&mx, &my, &mz);

				// check possible scrollbar stuff first
				if (menu->is_scroll_box) {
					int arrow_width, arrow_height, aw, ScrollAllow=0;
					static fix64 ScrollTime=0;
					if (ScrollTime + F1_0/5 < timer_query())
					{
						ScrollTime = timer_query();
						ScrollAllow = 1;
					}

					if (menu->scroll_offset != 0) {
						gr_set_curfont(newmenu_get_scroll_marker_font(menu));
						gr_get_string_size(UP_ARROW_MARKER, &arrow_width, &arrow_height, &aw);
						x1 = grd_curcanv->cv_bitmap.bm_x+BORDERX-FSPACX(12);
						y1 = grd_curcanv->cv_bitmap.bm_y + menu->items[menu->scroll_offset].y-(newmenu_get_scroll_line_spacing(menu)*menu->scroll_offset);
						x2 = x1 + arrow_width;
						y2 = y1 + arrow_height;
						if (((mx > x1) && (mx < x2)) && ((my > y1) && (my < y2)) && ScrollAllow) {
							newmenu_scroll(menu, -1);
						}
					}
					if (menu->scroll_offset+menu->max_displayable<menu->nitems) {
						gr_set_curfont(newmenu_get_scroll_marker_font(menu));
						gr_get_string_size(DOWN_ARROW_MARKER, &arrow_width, &arrow_height, &aw);
						x1 = grd_curcanv->cv_bitmap.bm_x+BORDERX-FSPACX(12);
						y1 = grd_curcanv->cv_bitmap.bm_y + menu->items[menu->scroll_offset+menu->max_displayable-1].y-(newmenu_get_scroll_line_spacing(menu)*menu->scroll_offset);
						x2 = x1 + arrow_width;
						y2 = y1 + arrow_height;
						if (((mx > x1) && (mx < x2)) && ((my > y1) && (my < y2)) && ScrollAllow) {
							newmenu_scroll(menu, 1);
						}
					}
				}

				for (i=menu->scroll_offset; i<menu->max_on_menu+menu->scroll_offset; i++ )	{
					newmenu_get_item_bounds(menu, i, &x1, &y1, &x2, &y2);

					if (((mx > x1) && (mx < x2)) && ((my > y1) && (my < y2)) && (menu->items[i].type != NM_TYPE_TEXT) ) {
						if (i != menu->citem) {
							if(Hack_DblClick_MenuMode) menu->dblclick_flag = 0;
						}

						menu->citem = i;

						if ( menu->items[menu->citem].type == NM_TYPE_SLIDER ) {
							char slider_text[NM_MAX_TEXT_LEN+1], *p, *s1;
							int slider_width, height, aw, sleft_width, sright_width, smiddle_width;

							strcpy(slider_text, menu->items[menu->citem].saved_text);
							p = strchr(slider_text, '\t');
							if (p) {
								*p = '\0';
								s1 = p+1;
							}
							if (p) {
								gr_get_string_size(s1, &slider_width, &height, &aw);
								gr_get_string_size(SLIDER_LEFT, &sleft_width, &height, &aw);
								gr_get_string_size(SLIDER_RIGHT, &sright_width, &height, &aw);
								gr_get_string_size(SLIDER_MIDDLE, &smiddle_width, &height, &aw);

								x1 = grd_curcanv->cv_bitmap.bm_x + menu->items[menu->citem].x + menu->items[menu->citem].w - slider_width;
								x2 = x1 + slider_width + sright_width;
								if ( (mx > x1) && (mx < (x1 + sleft_width)) && (menu->items[menu->citem].value != menu->items[menu->citem].min_value) ) {
									menu->items[menu->citem].value = menu->items[menu->citem].min_value;
									changed = 1;
								} else if ( (mx < x2) && (mx > (x2 - sright_width)) && (menu->items[menu->citem].value != menu->items[menu->citem].max_value) ) {
									menu->items[menu->citem].value = menu->items[menu->citem].max_value;
									changed = 1;
								} else if ( (mx > (x1 + sleft_width)) && (mx < (x2 - sright_width)) ) {
									int num_values, value_width, new_value;

									num_values = menu->items[menu->citem].max_value - menu->items[menu->citem].min_value + 1;
									value_width = (slider_width - sleft_width - sright_width) / num_values;
									new_value = (mx - x1 - sleft_width) / value_width;
									if ( menu->items[menu->citem].value != new_value ) {
										menu->items[menu->citem].value = new_value;
										changed = 1;
									}
								}
								*p = '\t';
							}
						}
						if (menu->citem == old_choice)
							break;
						if ((menu->items[menu->citem].type==NM_TYPE_INPUT) && (menu->citem!=old_choice))
							menu->items[menu->citem].value = -1;
						if ((old_choice>-1) && (menu->items[old_choice].type==NM_TYPE_INPUT_MENU) && (old_choice!=menu->citem))	{
							menu->items[old_choice].group=0;
							strcpy(menu->items[old_choice].text, menu->items[old_choice].saved_text );
							menu->items[old_choice].value = -1;
						}
						break;
					}
				}
			}

		#ifdef ANDROID
			if ((event->type == EVENT_MOUSE_BUTTON_UP) && !menu->drag_happened)
			{
				mouse_get_pos(&mx, &my, &mz);
				if (android_tap_outside_game_menu(menu, mx, my)) {
					window_close(menu->wind);
					gr_set_current_canvas(save_canvas);
					return 1;
				}
			}
		#endif

#ifdef ANDROID
			if ((event->type == EVENT_MOUSE_BUTTON_UP) && menu->reorderitems)
			{
				newmenu_reorder_drop(menu);
				gr_set_current_canvas(save_canvas);
				return 1;
			}
#endif

			if ((event->type == EVENT_MOUSE_BUTTON_UP) && !menu->all_text && !menu->drag_happened && (menu->citem != -1) && (menu->items[menu->citem].type == NM_TYPE_MENU) )
			{
				mouse_get_pos(&mx, &my, &mz);
				for (i=menu->scroll_offset; i<menu->max_on_menu+menu->scroll_offset; i++ )	{
					newmenu_get_item_bounds(menu, i, &x1, &y1, &x2, &y2);
					if (((mx > x1) && (mx < x2)) && ((my > y1) && (my < y2))) {
						if (Hack_DblClick_MenuMode) {
							if (menu->dblclick_flag)
							{
								// Tell callback, allow staying in menu
								event->type = EVENT_NEWMENU_SELECTED;
								if (menu->subfunction && (*menu->subfunction)(menu, event, menu->userdata))
									return 1;

								if (menu->rval)
									*menu->rval = menu->citem;
								window_close(menu->wind);
								gr_set_current_canvas(save_canvas);
								return 1;
							}
							else menu->dblclick_flag = 1;
						}
						else
						{
							// Tell callback, allow staying in menu
							event->type = EVENT_NEWMENU_SELECTED;
							if (menu->subfunction && (*menu->subfunction)(menu, event, menu->userdata))
								return 1;

							if (menu->rval)
								*menu->rval = menu->citem;
							window_close(menu->wind);
							gr_set_current_canvas(save_canvas);
							return 1;
						}
					}
				}
			}

			if ((event->type == EVENT_MOUSE_BUTTON_UP) && (menu->citem>-1) && (menu->items[menu->citem].type==NM_TYPE_INPUT_MENU) && (menu->items[menu->citem].group==0))
			{
				menu->items[menu->citem].group = 1;
				if ( !d_strnicmp( menu->items[menu->citem].saved_text, TXT_EMPTY, strlen(TXT_EMPTY) ) )	{
					menu->items[menu->citem].text[0] = 0;
					menu->items[menu->citem].value = -1;
				} else {
					strip_end_whitespace(menu->items[menu->citem].text);
				}
			}

#ifdef ANDROID
			// Deferred check/radio toggle on button-up for scroll boxes (prevents accidental toggle during drag)
			if ((event->type == EVENT_MOUSE_BUTTON_UP) && menu->is_scroll_box && !menu->drag_happened && (menu->citem > -1))
			{
				mouse_get_pos(&mx, &my, &mz);
				newmenu_get_item_bounds(menu, menu->citem, &x1, &y1, &x2, &y2);
				if (((mx > x1) && (mx < x2)) && ((my > y1) && (my < y2))) {
					switch (menu->items[menu->citem].type) {
						case NM_TYPE_CHECK:
							menu->items[menu->citem].value = !menu->items[menu->citem].value;
							if (menu->is_scroll_box)
								menu->last_scroll_check=-1;
							changed = 1;
							break;
						case NM_TYPE_RADIO:
							for (i=0; i<menu->nitems; i++) {
								if ((i!=menu->citem) && (menu->items[i].type==NM_TYPE_RADIO) && (menu->items[i].group==menu->items[menu->citem].group) && (menu->items[i].value)) {
									menu->items[i].value = 0;
									changed = 1;
								}
							}
							menu->items[menu->citem].value = 1;
							break;
					}
				}
			}
#endif

			gr_set_current_canvas(save_canvas);

			if (changed && menu->subfunction)
			{
				event->type = EVENT_NEWMENU_CHANGED;
				(*menu->subfunction)(menu, event, menu->userdata);
			}
			break;
		}
		case MBTN_RIGHT:
			if (menu->mouse_state)
			{
				if ( (menu->citem>-1) && (menu->items[menu->citem].type==NM_TYPE_INPUT_MENU) && (menu->items[menu->citem].group==1))	{
					menu->items[menu->citem].group=0;
					strcpy(menu->items[menu->citem].text, menu->items[menu->citem].saved_text );
					menu->items[menu->citem].value = -1;
				} else {
					window_close(menu->wind);
					return 1;
				}
			}
			break;
		case MBTN_Z_UP:
			if (menu->mouse_state)
				newmenu_scroll(menu, -1);
			break;
		case MBTN_Z_DOWN:
			if (menu->mouse_state)
				newmenu_scroll(menu, 1);
			break;
	}

	return 0;
}

int newmenu_key_command(window *wind, d_event *event, newmenu *menu)
{
	if (!menu || !menu->items || menu->citem < 0 || menu->citem >= menu->nitems)
		return 0;

	newmenu_item *item = &menu->items[menu->citem];
	int k = event_key_get(event);
	int old_choice, i;
	int changed = 0;
	int rval = 1;

#ifndef ANDROID
	if (keyd_pressed[KEY_NUMLOCK])
	{
		switch( k )
		{
			case KEY_PAD0: k = KEY_0;  break;
			case KEY_PAD1: k = KEY_1;  break;
			case KEY_PAD2: k = KEY_2;  break;
			case KEY_PAD3: k = KEY_3;  break;
			case KEY_PAD4: k = KEY_4;  break;
			case KEY_PAD5: k = KEY_5;  break;
			case KEY_PAD6: k = KEY_6;  break;
			case KEY_PAD7: k = KEY_7;  break;
			case KEY_PAD8: k = KEY_8;  break;
			case KEY_PAD9: k = KEY_9;  break;
			case KEY_PADPERIOD: k = KEY_PERIOD; break;
		}
	}
#endif

	old_choice = menu->citem;

	switch( k )	{
		case KEY_HOME:
		case KEY_PAD7:
			newmenu_scroll(menu, -menu->nitems);
			break;
		case KEY_END:
		case KEY_PAD1:
			newmenu_scroll(menu, menu->nitems);
			break;
		case KEY_TAB + KEY_SHIFTED:
		case KEY_UP:
		case KEY_PAGEUP:
		case KEY_PAD8:
			if (k == KEY_PAGEUP)
				newmenu_scroll(menu, -10);
			else
				newmenu_scroll(menu, -1);

			if ((menu->items[menu->citem].type==NM_TYPE_INPUT) && (menu->citem!=old_choice))
				menu->items[menu->citem].value = -1;
			if ((old_choice>-1) && (menu->items[old_choice].type==NM_TYPE_INPUT_MENU) && (old_choice!=menu->citem))	{
				menu->items[old_choice].group=0;
				strcpy(menu->items[old_choice].text, menu->items[old_choice].saved_text );
				menu->items[old_choice].value = -1;
			}
			break;
		case KEY_TAB:
		case KEY_DOWN:
		case KEY_PAGEDOWN:
		case KEY_PAD2:
			if (k == KEY_PAGEDOWN)
				newmenu_scroll(menu, 10);
			else
				newmenu_scroll(menu, 1);

			if ((menu->items[menu->citem].type==NM_TYPE_INPUT) && (menu->citem!=old_choice))
				menu->items[menu->citem].value = -1;
			if ( (old_choice>-1) && (menu->items[old_choice].type==NM_TYPE_INPUT_MENU) && (old_choice!=menu->citem))	{
				menu->items[old_choice].group=0;
				strcpy(menu->items[old_choice].text, menu->items[old_choice].saved_text );
				menu->items[old_choice].value = -1;
			}
			break;
		case KEY_SPACEBAR:
			if ( menu->citem > -1 )	{

				switch( item->type )	{
					case NM_TYPE_MENU:
					case NM_TYPE_INPUT:
					case NM_TYPE_INPUT_MENU:
						break;
					case NM_TYPE_CHECK:
						if ( item->value )
							item->value = 0;
						else
							item->value = 1;
						if (menu->is_scroll_box)
						{
							if (menu->citem==(menu->max_on_menu+menu->scroll_offset-1) || menu->citem==menu->scroll_offset)
							{
								menu->last_scroll_check=-1;
							}
						}

						changed = 1;
						break;
					case NM_TYPE_RADIO:
						for (i=0; i<menu->nitems; i++ )	{
							if ((i!=menu->citem) && (menu->items[i].type==NM_TYPE_RADIO) && (menu->items[i].group==item->group) && (menu->items[i].value) )	{
								menu->items[i].value = 0;
								changed = 1;
							}
						}
						item->value = 1;
						changed = 1;
						break;
				}
			}
			break;

		case KEY_SHIFTED+KEY_UP:
			if (newmenu_reorder_move(menu, -1))
				changed = 1;
			break;
		case KEY_SHIFTED+KEY_DOWN:
			if (newmenu_reorder_move(menu, 1))
				changed = 1;
			break;
		case KEY_ENTER:
		case KEY_PADENTER:
			if ((menu->citem > -1) && (item->type == NM_TYPE_CHECK || item->type == NM_TYPE_RADIO))	{
				switch (item->type)	{
					case NM_TYPE_CHECK:
						item->value = !item->value;
						if (menu->is_scroll_box)
						{
							if (menu->citem==(menu->max_on_menu+menu->scroll_offset-1) || menu->citem==menu->scroll_offset)
							{
								menu->last_scroll_check=-1;
							}
						}

						changed = 1;
						break;
					case NM_TYPE_RADIO:
						for (i=0; i<menu->nitems; i++ )	{
							if ((i!=menu->citem) && (menu->items[i].type==NM_TYPE_RADIO) && (menu->items[i].group==item->group) && (menu->items[i].value) )	{
								menu->items[i].value = 0;
								changed = 1;
							}
						}
						item->value = 1;
						changed = 1;
						break;
				}
			} else if ( (menu->citem>-1) && (item->type==NM_TYPE_INPUT_MENU) && (item->group==0))	{
				item->group = 1;
				if ( !d_strnicmp( item->saved_text, TXT_EMPTY, strlen(TXT_EMPTY) ) )	{
					item->text[0] = 0;
					item->value = -1;
				} else {
					strip_end_whitespace(item->text);
				}
			} else
			{
				if (item->type==NM_TYPE_INPUT_MENU)
					item->group = 0;	// go out of editing mode

				// Tell callback, allow staying in menu
				event->type = EVENT_NEWMENU_SELECTED;
				if (menu->subfunction && (*menu->subfunction)(menu, event, menu->userdata))
					return 1;

				if (menu->rval)
					*menu->rval = menu->citem;
				window_close(menu->wind);
				return 1;
			}
			break;

		case KEY_ESC:
			if ( (menu->citem>-1) && (item->type==NM_TYPE_INPUT_MENU) && (item->group==1))	{
				item->group=0;
				strcpy(item->text, item->saved_text );
				item->value = -1;
			} else {
				window_close(menu->wind);
				return 1;
			}
			break;

#ifndef NDEBUG
		case KEY_BACKSP:
			if ( (menu->citem>-1) && (item->type!=NM_TYPE_INPUT)&&(item->type!=NM_TYPE_INPUT_MENU))
				Int3();
			break;
#endif

		default:
			rval = 0;
			break;
	}

	if ( menu->citem > -1 )	{
		int ascii;

		// Alerting callback of every keypress for NM_TYPE_INPUT. Alternatively, just respond to EVENT_NEWMENU_SELECTED
		if ( ((item->type==NM_TYPE_INPUT)||((item->type==NM_TYPE_INPUT_MENU)&&(item->group==1)) )&& (old_choice==menu->citem) )	{
			if ( k==KEY_LEFT || k==KEY_BACKSP || k==KEY_PAD4 )	{
				if (item->value==-1) item->value = strlen(item->text);
				if (item->value > 0)
					item->value--;
				item->text[item->value] = 0;

				if (item->type==NM_TYPE_INPUT)
					changed = 1;
				rval = 1;
			} else {
				ascii = key_ascii();
				if ((ascii < 255 ) && (item->value < item->text_len ))
				{
					int allowed;

					if (item->value==-1) {
						item->value = 0;
					}

					allowed = char_allowed(ascii);

					if (!allowed && ascii==' ' && char_allowed('_')) {
						ascii = '_';
						allowed=1;
					}

					if (allowed) {
						item->text[item->value++] = ascii;
						item->text[item->value] = 0;

						if (item->type==NM_TYPE_INPUT)
							changed = 1;
					}
				}
			}
		}
		else if ((item->type!=NM_TYPE_INPUT) && (item->type!=NM_TYPE_INPUT_MENU) )
		{
			ascii = key_ascii();
			if (ascii < 255 ) {
				int choice1 = menu->citem;
				ascii = toupper(ascii);
				do {
					int i,ch;
					choice1++;
					if (choice1 >= menu->nitems )
						choice1=0;

					for (i=0;(ch=menu->items[choice1].text[i])!=0 && ch==' ';i++);

					if ( ( (menu->items[choice1].type==NM_TYPE_MENU) ||
						  (menu->items[choice1].type==NM_TYPE_CHECK) ||
						  (menu->items[choice1].type==NM_TYPE_RADIO) ||
						  (menu->items[choice1].type==NM_TYPE_NUMBER) ||
						  (menu->items[choice1].type==NM_TYPE_SLIDER) )
						&& (ascii==toupper(ch)) )
					{
						k = 0;
						menu->citem = choice1;
					}

					while (menu->citem+4>=menu->max_on_menu+menu->scroll_offset && menu->scroll_offset < menu->nitems-menu->max_on_menu)
						menu->scroll_offset++;
					while (menu->citem-4<menu->scroll_offset && menu->scroll_offset > 0)
						menu->scroll_offset--;

				} while (choice1 != menu->citem );
			}
		}

		if ( (item->type==NM_TYPE_NUMBER) || (item->type==NM_TYPE_SLIDER))
		{
			switch( k ) {
				case KEY_LEFT:
				case KEY_PAD4:
					item->value -= 1;
					changed = 1;
					rval = 1;
					break;
				case KEY_RIGHT:
				case KEY_PAD6:
					item->value++;
					changed = 1;
					rval = 1;
					break;
				case KEY_SPACEBAR:
					item->value += 10;
					changed = 1;
					rval = 1;
					break;
				case KEY_BACKSP:
					item->value -= 10;
					changed = 1;
					rval = 1;
					break;
			}

			if (item->value < item->min_value) item->value=item->min_value;
			if (item->value > item->max_value) item->value=item->max_value;
		}

	}

	if (changed && menu->subfunction)
	{
		event->type = EVENT_NEWMENU_CHANGED;
		(*menu->subfunction)(menu, event, menu->userdata);
	}

	return rval;
}

void newmenu_create_structure( newmenu *menu )
{
	int i,j,aw, tw, th, twidth,fm,right_offset;
	int nmenus, nothers;
	grs_font *save_font;
	grs_canvas *save_canvas;
	int string_width, string_height, average_width;

	save_canvas = grd_curcanv;

	gr_set_current_canvas(NULL);

	save_font = grd_curcanv->cv_font;

	tw = th = 0;

	if ( menu->title )	{
		gr_set_curfont(HUGE_FONT);
		gr_get_string_size(menu->title,&string_width,&string_height,&average_width );
		tw = string_width;
		th = string_height;
	}
	if ( menu->subtitle )	{
		gr_set_curfont(MEDIUM3_FONT);
		gr_get_string_size(menu->subtitle,&string_width,&string_height,&average_width );
		if (string_width > tw )
			tw = string_width;
		th += string_height;
	}

	th += FSPACY(5);		//put some space between titles & body

	gr_set_curfont(newmenu_get_body_font(menu));
	menu->scroll_line_spacing = (int)LINE_SPACING;

	menu->w = aw = 0;
	menu->h = th;
	nmenus = nothers = 0;

	// Find menu height & width (store in w,h)
	for (i=0; i<menu->nitems; i++ )	{
		menu->items[i].y = menu->h;
		gr_get_string_size(menu->items[i].text,&string_width,&string_height,&average_width );
		menu->items[i].right_offset = 0;

		menu->items[i].saved_text[0] = '\0';

		if ( menu->items[i].type == NM_TYPE_SLIDER )	{
			int index,w1,h1,aw1;
			nothers++;
			index = sprintf( menu->items[i].saved_text, "%s", SLIDER_LEFT );
			for (j=0; j<(menu->items[i].max_value-menu->items[i].min_value+1); j++ )	{
				index+= sprintf( menu->items[i].saved_text + index, "%s", SLIDER_MIDDLE );
			}
			sprintf( menu->items[i].saved_text + index, "%s", SLIDER_RIGHT );
			gr_get_string_size(menu->items[i].saved_text,&w1,&h1,&aw1 );
			string_width += w1 + aw;
		}

		if ( menu->items[i].type == NM_TYPE_MENU )	{
			nmenus++;
		}

		if ( menu->items[i].type == NM_TYPE_CHECK )	{
			int w1,h1,aw1;
			nothers++;
			gr_get_string_size(NORMAL_CHECK_BOX, &w1, &h1, &aw1  );
			menu->items[i].right_offset = w1;
			gr_get_string_size(CHECKED_CHECK_BOX, &w1, &h1, &aw1  );
			if (w1 > menu->items[i].right_offset)
				menu->items[i].right_offset = w1;
		}

		if (menu->items[i].type == NM_TYPE_RADIO ) {
			int w1,h1,aw1;
			nothers++;
			gr_get_string_size(NORMAL_RADIO_BOX, &w1, &h1, &aw1  );
			menu->items[i].right_offset = w1;
			gr_get_string_size(CHECKED_RADIO_BOX, &w1, &h1, &aw1  );
			if (w1 > menu->items[i].right_offset)
				menu->items[i].right_offset = w1;
		}

		if  (menu->items[i].type==NM_TYPE_NUMBER )	{
			int w1,h1,aw1;
			char test_text[20];
			nothers++;
			sprintf( test_text, "%d", menu->items[i].max_value );
			gr_get_string_size( test_text, &w1, &h1, &aw1 );
			menu->items[i].right_offset = w1;
			sprintf( test_text, "%d", menu->items[i].min_value );
			gr_get_string_size( test_text, &w1, &h1, &aw1 );
			if ( w1 > menu->items[i].right_offset)
				menu->items[i].right_offset = w1;
		}

		if ((menu->items[i].type == NM_TYPE_INPUT) || (menu->items[i].type == NM_TYPE_INPUT_MENU))
		{
			Assert( strlen(menu->items[i].text) < NM_MAX_TEXT_LEN );
			strcpy(menu->items[i].saved_text, menu->items[i].text );

			string_width = menu->items[i].text_len*FSPACX(8)+menu->items[i].text_len;
			if ( menu->items[i].type == NM_TYPE_INPUT && string_width > MAX_TEXT_WIDTH )
				string_width = MAX_TEXT_WIDTH;

			menu->items[i].value = -1;
			menu->items[i].group = 0;
			if (menu->items[i].type == NM_TYPE_INPUT_MENU)
				nmenus++;
			else
				nothers++;
		}

		menu->items[i].w = string_width;
		menu->items[i].h = string_height;

		if ( string_width > menu->w )
			menu->w = string_width;		// Save maximum width
		if ( average_width > aw )
			aw = average_width;
		menu->h += string_height+FSPACY(1);		// Find the height of all strings
	}

	if (i > menu->max_on_menu)
	{
		menu->is_scroll_box=1;
		menu->h = th+(menu->scroll_line_spacing*menu->max_on_menu);
		menu->max_displayable=menu->max_on_menu;

		// if our last citem was > menu->max_on_menu, make sure we re-scroll when we call this menu again
		if (menu->citem > menu->max_on_menu-4)
		{
			menu->scroll_offset = menu->citem - (menu->max_on_menu-4);
			if (menu->scroll_offset + menu->max_on_menu > menu->nitems)
				menu->scroll_offset = menu->nitems - menu->max_on_menu;
		}
	}
	else
	{
		menu->is_scroll_box=0;
		menu->max_on_menu=i;
	}

	right_offset=0;

	for (i=0; i<menu->nitems; i++ )	{
		menu->items[i].w = menu->w;
		if (menu->items[i].right_offset > right_offset )
			right_offset = menu->items[i].right_offset;
	}

	menu->w += right_offset;

	twidth = 0;
	if ( tw > menu->w )	{
		twidth = ( tw - menu->w )/2;
		menu->w = tw;
	}

	// Find min point of menu border
	menu->w += BORDERX*2;
	menu->h += BORDERY*2;

	menu->x = (GWIDTH-menu->w)/2;
	menu->y = (GHEIGHT-menu->h)/2;

	if ( menu->x < 0 ) menu->x = 0;
	if ( menu->y < 0 ) menu->y = 0;

	nm_draw_background1( menu->filename );

	// Update all item's x & y values.
	for (i=0; i<menu->nitems; i++ )	{
		menu->items[i].x = BORDERX + twidth + right_offset;
		menu->items[i].y += BORDERY;
		if ( menu->items[i].type==NM_TYPE_RADIO )	{
			fm = -1;	// find first marked one
			for ( j=0; j<menu->nitems; j++ )	{
				if ( menu->items[j].type==NM_TYPE_RADIO && menu->items[j].group==menu->items[i].group )	{
					if (fm==-1 && menu->items[j].value)
						fm = j;
					menu->items[j].value = 0;
				}
			}
			if ( fm>=0 )
				menu->items[fm].value=1;
			else
				menu->items[i].value=1;
		}
	}

	if (menu->citem != -1)
	{
		if (menu->citem < 0 ) menu->citem = 0;
		if (menu->citem > menu->nitems-1 ) menu->citem = menu->nitems-1;

#ifdef NEWMENU_MOUSE
		menu->dblclick_flag = 1;
#endif
		i = 0;
		while ( menu->items[menu->citem].type==NM_TYPE_TEXT )	{
			menu->citem++;
			i++;
			if (menu->citem >= menu->nitems ) {
				menu->citem=0;
			}
			if (i > menu->nitems ) {
				menu->citem=0;
				menu->all_text=1;
				break;
			}
		}
#ifdef ANDROID
		if (menu->items[menu->citem].type == NM_TYPE_INPUT_MENU) {
			menu->items[menu->citem].group = 1;
			if (!d_strnicmp(menu->items[menu->citem].saved_text, TXT_EMPTY, strlen(TXT_EMPTY))) {
				menu->items[menu->citem].text[0] = 0;
				menu->items[menu->citem].value = -1;
			} else {
				strip_end_whitespace(menu->items[menu->citem].text);
			}
		}
#endif
	}

	menu->mouse_state = 0;
	menu->swidth = SWIDTH;
	menu->sheight = SHEIGHT;
	menu->fntscalex = FNTScaleX;
	menu->fntscaley = FNTScaleY;
	gr_set_curfont(save_font);
	gr_set_current_canvas(save_canvas);
}

static void newmenu_draw_contents(newmenu *menu)
{
	int th = 0, ty, sx, sy;
	int i;
	int scroll_line_spacing;
	int string_width, string_height, average_width;

	gr_set_curfont(newmenu_get_body_font(menu));
	scroll_line_spacing = newmenu_get_scroll_line_spacing(menu);

	ty = BORDERY;

	if ( menu->title )	{
		gr_set_curfont(HUGE_FONT);
		gr_set_fontcolor( BM_XRGB(31,31,31), -1 );
		gr_get_string_size(menu->title,&string_width,&string_height,&average_width );
		th = string_height;
		gr_string( 0x8000, ty, menu->title );
	}

	if ( menu->subtitle )	{
		gr_set_curfont(MEDIUM3_FONT);
		gr_set_fontcolor( BM_XRGB(21,21,21), -1 );
		gr_get_string_size(menu->subtitle,&string_width,&string_height,&average_width );
		gr_string( 0x8000, ty+th, menu->subtitle );
	}

	gr_set_curfont(newmenu_get_body_font(menu));

	// Redraw everything...
	for (i=menu->scroll_offset; i<menu->max_displayable+menu->scroll_offset; i++ )
	{
		draw_item( &menu->items[i], (i==menu->citem && !menu->all_text),
#ifdef ANDROID
		           (menu->reorderitems && menu->reorder.grabbed && i == menu->citem),
#else
		           0,
#endif
		           menu->tiny_mode, menu->tabs_flag, menu->scroll_offset, scroll_line_spacing );

	}

	if (menu->is_scroll_box)
	{
		menu->last_scroll_check=menu->scroll_offset;
		gr_set_curfont(newmenu_get_scroll_marker_font(menu));

		sy=menu->items[menu->scroll_offset].y-(scroll_line_spacing*menu->scroll_offset);
		sx=BORDERX-FSPACX(12);

		if (menu->scroll_offset!=0)
			gr_printf( sx, sy, UP_ARROW_MARKER );
		else
			gr_printf( sx, sy, "  " );

		sy=menu->items[menu->scroll_offset+menu->max_displayable-1].y-(scroll_line_spacing*menu->scroll_offset);
		sx=BORDERX-FSPACX(12);

		if (menu->scroll_offset+menu->max_displayable<menu->nitems)
			gr_printf( sx, sy, DOWN_ARROW_MARKER );
		else
			gr_printf( sx, sy, "  " );

	}

	{
		d_event event;

		event.type = EVENT_NEWMENU_DRAW;
		if (menu->subfunction)
			(*menu->subfunction)(menu, &event, menu->userdata);
	}
}

#ifdef ANDROID
static void android_menu_scale_blit_source_region(grs_bitmap *bitmap,
                                                  const android_menu_scale_result *result, int masked)
{
	int row;
	grs_bitmap cropped;

	if (!bitmap || !result || !result->active)
		return;

	gr_init_bitmap_alloc(&cropped, BM_LINEAR, 0, 0, result->src.w,
	                     result->src.h, result->src.w);
	for (row = 0; row < result->src.h; row++)
		memcpy(cropped.bm_data + row * result->src.w,
		       bitmap->bm_data + (result->src.y + row) * bitmap->bm_rowsize +
		       result->src.x,
		       result->src.w);
	android_menu_scale_blit_bitmap(&cropped, result, masked);
	gr_free_bitmap_data(&cropped);
}

static int android_menu_scale_round_coord(int value, float scale)
{
	return (int) (value * scale + 0.5f);
}

static void android_newmenu_scale_items(newmenu_item *dst, const newmenu_item *src,
                                        int count, float scale)
{
	int i;

	for (i = 0; i < count; i++) {
		dst[i] = src[i];
		dst[i].x = android_menu_scale_round_coord(src[i].x, scale);
		dst[i].y = android_menu_scale_round_coord(src[i].y, scale);
		dst[i].w = android_menu_scale_round_coord(src[i].w, scale);
		dst[i].h = android_menu_scale_round_coord(src[i].h, scale);
		dst[i].right_offset = android_menu_scale_round_coord(src[i].right_offset, scale);
	}
}

static void android_newmenu_draw_direct_contents(newmenu *menu,
                                                 android_menu_scale_result *result)
{
	grs_bitmap overlay_bitmap;
	grs_canvas overlay_canvas, menu_canvas;
	grs_canvas *save_canvas = grd_curcanv;
	android_menu_scale_draw_state draw_state;
	newmenu menu_copy;
	newmenu_item *items_copy;
	float scale;
	int menu_x, menu_y;

	if (!menu || !result || !result->active || result->dst.w <= 0 || result->dst.h <= 0)
		return;

	scale = result->scale;
	if (scale <= 1.0f)
		return;

	items_copy = d_malloc(sizeof(newmenu_item) * menu->nitems);
	if (!items_copy)
		return;

	menu_copy = *menu;
	android_newmenu_scale_items(items_copy, menu->items, menu->nitems, scale);
	menu_copy.items = items_copy;
	menu_copy.w = android_menu_scale_round_coord(menu->w, scale);
	menu_copy.h = android_menu_scale_round_coord(menu->h, scale);
	menu_copy.scroll_line_spacing = android_menu_scale_round_coord(
		newmenu_get_scroll_line_spacing(menu), scale);

	gr_init_bitmap_alloc(&overlay_bitmap, BM_LINEAR, 0, 0, result->dst.w,
	                     result->dst.h, result->dst.w);
	memset(overlay_bitmap.bm_data, TRANSPARENCY_COLOR, result->dst.w * result->dst.h);
	gr_init_canvas(&overlay_canvas, overlay_bitmap.bm_data, BM_LINEAR,
	               result->dst.w, result->dst.h);

	if (android_menu_scale_begin_scaled_draw(scale, &draw_state)) {
		gr_set_current_canvas(&overlay_canvas);
		menu_x = android_menu_scale_round_coord(menu->x - result->src.x, scale);
		menu_y = android_menu_scale_round_coord(menu->y - result->src.y, scale);
		gr_init_sub_canvas(&menu_canvas, &overlay_canvas, menu_x, menu_y,
		                   menu_copy.w, menu_copy.h);
		gr_set_current_canvas(&menu_canvas);
		newmenu_draw_contents(&menu_copy);
		android_menu_scale_end_scaled_draw(&draw_state);

		result->direct_render = 1;
		result->render_w = result->dst.w;
		result->render_h = result->dst.h;
		result->render_scale = scale;
	}

	gr_set_current_canvas(save_canvas);
	android_menu_scale_blit_bitmap(&overlay_bitmap, result, 1);
	gr_set_current_canvas(save_canvas);
	gr_free_bitmap_data(&overlay_bitmap);
	d_free(items_copy);
}

static void android_newmenu_draw_scaled(newmenu *menu,
                                        android_menu_scale_result *result)
{
	int masked = menu->filename != NULL;
	grs_bitmap source_bitmap;
	grs_canvas source_canvas;
	grs_canvas *save_canvas = grd_curcanv;

	if (menu->filename != NULL) {
		gr_set_current_canvas(NULL);
		nm_draw_background1(menu->filename);
		if (Game_wind == NULL) {
			load_palette(MENU_PALETTE,0,1);
			gr_palette_load(gr_palette);
		}
	}

	gr_init_bitmap_alloc(&source_bitmap, BM_LINEAR, 0, 0, SWIDTH, SHEIGHT, SWIDTH);
	if (masked)
		memset(source_bitmap.bm_data, TRANSPARENCY_COLOR, SWIDTH * SHEIGHT);
	gr_init_canvas(&source_canvas, source_bitmap.bm_data, BM_LINEAR, SWIDTH, SHEIGHT);
	gr_set_current_canvas(&source_canvas);
	if (menu->filename == NULL)
		nm_draw_background(menu->x-(menu->is_scroll_box?FSPACX(5):0),menu->y,menu->x+menu->w,menu->y+menu->h);

	gr_set_current_canvas(save_canvas);
	android_menu_scale_blit_source_region(&source_bitmap, result, masked);
	android_newmenu_draw_direct_contents(menu, result);
	gr_set_current_canvas(save_canvas);
	gr_free_bitmap_data(&source_bitmap);
}
#endif

int newmenu_draw(window *wind, newmenu *menu)
{
	grs_canvas *menu_canvas = window_get_canvas(wind), *save_canvas = grd_curcanv;
	#ifdef ANDROID
	android_menu_scale_result menu_scale;
	int have_menu_scale;
	#endif

	if (menu->swidth != SWIDTH || menu->sheight != SHEIGHT || menu->fntscalex != FNTScaleX || menu->fntscalex != FNTScaleY)
	{
		newmenu_create_structure ( menu );
		if (menu_canvas)
		{
			gr_init_sub_canvas(menu_canvas, &grd_curscreen->sc_canvas, menu->x, menu->y, menu->w, menu->h);
		}
	}

#ifdef ANDROID
	{
		int source_x = menu->x - (menu->is_scroll_box ? (int)FSPACX(5) : 0);
		int source_y = menu->y;
		int source_w = menu->x + menu->w - source_x;
		int source_h = menu->y + menu->h - source_y;
		extern volatile int g_levelcomplete_active;

		if (g_levelcomplete_active)
			have_menu_scale = 0;
		else
			have_menu_scale = android_menu_scale_compute_cropped(source_x, source_y, source_w, source_h,
			                                                   SWIDTH, SHEIGHT, BORDERX, BORDERY,
			                                                   &menu_scale);
	}
	if (have_menu_scale) {
		android_newmenu_draw_scaled(menu, &menu_scale);
		android_menu_scale_publish(&menu_scale);
	} else
#endif
	{
		gr_set_current_canvas( NULL );
		nm_draw_background1(menu->filename);
		if (menu->filename != NULL && Game_wind == NULL) {
			load_palette(MENU_PALETTE,0,1);
			gr_palette_load(gr_palette);
		}
		if (menu->filename == NULL)
			nm_draw_background(menu->x-(menu->is_scroll_box?FSPACX(5):0),menu->y,menu->x+menu->w,menu->y+menu->h);

		gr_set_current_canvas( menu_canvas );
		newmenu_draw_contents(menu);
		gr_set_current_canvas(save_canvas);
#ifdef ANDROID
		android_menu_scale_clear();
#endif
	}

#ifdef ANDROID
	{
		newmenu_item *current_item = NULL;
		int front_menu = window_get_front() == wind;

		if (menu->items && menu->citem >= 0 && menu->citem < menu->nitems)
			current_item = &menu->items[menu->citem];

		if (current_item &&
		    (current_item->type == NM_TYPE_INPUT ||
		     (current_item->type == NM_TYPE_INPUT_MENU && current_item->group == 1))) {
			extern void android_update_keyboard_field_y(int field_y);
			int visible_y = menu->y + current_item->y - (newmenu_get_scroll_line_spacing(menu) * menu->scroll_offset);
			android_update_keyboard_field_y(visible_y);

			if (front_menu) {
				extern void android_show_keyboard(int numeric, int field_y, const char *initial_text);
				extern void android_hide_keyboard(void);
				extern int android_is_keyboard_shown(void);
				int keyboard_shown = android_is_keyboard_shown();
				// Don't open keyboard while finger is down (drag in progress) --
				// dragging over a text-input item would pop the keyboard, shift
				// the blit offset, and cause selection oscillation.
				if (!keyboard_shown && !menu->mouse_state) {
					android_show_keyboard(0, visible_y, current_item->text ? current_item->text : "");
				} else if (keyboard_shown && !current_item) {
					android_hide_keyboard();
				}
			}
		} else if (front_menu) {
			extern void android_hide_keyboard(void);
			extern int android_is_keyboard_shown(void);
			if (android_is_keyboard_shown())
				android_hide_keyboard();
		}
	}
#endif

	return 1;
}

int newmenu_handler(window *wind, d_event *event, newmenu *menu)
{
	if (event->type == EVENT_WINDOW_CLOSED)
		return 0;

	if (menu->subfunction)
	{
		int rval;
#ifdef ANDROID
		newmenu_item *wrapped_items = NULL;
		int wrapped_nitems = 0;

		if (event->type == EVENT_WINDOW_CLOSE && menu->android_original_items) {
			wrapped_items = menu->items;
			wrapped_nitems = menu->nitems;
			menu->items = menu->android_original_items;
			menu->nitems = menu->android_original_nitems;
		}
#endif
		rval = (*menu->subfunction)(menu, event, menu->userdata);
#ifdef ANDROID
		if (wrapped_items) {
			menu->items = wrapped_items;
			menu->nitems = wrapped_nitems;
		}
#endif

		if (!window_exists(wind))
			return 1;	// some subfunction closed the window: bail!

		if (rval)
		{
			if (rval < -1)
			{
				if (menu->rval)
					*menu->rval = rval;
				window_close(wind);
			}

			return 1;		// event handled
		}
	}

	switch (event->type)
	{
		case EVENT_WINDOW_ACTIVATED:
			game_flush_inputs();
			event_toggle_focus(0);
			key_toggle_repeat(1);
			break;

		case EVENT_WINDOW_DEACTIVATED:
			//event_toggle_focus(1);	// No cursor recentering
			key_toggle_repeat(1);
			menu->mouse_state = 0;
			break;

		case EVENT_MOUSE_BUTTON_DOWN:
		case EVENT_MOUSE_BUTTON_UP:
		{
			int button = event_mouse_get_button(event);
			menu->mouse_state = event->type == EVENT_MOUSE_BUTTON_DOWN;
#ifdef ANDROID
			if (button == MBTN_LEFT) {
				if (menu->mouse_state && menu->is_scroll_box) {
					int mx, my, mz;
					mouse_get_pos(&mx, &my, &mz);
					menu->drag_start_y = my;
					menu->drag_happened = 0;
				} else {
					menu->drag_start_y = -1;
				}
			}
#endif
			return newmenu_mouse(wind, event, menu, button);
		}

		case EVENT_KEY_COMMAND:
			return newmenu_key_command(wind, event, menu);
			break;

#ifdef ANDROID
		case EVENT_JOYSTICK_BUTTON_DOWN:
		{
			int btn = event_joystick_get_button(event);
			int keycode = -1;
			if (menu->reorderitems) {
				if (btn == 0) {
					if (menu->reorder.grabbed)
						newmenu_reorder_drop(menu);
					else
						android_menu_reorder_start_button(&menu->reorder, timer_query());
					return 1;
				}
				if (btn == 1 && menu->reorder.grabbed) {
					newmenu_reorder_drop(menu);
					return 1;
				}
				if (menu->reorder.grabbed) {
					if (btn == 22)
						newmenu_reorder_move(menu, -1);
					else if (btn == 23)
						newmenu_reorder_move(menu, 1);
					if (btn >= 22 && btn <= 25)
						return 1;
				}
			}
			if (btn == 0)       keycode = KEY_ENTER;
			else if (btn == 1)  keycode = KEY_ESC;
			else if (btn == 22) keycode = KEY_UP;
			else if (btn == 23) keycode = KEY_DOWN;
			else if (btn == 24) keycode = KEY_LEFT;
			else if (btn == 25) keycode = KEY_RIGHT;
			if (keycode >= 0) {
				struct { event_type type; int keycode; } ke;
				ke.type = EVENT_KEY_COMMAND;
				ke.keycode = keycode;
				return newmenu_key_command(wind, (d_event *)&ke, menu);
			}
			break;
		}
		case EVENT_JOYSTICK_BUTTON_UP:
		{
			int btn = event_joystick_get_button(event);
			if (menu->reorderitems && btn == 0) {
				android_menu_reorder_stop_button(&menu->reorder);
				return 1;
			}
			break;
		}
		case EVENT_JOYSTICK_MOVED:
		{
			int axis, value;
			static int stick_dir[4]; /* LX, LY, RX, RY: -1/0/+1 */
			event_joystick_get_axis(event, &axis, &value);
			if (axis >= 0 && axis <= 3) {
				int dir = (value < -64) ? -1 : (value > 64) ? 1 : 0;
				if (dir != stick_dir[axis]) {
					stick_dir[axis] = dir;
					if (dir) {
						int keycode;
						if (menu->reorderitems && menu->reorder.grabbed) {
							if (axis == 1 || axis == 3)
								newmenu_reorder_move(menu, dir < 0 ? -1 : 1);
							return 1;
						}
						if (axis == 0 || axis == 2)
							keycode = (dir < 0) ? KEY_LEFT : KEY_RIGHT;
						else
							keycode = (dir < 0) ? KEY_UP : KEY_DOWN;
						struct { event_type type; int keycode; } ke;
						ke.type = EVENT_KEY_COMMAND;
						ke.keycode = keycode;
						return newmenu_key_command(wind, (d_event *)&ke, menu);
					}
				}
			}
			break;
		}
#endif

#ifdef ANDROID
		case EVENT_MOUSE_MOVED:
			if (menu->mouse_state && menu->reorderitems && menu->reorder.touch_candidate >= 0) {
				int mx, my, mz, target;
				grs_canvas *menu_canvas = window_get_canvas(wind);
				grs_canvas *save_canvas = grd_curcanv;
				gr_set_current_canvas(menu_canvas);

				mouse_get_pos(&mx, &my, &mz);
				if (!menu->reorder.grabbed)
					newmenu_reorder_poll(menu);
				if (menu->reorder.grabbed) {
					target = newmenu_reorder_item_at_pos(menu, mx, my);
					while (target >= 0 && menu->citem < target && newmenu_reorder_move(menu, 1))
						;
					while (target >= 0 && menu->citem > target && newmenu_reorder_move(menu, -1))
						;
					menu->drag_happened = 1;
				}

				gr_set_current_canvas(save_canvas);
				return 1;
			}
			// Drag-to-scroll for scrollable menus
			if (menu->mouse_state && menu->is_scroll_box && menu->drag_start_y >= 0) {
				int mx, my, mz;
				grs_canvas *menu_canvas = window_get_canvas(wind);
				grs_canvas *save_canvas = grd_curcanv;
				gr_set_current_canvas(menu_canvas);

				mouse_get_pos(&mx, &my, &mz);
				int ls = newmenu_get_scroll_line_spacing(menu);
				if (ls > 0) {
					int delta = my - menu->drag_start_y;
					int lines = delta / ls;
					if (lines != 0) {
						// Finger down = positive delta = scroll view up (decrease offset)
						// Finger up = negative delta = scroll view down (increase offset)
						int scroll_amount = -lines;
						int new_offset = menu->scroll_offset + scroll_amount;
						if (new_offset < 0)
							new_offset = 0;
						if (new_offset > menu->nitems - menu->max_on_menu)
							new_offset = menu->nitems - menu->max_on_menu;

						int actual = new_offset - menu->scroll_offset;
						if (actual != 0) {
							menu->scroll_offset = new_offset;
							menu->last_scroll_check = -1;

							// Move selection by the same amount
							if (!menu->all_text) {
								int new_citem = menu->citem + actual;
								if (new_citem < 0) new_citem = 0;
								if (new_citem >= menu->nitems) new_citem = menu->nitems - 1;
								// Skip NM_TYPE_TEXT items
								if (actual > 0) {
									while (new_citem < menu->nitems - 1 && menu->items[new_citem].type == NM_TYPE_TEXT)
										new_citem++;
								} else {
									while (new_citem > 0 && menu->items[new_citem].type == NM_TYPE_TEXT)
										new_citem--;
								}
								if (new_citem >= 0 && new_citem < menu->nitems && menu->items[new_citem].type != NM_TYPE_TEXT)
									menu->citem = new_citem;
							}

							menu->drag_happened = 1;
						}

						// Consume the distance that mapped to whole lines
						menu->drag_start_y += lines * ls;
					}
				}

				gr_set_current_canvas(save_canvas);
				return 1;
			}
			break;
#endif

		case EVENT_IDLE:
#ifdef ANDROID
			if (newmenu_reorder_poll(menu))
				return 1;
#endif
			timer_delay2(50);

			return newmenu_mouse(wind, event, menu, -1);
			break;

		case EVENT_WINDOW_DRAW:
		#ifdef ANDROID
			{
				int rval;
				extern int g_ogl_render_context;
				int prev_context = g_ogl_render_context;
				g_ogl_render_context = 0;
				rval = newmenu_draw(wind, menu);
				g_ogl_render_context = prev_context;
				return rval;
			}
		#else
			return newmenu_draw(wind, menu);
		#endif
			break;

		case EVENT_WINDOW_CLOSE:
#ifdef ANDROID
			{
				extern void android_hide_keyboard(void);
				android_hide_keyboard();
				android_menu_scale_clear();
				android_newmenu_free_wrapped_items(menu);
			}
#endif
			d_free(menu);
			break;

		default:
			break;
	}

	return 0;
}

newmenu *newmenu_do4( char * title, char * subtitle, int nitems, newmenu_item * item, int (*subfunction)(newmenu *menu, d_event *event, void *userdata), void *userdata, int citem, char * filename, int TinyMode, int TabsFlag )
{
	window *wind = NULL;
	newmenu *menu;

	CALLOC(menu, newmenu, 1);

	if (!menu)
		return NULL;

	menu->citem = citem;
	menu->scroll_offset = 0;
	menu->last_scroll_check = -1;
	menu->all_text = 0;
	menu->is_scroll_box = 0;
	menu->max_on_menu = TinyMode ? MAXDISPLAYABLEITEMSTINY : MAXDISPLAYABLEITEMS;
	menu->dblclick_flag = 0;
	menu->drag_start_y = -1;
	menu->drag_happened = 0;
	menu->title = title;
	menu->subtitle = subtitle;
	menu->nitems = nitems;
	menu->subfunction = subfunction;
	menu->items = item;
	menu->filename = filename;
	menu->tiny_mode = TinyMode;
	menu->tabs_flag = TabsFlag;
	menu->reorderitems = 0; // will be set if needed
	menu->rval = NULL;		// Default to not returning a value - respond to EVENT_NEWMENU_SELECTED instead
	menu->userdata = userdata;
#ifdef ANDROID
	android_menu_reorder_init(&menu->reorder);
#endif

	newmenu_free_background();

	if (nitems < 1 )
	{
		d_free(menu);
		return NULL;
	}

	menu->max_displayable=nitems;

#ifdef ANDROID
	android_newmenu_expand_tiny_text(menu);
#endif

	//set_screen_mode(SCREEN_MENU);	//hafta set the screen mode here or fonts might get changed/freed up if screen res changes

	newmenu_create_structure(menu);

	// Create the basic window
	if (menu)
		wind = window_create(&grd_curscreen->sc_canvas, menu->x, menu->y, menu->w, menu->h, (int (*)(window *, d_event *, void *))newmenu_handler, menu);
	if (!wind)
	{
#ifdef ANDROID
		android_newmenu_free_wrapped_items(menu);
#endif
		d_free(menu);

		return NULL;
	}
	menu->wind = wind;

	return menu;
}


int nm_messagebox1( char *title, int (*subfunction)(newmenu *menu, d_event *event, void *userdata), void *userdata, int nchoices, ... )
{
	int i;
	char * format;
	va_list args;
	char *s;
	char nm_text[MESSAGEBOX_TEXT_SIZE];
	newmenu_item nm_message_items[5];

	va_start(args, nchoices );

	Assert( nchoices <= 5 );

	for (i=0; i<nchoices; i++ )	{
		s = va_arg( args, char * );
		nm_message_items[i].type = NM_TYPE_MENU; nm_message_items[i].text = s;
	}
	format = va_arg( args, char * );
	strcpy( nm_text, "" );
	vsprintf(nm_text,format,args);
	va_end(args);

	Assert(strlen(nm_text) < MESSAGEBOX_TEXT_SIZE);

	return newmenu_do( title, nm_text, nchoices, nm_message_items, subfunction, userdata );
}

int nm_messagebox( char *title, int nchoices, ... )
{
	int i;
	char * format;
	va_list args;
	char *s;
	char nm_text[MESSAGEBOX_TEXT_SIZE];
	newmenu_item nm_message_items[5];

	va_start(args, nchoices );

	Assert( nchoices <= 5 );

	for (i=0; i<nchoices; i++ )	{
		s = va_arg( args, char * );
		nm_message_items[i].type = NM_TYPE_MENU; nm_message_items[i].text = s;
	}
	format = va_arg( args, char * );
	strcpy( nm_text, "" );
	vsprintf(nm_text,format,args);
	va_end(args);

	Assert(strlen(nm_text) < MESSAGEBOX_TEXT_SIZE );

	return newmenu_do( title, nm_text, nchoices, nm_message_items, NULL, NULL );
}

// Example listbox callback function...
// int lb_callback( int * citem, int *nitems, char * items[], int *keypress )
// {
// 	int i;
//
// 	if ( *keypress = KEY_CTRLED+KEY_D )	{
// 		if ( *nitems > 1 )	{
// 			PHYSFS_delete(items[*citem]);     // Delete the file
// 			for (i=*citem; i<*nitems-1; i++ )	{
// 				items[i] = items[i+1];
// 			}
// 			*nitems = *nitems - 1;
// 			d_free( items[*nitems] );
// 			items[*nitems] = NULL;
// 			return 1;	// redraw;
// 		}
//			*keypress = 0;
// 	}
// 	return 0;
// }

struct listbox
{
	window *wind;
	char *title;
	int nitems;
	char **item;
	int allow_abort_flag;
	int (*listbox_callback)(listbox *lb, d_event *event, void *userdata);
	int citem, first_item;
	int marquee_maxchars, marquee_charpos, marquee_scrollback;
	fix64 marquee_lasttime; // to scroll text if string does not fit in box
	int box_w, height, box_x, box_y, title_height, line_spacing, row_height, selected_row_height;
	short swidth, sheight; float fntscalex, fntscaley; // with these we check if resolution or fonts have changed so listbox structure can be recreated
	int mouse_state;
	void *userdata;
};

char **listbox_get_items(listbox *lb)
{
	return lb->item;
}

int listbox_get_nitems(listbox *lb)
{
	return lb->nitems;
}

int listbox_get_citem(listbox *lb)
{
	return lb->citem;
}

window *listbox_get_window(listbox *lb)
{
	return lb->wind;
}

#ifdef INTROSPECT_ON
const char *listbox_get_title(listbox *lb)
{
	return lb->title;
}
#endif

void listbox_delete_item(listbox *lb, int item)
{
	int i;

	Assert(item >= 0);

	if (lb->nitems)
	{
		for (i=item; i<lb->nitems-1; i++ )
			lb->item[i] = lb->item[i+1];
		lb->nitems--;
		lb->item[lb->nitems] = NULL;

		if (lb->citem >= lb->nitems)
			lb->citem = lb->nitems ? lb->nitems - 1 : 0;
	}
}

void update_scroll_position(listbox *lb)
{
	if (lb->citem<0)
		lb->citem = 0;

	if (lb->citem>=lb->nitems)
		lb->citem = lb->nitems-1;

	if (lb->citem< lb->first_item)
		lb->first_item = lb->citem;

	if (lb->citem>=( lb->first_item+LB_ITEMS_ON_SCREEN))
		lb->first_item = lb->citem-LB_ITEMS_ON_SCREEN+1;

	if (lb->nitems <= LB_ITEMS_ON_SCREEN )
		lb->first_item = 0;

	if (lb->first_item>lb->nitems-LB_ITEMS_ON_SCREEN)
		lb->first_item = lb->nitems-LB_ITEMS_ON_SCREEN;
	if (lb->first_item < 0 ) lb->first_item = 0;
}

static void listbox_get_item_bounds(listbox *lb, int item_index,
	                              int *x1, int *y1, int *x2, int *y2)
{
	int line_spacing = lb->line_spacing > 0 ? lb->line_spacing : (int)LINE_SPACING;
	int visible_y = (item_index - lb->first_item) * line_spacing + lb->box_y;
	int row_height = item_index == lb->citem ? lb->selected_row_height : lb->row_height;

	*x1 = lb->box_x;
	*x2 = lb->box_x + lb->box_w;
	*y1 = visible_y - FSPACY(1);
	*y2 = *y1 + row_height;
}

#ifdef ANDROID
static int android_listbox_is_pilot_select(listbox *lb)
{
	return lb && lb->title && !strcmp(lb->title, TXT_SELECT_PILOT);
}

static void android_listbox_publish_scale_rect(listbox *lb)
{
	android_menu_scale_result menu_scale;
	int source_x, source_y, source_w, source_h;

	if (!lb)
		return;
	source_x = lb->box_x - BORDERX;
	source_y = lb->box_y - lb->title_height - BORDERY;
	source_w = lb->box_w + 2 * BORDERX;
	source_h = lb->height + lb->title_height + 2 * BORDERY;

	if (android_menu_scale_compute_cropped(source_x, source_y, source_w, source_h,
	                                      SWIDTH, SHEIGHT, BORDERX, BORDERY,
	                                      &menu_scale))
		android_menu_scale_publish(&menu_scale);
	else
		android_menu_scale_clear();
}

static int android_listbox_item_at_point(listbox *lb, int mx, int my)
{
	int i, x1, y1, x2, y2;
	int item = -1;
	int slop_y = FSPACY(3);

	if (!lb)
		return -1;
	for (i = lb->first_item; i < lb->first_item + LB_ITEMS_ON_SCREEN; i++) {
		if (i >= lb->nitems)
			break;
		listbox_get_item_bounds(lb, i, &x1, &y1, &x2, &y2);
		if ((mx >= x1 - BORDERX) && (mx <= x2 + BORDERX) &&
		    (my >= y1) && (my <= y2))
			item = i;
	}
	if (item >= 0)
		return item;
	for (i = lb->first_item; i < lb->first_item + LB_ITEMS_ON_SCREEN; i++) {
		if (i >= lb->nitems)
			break;
		listbox_get_item_bounds(lb, i, &x1, &y1, &x2, &y2);
		if ((mx >= x1 - BORDERX) && (mx <= x2 + BORDERX) &&
		    (my >= y1 - slop_y) && (my <= y2 + slop_y))
			item = i;
	}
	return item;
}

static void android_log_listbox_touch(listbox *lb, const char *phase,
                                      int mx, int my, int item)
{
	static int diag_count;
	android_menu_scale_result scale;
	int x1 = 0, y1 = 0, x2 = 0, y2 = 0;
	const char *text = "";

	if (!lb || diag_count >= 160)
		return;
	diag_count++;
	if (item >= 0 && item < lb->nitems) {
		listbox_get_item_bounds(lb, item, &x1, &y1, &x2, &y2);
		text = lb->item[item] ? lb->item[item] : "";
	}
	android_menu_scale_get_state(&scale);
	debug_log(DLOG_GAME,
	          "[listbox-touch] %s mx=%d my=%d item=%d citem=%d first=%d n=%d line=%d box=(%d,%d %dx%d) bounds=(%d,%d %dx%d) title='%s' text='%s' scale=%d src=(%d,%d %dx%d) dst=(%d,%d %dx%d)\n",
	          phase, mx, my, item, lb->citem, lb->first_item, lb->nitems,
	          lb->line_spacing, lb->box_x, lb->box_y, lb->box_w, lb->height,
	          x1, y1, x2 - x1, y2 - y1, lb->title ? lb->title : "", text, scale.active,
	          scale.src.x, scale.src.y, scale.src.w, scale.src.h,
	          scale.dst.x, scale.dst.y, scale.dst.w, scale.dst.h);
}
#endif

int listbox_mouse(window *wind, d_event *event, listbox *lb, int button)
{
	int i = -1, mx, my, mz, x1, x2, y1, y2;

	switch (button)
	{
		case MBTN_LEFT:
		{
			if (lb->mouse_state)
			{
				mouse_get_pos(&mx, &my, &mz);
#ifdef ANDROID
				if (android_listbox_is_pilot_select(lb)) {
					i = android_listbox_item_at_point(lb, mx, my);
					android_log_listbox_touch(lb, "down", mx, my, i);
					if (i >= 0) {
						lb->citem = i;
						android_pilot_listbox_mouse_down(lb, lb->title, lb->nitems, i);
						return 1;
					}
					android_pilot_listbox_hold_clear(lb);
					return 0;
				}
#endif
#ifdef ANDROID
				android_log_listbox_touch(lb, "down", mx, my,
				                          android_listbox_item_at_point(lb, mx, my));
#endif
				for (i=lb->first_item; i<lb->first_item+LB_ITEMS_ON_SCREEN; i++ )	{
					if (i >= lb->nitems)
						break;
					listbox_get_item_bounds(lb, i, &x1, &y1, &x2, &y2);
					if ( ((mx > x1) && (mx < x2)) && ((my > y1) && (my < y2)) ) {
						lb->citem = i;
#ifdef ANDROID
						android_pilot_listbox_mouse_down(lb, lb->title, lb->nitems, i);
#endif
						return 1;
					}
				}
#ifdef ANDROID
				android_pilot_listbox_hold_clear(lb);
#endif
			}
			else if (event->type == EVENT_MOUSE_BUTTON_UP)
			{
				if (lb->citem < 0)
					return 0;

				mouse_get_pos(&mx, &my, &mz);
#ifdef ANDROID
				if (android_listbox_is_pilot_select(lb)) {
					i = android_listbox_item_at_point(lb, mx, my);
					android_log_listbox_touch(lb, "up", mx, my, i);
					if (i != lb->citem) {
						android_pilot_listbox_hold_clear(lb);
						return 0;
					}
					x1 = x2 = y1 = y2 = 0;
				} else
#endif
				{
#ifdef ANDROID
					android_log_listbox_touch(lb, "up", mx, my,
					                          android_listbox_item_at_point(lb, mx, my));
#endif
					listbox_get_item_bounds(lb, lb->citem, &x1, &y1, &x2, &y2);
				}
				if (
#ifdef ANDROID
				    (android_listbox_is_pilot_select(lb) && i == lb->citem) ||
#endif
				    (((mx > x1) && (mx < x2)) && ((my > y1) && (my < y2))) )
				{
#ifdef ANDROID
					{
						int rval;
						rval = android_pilot_listbox_mouse_up(lb, lb->title,
						                                    lb->nitems, lb->citem,
						                                    lb->listbox_callback,
						                                    lb->userdata);
						if (rval)
							return rval;
					}
#endif
					// Tell callback, allow staying in menu
					event->type = EVENT_NEWMENU_SELECTED;
					if (lb->listbox_callback && (*lb->listbox_callback)(lb, event, lb->userdata))
						return 1;

					window_close(wind);
					return 1;
				}
#ifdef ANDROID
				android_pilot_listbox_hold_clear(lb);
#endif
			}
			break;
		}
		case MBTN_RIGHT:
		{
			if (lb->allow_abort_flag && lb->mouse_state) {
				lb->citem = -1;
				window_close(wind);
				return 1;
			}
			break;
		}
		case MBTN_Z_UP:
		{
			if (lb->mouse_state)
			{
				lb->citem--;
				update_scroll_position(lb);
			}
			break;
		}
		case MBTN_Z_DOWN:
		{
			if (lb->mouse_state)
			{
				lb->citem++;
				update_scroll_position(lb);
			}
			break;
		}
		default:
			break;
	}

	return 0;
}

int listbox_key_command(window *wind, d_event *event, listbox *lb)
{
	// Note: statics are required to be zero-initialized in C, so we don't need to do it explicitly
	static char ascii_buffer[PATH_MAX];
	static fix64 last_key_time;

	// Reset the buffer after 3 seconds of inactivity
	if (last_key_time + F3_0 < timer_query())
		ascii_buffer[0] = 0;
	last_key_time = timer_query();

	int key = event_key_get(event);
	int rval = 1;

	switch(key)	{
		case KEY_HOME:
		case KEY_PAD7:
			ascii_buffer[0] = 0;
			lb->citem = 0;
			break;
		case KEY_END:
		case KEY_PAD1:
			ascii_buffer[0] = 0;
			lb->citem = lb->nitems-1;
			break;
		case KEY_UP:
		case KEY_PAD8:
			ascii_buffer[0] = 0;
			lb->citem--;
			break;
		case KEY_DOWN:
		case KEY_PAD2:
			ascii_buffer[0] = 0;
			lb->citem++;
			break;
		case KEY_PAGEDOWN:
		case KEY_PAD3:
			ascii_buffer[0] = 0;
			lb->citem += LB_ITEMS_ON_SCREEN;
			break;
		case KEY_PAGEUP:
		case KEY_PAD9:
			ascii_buffer[0] = 0;
			lb->citem -= LB_ITEMS_ON_SCREEN;
			break;
		case KEY_ESC:
			ascii_buffer[0] = 0;
			if (lb->allow_abort_flag) {
				lb->citem = -1;
				window_close(wind);
				return 1;
			}
			break;
		case KEY_ENTER:
		case KEY_PADENTER:
			ascii_buffer[0] = 0;
			// Tell callback, allow staying in menu
			event->type = EVENT_NEWMENU_SELECTED;
			if (lb->listbox_callback && (*lb->listbox_callback)(lb, event, lb->userdata))
				return 1;

			window_close(wind);
			return 1;
			break;

		default:
		{
			int ascii = key_ascii();
			if (ascii < 255) {
				size_t len = strlen(ascii_buffer);
				if (len < SDL_arraysize(ascii_buffer) - 1) {
					ascii_buffer[len++] = ascii;
					ascii_buffer[len] = 0;

					// We're only going to need to search backward for the first letter
					for (int cc = len > 1 ? lb->citem : 0; cc < lb->nitems; cc++)
						if (d_strnicmp(lb->item[cc], ascii_buffer, strlen(ascii_buffer)) == 0) {
							lb->citem = cc;
							break;
						}
				}
			}
			else
				ascii_buffer[0] = 0;
			rval = 0;
		}
	}

	update_scroll_position(lb);

	return rval;
}

void listbox_create_structure( listbox *lb)
{
	int i = 0;
	int aw = 0;

	gr_set_current_canvas(NULL);

	gr_set_curfont(MEDIUM3_FONT);

	lb->box_w = 0;
	for (i=0; i<lb->nitems; i++ )	{
		int w, h, aw;
		gr_get_string_size( lb->item[i], &w, &h, &aw );
		if ( w > lb->box_w )
			lb->box_w = w+FSPACX(10);
	}
	lb->line_spacing = (int)LINE_SPACING;
	lb->height = lb->line_spacing * LB_ITEMS_ON_SCREEN;

	{
		int w, h, aw;
		gr_get_string_size( lb->title, &w, &h, &aw );
		if ( w > lb->box_w )
			lb->box_w = w;
		lb->title_height = h+FSPACY(5);
	}

	gr_set_curfont(MEDIUM1_FONT);
	gr_get_string_size("O", &i, &lb->row_height, &aw);
	gr_set_curfont(MEDIUM2_FONT);
	gr_get_string_size("O", &i, &lb->selected_row_height, &aw);
	if (lb->row_height < lb->line_spacing + FSPACY(1))
		lb->row_height = lb->line_spacing + FSPACY(1);
	if (lb->selected_row_height < lb->row_height)
		lb->selected_row_height = lb->row_height;
	gr_set_curfont(MEDIUM3_FONT);

	lb->marquee_maxchars = lb->marquee_charpos = lb->marquee_scrollback = lb->marquee_lasttime = 0;
	// The box is bigger than we can fit on the screen since at least one string is too long. Check how many chars we can fit on the screen (at least only - MEDIUM*_FONT is variable font!) so we can make a marquee-like effect.
	if (lb->box_w + (BORDERX*2) > SWIDTH)
	{
		int w = 0, h = 0, aw = 0;

		lb->box_w = SWIDTH - (BORDERX*2);
		gr_get_string_size("O", &w, &h, &aw);
		lb->marquee_maxchars = lb->box_w/w;
		lb->marquee_lasttime = timer_query();
	}

	lb->box_x = (grd_curcanv->cv_bitmap.bm_w-lb->box_w)/2;
	lb->box_y = (grd_curcanv->cv_bitmap.bm_h-(lb->height+lb->title_height))/2 + lb->title_height;
	if ( lb->box_y < lb->title_height )
		lb->box_y = lb->title_height;

	if ( lb->citem < 0 ) lb->citem = 0;
	if ( lb->citem >= lb->nitems ) lb->citem = 0;

	lb->first_item = 0;
	update_scroll_position(lb);

	lb->mouse_state = 0;	//dblclick_flag = 0;
#ifdef ANDROID
	android_pilot_listbox_hold_clear(lb);
	android_listbox_publish_scale_rect(lb);
#endif
	lb->swidth = SWIDTH;
	lb->sheight = SHEIGHT;
	lb->fntscalex = FNTScaleX;
	lb->fntscaley = FNTScaleY;
}

static void listbox_draw_contents(listbox *lb)
{
	int i;
	int line_spacing = lb->line_spacing > 0 ? lb->line_spacing : (int)LINE_SPACING;

	nm_draw_background( lb->box_x-BORDERX,lb->box_y-lb->title_height-BORDERY,lb->box_x+lb->box_w+BORDERX,lb->box_y+lb->height+BORDERY );
	gr_set_curfont(MEDIUM3_FONT);
	gr_string( 0x8000, lb->box_y - lb->title_height, lb->title );

	gr_setcolor( BM_XRGB( 0,0,0)  );
	for (i=lb->first_item; i<lb->first_item+LB_ITEMS_ON_SCREEN; i++ )	{
		int y = (i-lb->first_item)*line_spacing+lb->box_y;
		if ( i >= lb->nitems )	{
			gr_setcolor( BM_XRGB(5,5,5));
			gr_rect( lb->box_x + lb->box_w - FSPACX(1), y-FSPACY(1), lb->box_x + lb->box_w, y + line_spacing);
			gr_setcolor( BM_XRGB(2,2,2));
			gr_rect( lb->box_x - FSPACX(1), y - FSPACY(1), lb->box_x, y + line_spacing );
			gr_setcolor( BM_XRGB(0,0,0));
			gr_rect( lb->box_x, y - FSPACY(1), lb->box_x + lb->box_w - FSPACX(1), y + line_spacing);
		} else {
			gr_set_curfont(( i == lb->citem )?MEDIUM2_FONT:MEDIUM1_FONT);
			gr_setcolor( BM_XRGB(5,5,5));
			gr_rect( lb->box_x + lb->box_w - FSPACX(1), y-FSPACY(1), lb->box_x + lb->box_w, y + line_spacing);
			gr_setcolor( BM_XRGB(2,2,2));
			gr_rect( lb->box_x - FSPACX(1), y - FSPACY(1), lb->box_x, y + line_spacing );
			gr_setcolor( BM_XRGB(0,0,0));
			gr_rect( lb->box_x, y - FSPACY(1), lb->box_x + lb->box_w - FSPACX(1), y + line_spacing);

			if (lb->marquee_maxchars && strlen(lb->item[i]) > lb->marquee_maxchars)
			{
				char *shrtstr = d_malloc(lb->marquee_maxchars+1);
				static int prev_citem = -1;
				
				if (prev_citem != lb->citem)
				{
					lb->marquee_charpos = lb->marquee_scrollback = 0;
					lb->marquee_lasttime = timer_query();
					prev_citem = lb->citem;
				}

				memset(shrtstr, '\0', lb->marquee_maxchars+1);
				
				if (i == lb->citem)
				{
					if (lb->marquee_lasttime + (F1_0/3) < timer_query())
					{
						lb->marquee_charpos = lb->marquee_charpos+(lb->marquee_scrollback?-1:+1);
						lb->marquee_lasttime = timer_query();
					}
					if (lb->marquee_charpos < 0) // reached beginning of string -> scroll forward
					{
						lb->marquee_charpos = 0;
						lb->marquee_scrollback = 0;
					}
					if (lb->marquee_charpos + lb->marquee_maxchars - 1 > strlen(lb->item[i])) // reached end of string -> scroll backward
					{
						lb->marquee_charpos = strlen(lb->item[i]) - lb->marquee_maxchars + 1;
						lb->marquee_scrollback = 1;
					}
					snprintf(shrtstr, lb->marquee_maxchars, "%s", lb->item[i]+lb->marquee_charpos);
				}
				else
				{
					snprintf(shrtstr, lb->marquee_maxchars, "%s", lb->item[i]);
				}
				gr_string( lb->box_x+FSPACX(5), y, shrtstr );
				d_free(shrtstr);
			}
			else
			{
				gr_string( lb->box_x+FSPACX(5), y, lb->item[i]  );
			}
		}
	}

	{
		d_event event;

		event.type = EVENT_NEWMENU_DRAW;
		if ( lb->listbox_callback )
			(*lb->listbox_callback)(lb, &event, lb->userdata);
	}
}

#ifdef ANDROID
static void android_listbox_draw_scaled(listbox *lb,
                                        android_menu_scale_result *result)
{
	grs_bitmap source_bitmap;
	grs_canvas source_canvas;
	grs_canvas *save_canvas = grd_curcanv;
	android_menu_scale_draw_state draw_state;
	listbox lb_copy;
	float scale;

	if (!lb || !result || !result->active || result->dst.w <= 0 || result->dst.h <= 0)
		return;

	scale = result->scale;
	gr_init_bitmap_alloc(&source_bitmap, BM_LINEAR, 0, 0, result->dst.w,
	                     result->dst.h, result->dst.w);
	memset(source_bitmap.bm_data, 0, result->dst.w * result->dst.h);
	gr_init_canvas(&source_canvas, source_bitmap.bm_data, BM_LINEAR,
	               result->dst.w, result->dst.h);
	lb_copy = *lb;

	if (android_menu_scale_begin_scaled_draw(scale, &draw_state)) {
		lb_copy.box_x = android_menu_scale_round_coord(lb->box_x - result->src.x, scale);
		lb_copy.box_y = android_menu_scale_round_coord(lb->box_y - result->src.y, scale);
		lb_copy.box_w = android_menu_scale_round_coord(lb->box_w, scale);
		lb_copy.height = android_menu_scale_round_coord(lb->height, scale);
		lb_copy.title_height = android_menu_scale_round_coord(lb->title_height, scale);
		lb_copy.line_spacing = android_menu_scale_round_coord(lb->line_spacing, scale);
		lb_copy.row_height = android_menu_scale_round_coord(lb->row_height, scale);
		lb_copy.selected_row_height = android_menu_scale_round_coord(lb->selected_row_height, scale);

		gr_set_current_canvas(&source_canvas);
		listbox_draw_contents(&lb_copy);
		android_menu_scale_end_scaled_draw(&draw_state);

		result->direct_render = 1;
		result->render_w = result->dst.w;
		result->render_h = result->dst.h;
		result->render_scale = scale;
	}

	gr_set_current_canvas(save_canvas);
	android_menu_scale_blit_bitmap(&source_bitmap, result, 0);
	gr_set_current_canvas(save_canvas);
	gr_free_bitmap_data(&source_bitmap);
}
#endif

int listbox_draw(window *wind, listbox *lb)
{
	(void)wind;

	if (lb->swidth != SWIDTH || lb->sheight != SHEIGHT || lb->fntscalex != FNTScaleX || lb->fntscalex != FNTScaleY)
		listbox_create_structure ( lb );

#ifdef ANDROID
	{
		android_menu_scale_result menu_scale;
		int source_x = lb->box_x - BORDERX;
		int source_y = lb->box_y - lb->title_height - BORDERY;
		int source_w = lb->box_w + 2 * BORDERX;
		int source_h = lb->height + lb->title_height + 2 * BORDERY;

		if (android_menu_scale_compute_cropped(source_x, source_y, source_w, source_h,
		                                      SWIDTH, SHEIGHT, BORDERX, BORDERY,
		                                      &menu_scale)) {
			android_listbox_draw_scaled(lb, &menu_scale);
			android_menu_scale_publish(&menu_scale);
			return 1;
		} else {
			android_menu_scale_clear();
		}
	}
#endif

	gr_set_current_canvas(NULL);
	listbox_draw_contents(lb);

	return 1;
}

int listbox_handler(window *wind, d_event *event, listbox *lb)
{
	if (event->type == EVENT_WINDOW_CLOSED) {
	#ifdef ANDROID
		android_pilot_listbox_hold_clear(lb);
	#endif
		return 0;
	}

	if (lb->listbox_callback)
	{
		int rval = (*lb->listbox_callback)(lb, event, lb->userdata);
		if (rval)
			return 1;		// event handled
	}

	switch (event->type)
	{
		case EVENT_WINDOW_ACTIVATED:
			game_flush_inputs();
			event_toggle_focus(0);
			key_toggle_repeat(1);
			break;

		case EVENT_WINDOW_DEACTIVATED:
			//event_toggle_focus(1);	// No cursor recentering
			key_toggle_repeat(0);
			break;

		case EVENT_MOUSE_BUTTON_DOWN:
		case EVENT_MOUSE_BUTTON_UP:
		{
			int button = event_mouse_get_button(event);
			lb->mouse_state = event->type == EVENT_MOUSE_BUTTON_DOWN;
			return listbox_mouse(wind, event, lb, button);
		}

		case EVENT_KEY_COMMAND:
			return listbox_key_command(wind, event, lb);
			break;

#ifdef ANDROID
		case EVENT_JOYSTICK_BUTTON_DOWN:
		{
			int btn = event_joystick_get_button(event);
			int keycode = -1;
			if (btn == 0) {
				int rval = android_pilot_listbox_joy_button_down(
					lb, lb->title, lb->nitems, lb->citem, wind,
					ANDROID_LISTBOX_HOLD_JOY_A,
					listbox_key_command);
				if (rval)
					return rval;
				keycode = KEY_ENTER;
			}
			else if (btn == 1)  keycode = KEY_ESC;
			else if (btn == 22) keycode = KEY_UP;
			else if (btn == 23) keycode = KEY_DOWN;
			else if (btn == 24) keycode = KEY_LEFT;
			else if (btn == 25) keycode = KEY_RIGHT;
			if (keycode >= 0) {
				struct { event_type type; int keycode; } ke;
				ke.type = EVENT_KEY_COMMAND;
				ke.keycode = keycode;
				return listbox_key_command(wind, (d_event *)&ke, lb);
			}
			break;
		}
		case EVENT_JOYSTICK_BUTTON_UP:
		{
			int btn = event_joystick_get_button(event);
			if (btn == 0)
				return android_pilot_listbox_joy_button_up(
					lb, lb->title, lb->nitems, lb->citem, wind,
					ANDROID_LISTBOX_HOLD_JOY_A,
					listbox_key_command, lb->listbox_callback, lb->userdata);
			break;
		}
		case EVENT_JOYSTICK_MOVED:
		{
			int axis, value;
			static int stick_dir[4];
			event_joystick_get_axis(event, &axis, &value);
			if (axis >= 0 && axis <= 3) {
				int dir = (value < -64) ? -1 : (value > 64) ? 1 : 0;
				if (dir != stick_dir[axis]) {
					stick_dir[axis] = dir;
					if (dir) {
						int keycode;
						if (axis == 0 || axis == 2)
							keycode = (dir < 0) ? KEY_LEFT : KEY_RIGHT;
						else
							keycode = (dir < 0) ? KEY_UP : KEY_DOWN;
						struct { event_type type; int keycode; } ke;
						ke.type = EVENT_KEY_COMMAND;
						ke.keycode = keycode;
						return listbox_key_command(wind, (d_event *)&ke, lb);
					}
				}
			}
			break;
		}
#endif

		case EVENT_IDLE:
		#ifdef ANDROID
			{
				int rval = android_pilot_listbox_hold_poll(
					lb, lb->title, lb->nitems, lb->citem,
					lb->listbox_callback, lb->userdata);
				if (rval)
					return rval;
			}
		#endif
			timer_delay2(50);

			return listbox_mouse(wind, event, lb, -1);
			break;

		case EVENT_WINDOW_DRAW:
		#ifdef ANDROID
			{
				int rval;
				rval = android_pilot_listbox_hold_poll(
					lb, lb->title, lb->nitems, lb->citem,
					lb->listbox_callback, lb->userdata);
				if (rval)
					return rval;
				extern int g_ogl_render_context;
				int prev_context = g_ogl_render_context;
				g_ogl_render_context = 0;
				rval = listbox_draw(wind, lb);
				g_ogl_render_context = prev_context;
				return rval;
			}
		#else
			return listbox_draw(wind, lb);
		#endif
			break;

		case EVENT_WINDOW_CLOSE:
#ifdef ANDROID
			{
				android_menu_scale_clear();
			}
#endif
			d_free(lb);
			break;

		default:
			break;
	}

	return 0;
}

listbox *newmenu_listbox( char * title, int nitems, char * items[], int allow_abort_flag, int (*listbox_callback)(listbox *lb, d_event *event, void *userdata), void *userdata )
{
	return newmenu_listbox1( title, nitems, items, allow_abort_flag, 0, listbox_callback, userdata );
}

listbox *newmenu_listbox1( char * title, int nitems, char * items[], int allow_abort_flag, int default_item, int (*listbox_callback)(listbox *lb, d_event *event, void *userdata), void *userdata )
{
	listbox *lb;
	window *wind;

	CALLOC(lb, listbox, 1);

	if (!lb)
		return NULL;

	newmenu_free_background();

	lb->title = title;
	lb->nitems = nitems;
	lb->item = items;
	lb->citem = default_item;
	lb->allow_abort_flag = allow_abort_flag;
	lb->listbox_callback = listbox_callback;
	lb->userdata = userdata;

	set_screen_mode(SCREEN_MENU);	//hafta set the screen mode here or fonts might get changed/freed up if screen res changes
	
	listbox_create_structure(lb);

	wind = window_create(&grd_curscreen->sc_canvas, lb->box_x-BORDERX, lb->box_y-lb->title_height-BORDERY, lb->box_w+2*BORDERX, lb->height+lb->title_height+2*BORDERY, (int (*)(window *, d_event *, void *))listbox_handler, lb);
	if (!wind)
	{
		d_free(lb);

		return NULL;
	}
	lb->wind = wind;

	return lb;
}

//added on 10/14/98 by Victor Rachels to attempt a fixedwidth font messagebox
newmenu *nm_messagebox_fixedfont( char *title, int nchoices, ... )
{
	int i;
	char * format;
	va_list args;
	char *s;
	char nm_text[MESSAGEBOX_TEXT_SIZE];
	newmenu_item nm_message_items[5];

	va_start(args, nchoices );

	Assert( nchoices <= 5 );

	for (i=0; i<nchoices; i++ )	{
		s = va_arg( args, char * );
		nm_message_items[i].type = NM_TYPE_MENU; nm_message_items[i].text = s;
	}
	format = va_arg( args, char * );
	//sprintf(	  nm_text, "" ); // adb: ?
	vsprintf(nm_text,format,args);
	va_end(args);

	Assert(strlen(nm_text) < MESSAGEBOX_TEXT_SIZE );

        return newmenu_do_fixedfont( title, nm_text, nchoices, nm_message_items, NULL, NULL, 0, NULL );
}
//end this section addition - Victor Rachels
