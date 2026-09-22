/*
 *
 * Routines to display title screens...
 *
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <limits.h>
#include "d1_in_d2.h"
#include "d1_in_d2_presentation.h"

#include "pstypes.h"
#include "titles.h"
#include "timer.h"
#include "key.h"
#include "gr.h"
#include "palette.h"
#include "iff.h"
#include "pcx.h"
#include "u_mem.h"
#include "joy.h"
#include "gamefont.h"
#include "dxxerror.h"
#include "polyobj.h"
#include "textures.h"
#include "screens.h"
#include "multi.h"
#include "player.h"
#include "digi.h"
#include "text.h"
#include "kmatrix.h"
#include "piggy.h"
#include "songs.h"
#include "newmenu.h"
#include "menu.h"
#include "mouse.h"
#include "console.h"
#include "args.h"

#ifdef OGL
#include "ogl_init.h"
#endif
#ifdef __ANDROID__
#include "android_crash_handler.h"
#include "android_screen_advance.h"
#include "coop/coop_briefing.h"
#endif

#define MAX_BRIEFING_COLORS     7
#define DEFAULT_BRIEFING_BKG		"brief03.pcx"



// added by Jan Bobrowski for variable-size menu screen
static int rescale_x(int x)
{
	return x * GWIDTH / 320;
}

static int rescale_y(int y)
{
	return y * GHEIGHT / 200;
}

/* Native D1 briefing session; private presentation implementation adapted from
 * d1/main/titles.c. D2 titles retains its own command and movie session */

typedef struct {
	char    bs_name[PATH_MAX];                //  filename, eg merc01.  Assumes .lbm suffix.
	sbyte   level_num;
	sbyte   message_num;
	short   text_ulx, text_uly;         //  upper left x,y of text window
	short   text_width, text_height;    //  width and height of text window
} briefing_screen;

#define BRIEFING_SECRET_NUM 31          //  This must correspond to the first secret level which must come at the end of the list.
#define BRIEFING_OFFSET_NUM 4           // This must correspond to the first level screen (ie, past the bald guy briefing screens)

#define	ENDING_LEVEL_NUM_OEMSHARE 0x7f
#define	ENDING_LEVEL_NUM_REGISTER 0x7e

static const briefing_screen Briefing_screens_full[] = {
	{ "brief01.pcx",   0,  1,  13, 140, 290,  59 },
	{ "brief02.pcx",   0,  2,  27,  34, 257, 177 },
	{ "brief03.pcx",   0,  3,  20,  22, 257, 177 },
	{ "brief02.pcx",   0,  4,  27,  34, 257, 177 },
	{ "moon01.pcx",    1,  5,  10,  10, 300, 170 }, // level 1
	{ "moon01.pcx",    2,  6,  10,  10, 300, 170 }, // level 2
	{ "moon01.pcx",    3,  7,  10,  10, 300, 170 }, // level 3
	{ "venus01.pcx",   4,  8,  15, 15, 300,  200 }, // level 4
	{ "venus01.pcx",   5,  9,  15, 15, 300,  200 }, // level 5
	{ "brief03.pcx",   6, 10,  20,  22, 257, 177 },
	{ "merc01.pcx",    6, 11,  10, 15, 300, 200 },  // level 6
	{ "merc01.pcx",    7, 12,  10, 15, 300, 200 },  // level 7
	{ "brief03.pcx",   8, 13,  20,  22, 257, 177 },
	{ "mars01.pcx",    8, 14,  10, 100, 300,  200 }, // level 8
	{ "mars01.pcx",    9, 15,  10, 100, 300,  200 }, // level 9
	{ "brief03.pcx",  10, 16,  20,  22, 257, 177 },
	{ "mars01.pcx",   10, 17,  10, 100, 300,  200 }, // level 10
	{ "jup01.pcx",    11, 18,  10, 40, 300,  200 }, // level 11
	{ "jup01.pcx",    12, 19,  10, 40, 300,  200 }, // level 12
	{ "brief03.pcx",  13, 20,  20,  22, 257, 177 },
	{ "jup01.pcx",    13, 21,  10, 40, 300,  200 }, // level 13
	{ "jup01.pcx",    14, 22,  10, 40, 300,  200 }, // level 14
	{ "saturn01.pcx", 15, 23,  10, 40, 300,  200 }, // level 15
	{ "brief03.pcx",  16, 24,  20,  22, 257, 177 },
	{ "saturn01.pcx", 16, 25,  10, 40, 300,  200 }, // level 16
	{ "brief03.pcx",  17, 26,  20,  22, 257, 177 },
	{ "saturn01.pcx", 17, 27,  10, 40, 300,  200 }, // level 17
	{ "uranus01.pcx", 18, 28,  100, 100, 300,  200 }, // level 18
	{ "uranus01.pcx", 19, 29,  100, 100, 300,  200 }, // level 19
	{ "uranus01.pcx", 20, 30,  100, 100, 300,  200 }, // level 20
	{ "uranus01.pcx", 21, 31,  100, 100, 300,  200 }, // level 21
	{ "neptun01.pcx", 22, 32,  10, 20, 300,  200 }, // level 22
	{ "neptun01.pcx", 23, 33,  10, 20, 300,  200 }, // level 23
	{ "neptun01.pcx", 24, 34,  10, 20, 300,  200 }, // level 24
	{ "pluto01.pcx",  25, 35,  10, 20, 300,  200 }, // level 25
	{ "pluto01.pcx",  26, 36,  10, 20, 300,  200 }, // level 26
	{ "pluto01.pcx",  27, 37,  10, 20, 300,  200 }, // level 27
	{ "aster01.pcx",  -1, 38,  10, 90, 300,  200 }, // secret level -1
	{ "aster01.pcx",  -2, 39,  10, 90, 300,  200 }, // secret level -2
	{ "aster01.pcx",  -3, 40,  10, 90, 300,  200 }, // secret level -3
	{ "end01.pcx",   ENDING_LEVEL_NUM_OEMSHARE,  1,  23, 40, 320, 200 },   //  OEM and shareware end
	{ "end02.pcx",   ENDING_LEVEL_NUM_REGISTER,  1,  5, 5, 300, 200 },    // registered end
	{ "end01.pcx",   ENDING_LEVEL_NUM_REGISTER,  2,  23, 40, 320, 200 },  // registered end
	{ "end03.pcx",   ENDING_LEVEL_NUM_REGISTER,  3,  5, 5, 300, 200 },    // registered end

};

static const briefing_screen Briefing_screens_share[] = {
	{ "brief01.pcx",   0,  1,  13, 140, 290,  59 },
	{ "brief02.pcx",   0,  2,  27,  34, 257, 177 },
	{ "brief03.pcx",   0,  3,  20,  22, 257, 177 },
	{ "brief02.pcx",   0,  4,  27,  34, 257, 177 },
	{ "moon01.pcx",    1,  5,  10,  10, 300, 170 }, // level 1
	{ "moon01.pcx",    2,  6,  10,  10, 300, 170 }, // level 2
	{ "moon01.pcx",    3,  7,  10,  10, 300, 170 }, // level 3
	{ "venus01.pcx",   4,  8,  15, 15, 300,  200 }, // level 4
	{ "venus01.pcx",   5,  9,  15, 15, 300,  200 }, // level 5
	{ "brief03.pcx",   6, 10,  20,  22, 257, 177 },
	{ "merc01.pcx",    6, 10,  10, 15, 300, 200 }, // level 6
	{ "merc01.pcx",    7, 11,  10, 15, 300, 200 }, // level 7
	{ "end01.pcx",   ENDING_LEVEL_NUM_OEMSHARE,  1,  23, 40, 320, 200 }, // shareware end
};

#define Briefing_screens (IS_D1_SHAREWARE_MISSION_HOGSIZE(PHYSFSX_fsize("descent.hog"))?Briefing_screens_share:Briefing_screens_full)
#define	MAX_BRIEFING_SCREEN (IS_D1_SHAREWARE_MISSION_HOGSIZE(PHYSFSX_fsize("descent.hog"))?(sizeof(Briefing_screens_share) / sizeof(Briefing_screens_share[0])):(sizeof(Briefing_screens_full) / sizeof(Briefing_screens_full[0])))

typedef struct msgstream
{
	int x;
	int y;
	int color;
	int ch;
} __pack__ msgstream;

typedef struct briefing
{
	int colors[MAX_BRIEFING_COLORS], color, erase_color;
	short	level_num;
	short	cur_screen;
	briefing_screen	*screen;
	grs_bitmap background;
	char	background_name[PATH_MAX];
	char	*text;
	char	*message;
	int		text_x, text_y;
	msgstream messagestream[2048];
	int		streamcount;
	short	tab_stop;
	ubyte	flashing_cursor;
	ubyte	new_page;
	int		new_screen;
	int		failed;
	fix64		start_time;
	fix64		delay_count;
	int		robot_num;
	grs_canvas	*robot_canv;
	vms_angvec	robot_angles;
	char    bitmap_name[32];
	grs_bitmap  guy_bitmap;
	sbyte	guy_bitmap_show;
	sbyte   door_dir, door_div_count, animating_bitmap_type;
	sbyte	prev_ch;
#ifdef __ANDROID__
	int coop_authored_page_done;
#endif
} briefing;

static void briefing_init(briefing *br, short level_num)
{
#ifdef __ANDROID__
	br->coop_authored_page_done = 0;
#endif
	br->level_num = level_num;
	if (br->level_num == 1)
		br->level_num = 0;	// for start of game stuff

	br->cur_screen = 0;
	br->screen = NULL;
	gr_init_bitmap_data (&br->background);
	strncpy(br->background_name, DEFAULT_BRIEFING_BKG, sizeof(br->background_name));
	br->robot_num = 0;
	br->robot_canv = NULL;
	br->robot_angles.p = br->robot_angles.b = br->robot_angles.h = 0;
	br->bitmap_name[0] = '\0';
	br->door_dir = 1;
	br->door_div_count = 0;
	br->animating_bitmap_type = 0;
}

//-----------------------------------------------------------------------------
//	Load Descent briefing text.
static int load_screen_text(char *filename, char **buf)
{
	PHYSFS_file *tfile;
	char resource[PATH_MAX];
	int len, have_binary = 0;
	char *ext;

	if ((ext = strrchr(filename, '.')) == NULL)
		return (0);
	if (!d_stricmp(ext, ".txb"))
		have_binary = 1;

	d1_in_d2_presentation_resource(filename, resource, sizeof(resource));
	if ((tfile = PHYSFSX_openReadBuffered(resource)) == NULL)
		return (0);

	const PHYSFS_sint64 length = PHYSFS_fileLength(tfile);
	if (length < 0 || length >= INT_MAX) { PHYSFS_close(tfile); return 0; }
	len = (int)length;
	MALLOC(*buf, char, len+1);
	if (!*buf || PHYSFS_readBytes(tfile, *buf, len) != len) {
		PHYSFS_close(tfile);
		d_free(*buf);
		return 0;
	}
	PHYSFS_close(tfile);

	if (have_binary)
		decode_text(*buf, len);

	*(*buf+len)='\0';

	return (1);
}

static int get_message_num(char **message)
{
	int	num=0;

	while (strlen(*message) > 0 && **message == ' ')
		(*message)++;

	while (strlen(*message) > 0 && (**message >= '0') && (**message <= '9')) {
		const int digit = **message - '0';
		num = num > (INT_MAX - digit) / 10 ? INT_MAX : 10 * num + digit;
		(*message)++;
	}

	while (strlen(*message) > 0 && *(*message)++ != 10)		//	Get and drop eoln
		;

	return num;
}

static void get_message_name(char **message, char *result, size_t capacity)
{
	while (strlen(*message) > 0 && **message == ' ')
		(*message)++;

	while (strlen(*message) > 0 && (**message != ' ') && (**message != 10)) {
		if (**message != '\n' && capacity > 1) {
			*result++ = **message;
			--capacity;
		}
		(*message)++;
	}

	if (**message != 10)
		while (strlen(*message) > 0 && *(*message)++ != 10)		//	Get and drop eoln
			;

	*result = 0;
}

// Return a pointer to the start of text for screen #screen_num.
static char * get_briefing_message(briefing *br, int screen_num)
{
	char	*tptr = br->text;
	int	cur_screen=0;
	int	ch;

	Assert(screen_num >= 0);

	while ( (*tptr != 0 ) && (screen_num != cur_screen)) {
		ch = *tptr++;
		if (ch == '$' && *tptr) {
			ch = *tptr++;
			if (ch == 'S')
				cur_screen = get_message_num(&tptr);
		}
	}

	if (screen_num!=cur_screen)
		return (NULL);

	return tptr;
}

static void init_char_pos(briefing *br, int x, int y)
{
	br->text_x = x;
	br->text_y = y;
}

// Make sure the text stays on the screen
// Return 1 if new page required
// 0 otherwise
static int check_text_pos(briefing *br)
{
	if (br->text_x > br->screen->text_ulx + br->screen->text_width)
	{
		br->text_x = br->screen->text_ulx;
		br->text_y += br->screen->text_uly;
	}

	if (br->text_y > br->screen->text_uly + br->screen->text_height)
	{
		br->new_page = 1;
		return 1;
	}

	return 0;
}

static void put_char_delay(briefing *br, int ch)
{
	char str[2];
	int	w, h, aw;

	str[0] = ch; str[1] = '\0';
	if (br->delay_count && (timer_query() < br->start_time + br->delay_count))
	{
		br->message--;		// Go back to same character
		return;
	}

	if (br->streamcount >= (int)(sizeof(br->messagestream) / sizeof(br->messagestream[0]))) {
		--br->message;
		br->new_page = 1;
		return;
	}
	br->messagestream[br->streamcount].x = br->text_x;
	br->messagestream[br->streamcount].y = br->text_y;
	br->messagestream[br->streamcount].color = br->colors[br->color];
	br->messagestream[br->streamcount].ch = ch;
	br->streamcount++;

	br->prev_ch = ch;
	gr_get_string_size(str, &w, &h, &aw );
	br->text_x += w;

	br->start_time = timer_query();
}

static void init_spinning_robot(briefing *br);
static int load_briefing_screen(briefing *br, const char *fname);

// Process a character for the briefing,
// including special characters preceded by a '$'.
// Return 1 when page is finished, 0 otherwise
static int briefing_process_char(briefing *br)
{
	int	ch;

	gr_set_curfont( GAME_FONT );

	if (!*br->message) { br->new_screen = 1; return 1; }
	ch = *br->message++;
	if (ch == '$') {
		if (!*br->message) { br->new_screen = 1; return 1; }
		ch = *br->message++;
		if (ch == 'C') {
			br->color = get_message_num(&br->message)-1;
			if (br->color < 0)
				br->color = 0;
			else if (br->color > MAX_BRIEFING_COLORS-1)
				br->color = MAX_BRIEFING_COLORS-1;
			br->prev_ch = 10;
		} else if (ch == 'F') {     // toggle flashing cursor
			br->flashing_cursor = !br->flashing_cursor;
			br->prev_ch = 10;
			while (*br->message && *br->message != 10) ++br->message;
			if (*br->message) ++br->message;
		} else if (ch == 'T') {
			br->tab_stop = get_message_num(&br->message);
			br->prev_ch = 10;							//	read to eoln
		} else if (ch == 'R') {
			if (br->robot_canv != NULL)
			{
				d_free(br->robot_canv);
				br->robot_canv=NULL;
			}

			init_spinning_robot(br);
			br->robot_num = get_message_num(&br->message);
			if (br->robot_num < 0 || br->robot_num >= N_robot_types) br->robot_num = -1;
#if 0 // NOTE: code we wanted to merge from D2. However it breaks the briefing screen of the "Spider" bot: swallows it's name  line
                        while (*br->message++ != 10)
                                ;
#endif
			br->prev_ch = 10;                           // read to eoln
		} else if (ch == 'N') {
			if (br->robot_canv != NULL)
			{
				d_free(br->robot_canv);
				br->robot_canv=NULL;
			}

			br->robot_num = -1;
			get_message_name(&br->message, br->bitmap_name, sizeof(br->bitmap_name) - 3);
			strcat(br->bitmap_name, "#0");
			br->animating_bitmap_type = 0;
			br->prev_ch = 10;
		} else if (ch == 'O') {
			if (br->robot_canv != NULL)
			{
				d_free(br->robot_canv);
				br->robot_canv=NULL;
			}

			br->robot_num = -1;
			get_message_name(&br->message, br->bitmap_name, sizeof(br->bitmap_name) - 3);
			strcat(br->bitmap_name, "#0");
			br->animating_bitmap_type = 1;
			br->prev_ch = 10;
		} else if (ch == 'B') {
			char		bitmap_name[32], resource[PATH_MAX];
			ubyte		temp_palette[768];
			int		iff_error;
			(void)iff_error;

			if (br->robot_canv != NULL)
			{
				d_free(br->robot_canv);
				br->robot_canv=NULL;
			}

			br->robot_num = -1;
			get_message_name(&br->message, bitmap_name, sizeof(bitmap_name) - 4);
			strcat(bitmap_name, ".bbm");
			gr_free_bitmap_data(&br->guy_bitmap);
			gr_init_bitmap_data (&br->guy_bitmap);
			d1_in_d2_presentation_resource(bitmap_name, resource, sizeof(resource));
			iff_error = iff_read_bitmap(resource, &br->guy_bitmap, BM_LINEAR, temp_palette);
			br->guy_bitmap_show = iff_error == IFF_NO_ERROR;
			if (iff_error != IFF_NO_ERROR) {
				br->failed = 1;
				return 1;
			}
			br->prev_ch = 10;
		} else if (ch == 'S') {
#ifdef __ANDROID__
			br->coop_authored_page_done = 1;
#endif
			br->new_screen = 1;
			return 1;
		} else if (ch == 'P') {
#ifdef __ANDROID__
			br->coop_authored_page_done = 1;
#endif		//	New page.
			br->new_page = 1;

			while (*br->message && *br->message != 10) {
				br->message++;	//	drop carriage return after special escape sequence
			}
			if (*br->message) br->message++;
			br->prev_ch = 10;

			return 1;
		} else if (ch == '$' || ch == ';') // Print a $/;
			put_char_delay(br, ch);
	} else if (ch == '\t') {		//	Tab
		if (br->text_x - br->screen->text_ulx < FSPACX(br->tab_stop))
			br->text_x = br->screen->text_ulx + FSPACX(br->tab_stop);
	} else if ((ch == ';') && (br->prev_ch == 10)) {
		while (*br->message && *br->message != 10) ++br->message;
		if (*br->message) ++br->message;
		br->prev_ch = 10;
	} else if (ch == '\\') {
		br->prev_ch = ch;
	} else if (ch == 10) {
		if (br->prev_ch != '\\') {
			br->prev_ch = ch;
			br->text_y += FSPACY(5)+FSPACY(5)*3/5;
			br->text_x = br->screen->text_ulx;
			if (br->text_y > br->screen->text_uly + br->screen->text_height) {
				if (!load_briefing_screen(br, Briefing_screens[br->cur_screen].bs_name))
					return 1;
				br->text_x = br->screen->text_ulx;
				br->text_y = br->screen->text_uly;
			}
		} else {
			if (ch == 13)		//Can this happen? Above says ch==10
				Int3();
			br->prev_ch = ch;
		}
	} else
		put_char_delay(br, ch);

	return 0;
}

static void set_briefing_fontcolor (briefing *br)
{
	br->colors[0] = gr_find_closest_color_current( 0, 40, 0);
	br->colors[1] = gr_find_closest_color_current( 40, 33, 35);
	br->colors[2] = gr_find_closest_color_current( 8, 31, 54);

	//green
	br->colors[0] = gr_find_closest_color_current( 0, 54, 0);
	//white
	br->colors[1] = gr_find_closest_color_current( 42, 38, 32);

	//Begin D1X addition
	//red
	br->colors[2] = gr_find_closest_color_current( 63, 0, 0);

	//blue
	br->colors[3] = gr_find_closest_color_current( 0, 0, 54);
	//gray
	br->colors[4] = gr_find_closest_color_current( 14, 14, 14);
	//yellow
	br->colors[5] = gr_find_closest_color_current( 54, 54, 0);
	//purple
	br->colors[6] = gr_find_closest_color_current( 0, 54, 54);
	//End D1X addition

	br->erase_color = gr_find_closest_color_current(0, 0, 0);
}

static void redraw_messagestream(briefing *br)
{
	msgstream *stream = br->messagestream;
	const int count = br->streamcount;
	char msgbuf[2];
	int i;

	for (i=0; i<count; i++) {
		msgbuf[0] = stream[i].ch;
		msgbuf[1] = 0;
		if (stream[i-1].color != stream[i].color)
			gr_set_fontcolor(stream[i].color,-1);
		gr_printf(stream[i].x+1,stream[i].y,"%s",msgbuf);
	}
}

static void flash_cursor(briefing *br, int cursor_flag)
{
	if (cursor_flag == 0)
		return;

	if ((timer_query() % (F1_0/2) ) > (F1_0/4))
		gr_set_fontcolor(br->colors[br->color], -1);
	else
		gr_set_fontcolor(br->erase_color, -1);

	gr_printf(br->text_x, br->text_y, "_" );
}

#define EXIT_DOOR_MAX   14
#define OTHER_THING_MAX 10      // Adam: This is the number of frames in your new animating thing.
#define DOOR_DIV_INIT   6

//-----------------------------------------------------------------------------
static void show_animated_bitmap(briefing *br)
{
	grs_canvas  *curcanv_save, *bitmap_canv=0;
	grs_bitmap	*bitmap_ptr;
#ifdef OGL
	float scale = 1.0;

	if (((float)SWIDTH/320) < ((float)SHEIGHT/200))
		scale = ((float)SWIDTH/320);
	else
		scale = ((float)SHEIGHT/200);
#endif

	// Only plot every nth frame.
	if (br->door_div_count) {
		if (br->bitmap_name[0] != 0) {
			bitmap_index bi;
			bi = piggy_find_bitmap(br->bitmap_name);
			bitmap_ptr = &GameBitmaps[bi.index];
			PIGGY_PAGE_IN( bi );
#ifdef OGL
			ogl_ubitmapm_cs(rescale_x(220), rescale_y(45),bitmap_ptr->bm_w*scale,bitmap_ptr->bm_h*scale,bitmap_ptr,255,F1_0);
#else
			gr_bitmapm(rescale_x(220), rescale_y(45), bitmap_ptr);
#endif
		}
		br->door_div_count--;
		return;
	}

	br->door_div_count = DOOR_DIV_INIT;

	if (br->bitmap_name[0] != 0) {
		char		*pound_signp;
		int		num, dig1, dig2;
		bitmap_index bi;

		switch (br->animating_bitmap_type) {
			case 0:		bitmap_canv = gr_create_sub_canvas(grd_curcanv, rescale_x(220), rescale_y(45), 64, 64);	break;
			case 1:		bitmap_canv = gr_create_sub_canvas(grd_curcanv, rescale_x(220), rescale_y(45), 94, 94);	break; // Adam: Change here for your new animating bitmap thing. 94, 94 are bitmap size.
			default:	Int3(); // Impossible, illegal value for br->animating_bitmap_type
		}

		curcanv_save = grd_curcanv;
		grd_curcanv = bitmap_canv;

		pound_signp = strchr(br->bitmap_name, '#');
		Assert(pound_signp != NULL);

		dig1 = *(pound_signp+1);
		dig2 = *(pound_signp+2);
		if (dig2 == 0)
			num = dig1-'0';
		else
			num = (dig1-'0')*10 + (dig2-'0');

		switch (br->animating_bitmap_type) {
			case 0:
				num += br->door_dir;
				if (num > EXIT_DOOR_MAX) {
					num = EXIT_DOOR_MAX;
					br->door_dir = -1;
				} else if (num < 0) {
					num = 0;
					br->door_dir = 1;
				}
				break;
			case 1:
				num++;
				if (num > OTHER_THING_MAX)
					num = 0;
				break;
		}

		Assert(num < 100);
		if (num >= 10) {
			*(pound_signp+1) = (num / 10) + '0';
			*(pound_signp+2) = (num % 10) + '0';
			*(pound_signp+3) = 0;
		} else {
			*(pound_signp+1) = (num % 10) + '0';
			*(pound_signp+2) = 0;
		}

		bi = piggy_find_bitmap(br->bitmap_name);
		bitmap_ptr = &GameBitmaps[bi.index];
		PIGGY_PAGE_IN( bi );
#ifdef OGL
		ogl_ubitmapm_cs(0,0,bitmap_ptr->bm_w*scale,bitmap_ptr->bm_h*scale,bitmap_ptr,255,F1_0);
#else
		gr_bitmapm(0, 0, bitmap_ptr);
#endif
		grd_curcanv = curcanv_save;
		d_free(bitmap_canv);

		switch (br->animating_bitmap_type) {
			case 0:
				if (num == EXIT_DOOR_MAX) {
					br->door_dir = -1;
					br->door_div_count = 64;
				} else if (num == 0) {
					br->door_dir = 1;
					br->door_div_count = 64;
				}
				break;
			case 1:
				break;
		}
	}
}

//-----------------------------------------------------------------------------
static void show_briefing_bitmap(grs_bitmap *bmp)
{
	grs_canvas	*curcanv_save, *bitmap_canv;
#ifdef OGL
	float scale = 1.0;
#endif

	bitmap_canv = gr_create_sub_canvas(grd_curcanv, rescale_x(220), rescale_y(55), (bmp->bm_w*(SWIDTH/(HIRESMODE ? 640 : 320))),(bmp->bm_h*(SHEIGHT/(HIRESMODE ? 480 : 200))));
	curcanv_save = grd_curcanv;
	gr_set_current_canvas(bitmap_canv);

#ifdef OGL
	if (((float)SWIDTH/(HIRESMODE ? 640 : 320)) < ((float)SHEIGHT/(HIRESMODE ? 480 : 200)))
		scale = ((float)SWIDTH/(HIRESMODE ? 640 : 320));
	else
		scale = ((float)SHEIGHT/(HIRESMODE ? 480 : 200));

	ogl_ubitmapm_cs(0,0,bmp->bm_w*scale,bmp->bm_h*scale,bmp,255,F1_0);
#else
	gr_bitmapm(0, 0, bmp);
#endif
	gr_set_current_canvas(curcanv_save);

	d_free(bitmap_canv);
}

//-----------------------------------------------------------------------------
static void init_spinning_robot(briefing *br) //(int x,int y,int w,int h)
{
	int x = rescale_x(138);
	int y = rescale_y(55);
	int w = rescale_x(166);
	int h = rescale_y(138);

	br->robot_canv = gr_create_sub_canvas(grd_curcanv, x, y, w, h);
}

static void show_spinning_robot_frame(briefing *br, int robot_num)
{
	grs_canvas	*curcanv_save;

	if (robot_num != -1 && br->robot_canv) {
		br->robot_angles.p = br->robot_angles.b = 0;
		br->robot_angles.h += 150;

		curcanv_save = grd_curcanv;
		grd_curcanv = br->robot_canv;
		Assert(Robot_info[robot_num].model_num != -1);
		draw_model_picture(Robot_info[robot_num].model_num, &br->robot_angles);
		grd_curcanv = curcanv_save;
	}

}

//-----------------------------------------------------------------------------
#define KEY_DELAY_DEFAULT       ((F1_0*20)/1000)

static void init_new_page(briefing *br)
{
#ifdef __ANDROID__
	if (br->coop_authored_page_done) coop_briefing_step_complete(1);
	br->coop_authored_page_done = 0;
#endif
	br->new_page = 0;
	br->robot_num = -1;

	if (!load_briefing_screen(br, br->background_name))
		return;
	br->text_x = br->screen->text_ulx;
	br->text_y = br->screen->text_uly;

	br->streamcount=0;
	if (br->guy_bitmap_show) {
		gr_free_bitmap_data (&br->guy_bitmap);
		br->guy_bitmap_show=0;
	}

	br->start_time = 0;
	br->delay_count = KEY_DELAY_DEFAULT;
}

//	-----------------------------------------------------------------------------

#define NEW_END_GUY1	1
#define NEW_END_GUY2	3

static void free_briefing_screen(briefing *br);
extern void swap_0_255(grs_bitmap *bmp);

//	loads a briefing screen
static int load_briefing_screen(briefing *br, const char *fname)
{
	int pcx_error;
	char fname2[PATH_MAX], resource[PATH_MAX];

	free_briefing_screen(br);
	snprintf(fname2, sizeof(fname2), "%s", fname);
	d1_in_d2_presentation_resource(fname2, resource, sizeof(resource));
	/* Original hires art may replace base screens, never a mission's own art */
	if (SWIDTH >= 640 && SHEIGHT >= 480 && !strncmp(resource, "d1-original/", 12)) {
		char high[PATH_MAX];
		snprintf(high, sizeof(high), "%s", fname);
		char *ext = strrchr(high, '.');
		if (ext && (size_t)(ext - high) + 6 < sizeof(high)) {
			strcpy(ext, "h.pcx");
			if (d1_in_d2_presentation_resource(high, resource, sizeof(resource)))
				snprintf(fname2, sizeof(fname2), "%s", high);
		}
	}
	d1_in_d2_presentation_resource(fname2, resource, sizeof(resource));

	gr_init_bitmap_data(&br->background);
	if (d_stricmp(br->background_name, fname2))
		strncpy (br->background_name,fname2, sizeof(br->background_name));


	if ((pcx_error = pcx_read_bitmap(resource, &br->background, BM_LINEAR, gr_palette))!=PCX_ERROR_NONE)
	{
		con_printf(CON_URGENT, "Cannot load D1 briefing screen %s: %s (%i)\n", fname2, pcx_errormsg(pcx_error), pcx_error);
		br->failed = 1;
		return 0;
	}

	// Hack: Make sure black parts of robot are shown black
	d1_in_d2_asset_stats assets;
	d1_in_d2_get_stats(&assets);
	if ((assets.robot_pig_size == D1_MAC_PIGSIZE || assets.robot_pig_size == D1_MAC_SHARE_PIGSIZE) && gr_palette[0] == 63 &&
		(!d_stricmp(fname2, "brief03.pcx") || !d_stricmp(fname2, "end01.pcx") ||
		!d_stricmp(fname2, "brief03h.pcx") || !d_stricmp(fname2, "end01h.pcx")
		))
	{
		swap_0_255(&br->background);
		gr_palette[0] = gr_palette[1] = gr_palette[2] = 0;
		gr_palette[765] = gr_palette[766] = gr_palette[767] = 63;
	}

	show_fullscr(&br->background);

	gr_palette_load(gr_palette);

	set_briefing_fontcolor(br);

	MALLOC(br->screen, briefing_screen, 1);
	if (!br->screen) {
		br->failed = 1;
		return 0;
	}

	memcpy(br->screen, &Briefing_screens[br->cur_screen], sizeof(briefing_screen));
	br->screen->text_ulx = rescale_x(br->screen->text_ulx);
	br->screen->text_uly = rescale_y(br->screen->text_uly);
	br->screen->text_width = rescale_x(br->screen->text_width);
	br->screen->text_height = rescale_y(br->screen->text_height);
	init_char_pos(br, br->screen->text_ulx, br->screen->text_uly);

	return 1;
}

static void free_briefing_screen(briefing *br)
{
	if (br->robot_canv != NULL)
		d_free(br->robot_canv);

	if (br->screen != NULL)
		d_free(br->screen);

	if (br->background.bm_data != NULL)
		gr_free_bitmap_data (&br->background);
}



static int new_briefing_screen(briefing *br, int first)
{
#ifdef __ANDROID__
	if (!first && br->coop_authored_page_done) coop_briefing_step_complete(1);
	br->coop_authored_page_done = 0;
#endif
	gr_free_bitmap_data(&br->guy_bitmap);
	br->guy_bitmap_show = 0;
	br->new_screen = 0;

	if (!first)
		br->cur_screen++;

	while ((br->cur_screen < MAX_BRIEFING_SCREEN) && (Briefing_screens[br->cur_screen].level_num != br->level_num))
	{
		br->cur_screen++;
		if ((br->cur_screen == MAX_BRIEFING_SCREEN) && (br->level_num == 0))
		{
			// Showed the pre-game briefing, now show level 1 briefing
			br->level_num++;
			br->cur_screen = 0;
		}
	}

	if (br->cur_screen == MAX_BRIEFING_SCREEN)
		return 0;		// finished

	if (!load_briefing_screen(br, Briefing_screens[br->cur_screen].bs_name))
		return 0;

	br->message = get_briefing_message(br, Briefing_screens[br->cur_screen].message_num);

	if (br->message==NULL)
		return 0;

	br->color = 0;
	br->streamcount = 0;
	br->tab_stop = 0;
	br->flashing_cursor = 0;
	br->new_page = 0;
	br->start_time = 0;
	br->delay_count = KEY_DELAY_DEFAULT;
	br->robot_num = -1;
	br->bitmap_name[0] = 0;
	br->guy_bitmap_show = 0;
	br->prev_ch = -1;

	return 1;
}

//-----------------------------------------------------------------------------
static int briefing_handler(window *wind, d_event *event, briefing *br)
{
	/* The window service sends CLOSED with a null data pointer */
	if (event->type == EVENT_WINDOW_CLOSED)
		return 0;
	if (br->failed && event->type == EVENT_WINDOW_DRAW) {
		window_close(wind);
		return 1;
	}
#ifdef ANDROID
	if (event->type != EVENT_WINDOW_CLOSE && event->type != EVENT_WINDOW_CLOSED &&
	    android_screen_advance_take_request(ANDROID_SCREEN_ADVANCE_BRIEFING)) {
		coop_briefing_skip();
		window_close(wind);
		return 1;
	}
#endif
	switch (event->type)
	{
		case EVENT_WINDOW_ACTIVATED:
		case EVENT_WINDOW_DEACTIVATED:
			key_flush();
			break;

		case EVENT_MOUSE_BUTTON_DOWN:
			if (event_mouse_get_button(event) == 0)
			{
#ifdef ANDROID
				if (!android_screen_advance_accept_event(ANDROID_SCREEN_ADVANCE_BRIEFING, event))
					return 1;
#endif
				if (br->new_screen)
				{
					if (!new_briefing_screen(br, 0))
					{
						window_close(wind);
						return 1;
					}
				}
				else if (br->new_page)
					init_new_page(br);
				else
					br->delay_count = 0;

				return 1;
			}
			break;

		case EVENT_KEY_COMMAND:
		{
			int key = event_key_get(event);

			switch (key)
			{
				case KEY_ESC:
#ifdef __ANDROID__
					coop_briefing_skip();
#endif
					window_close(wind);
					return 1;

				case KEY_SPACEBAR:
				case KEY_ENTER:
					br->delay_count = 0;
					// fall through

				default:
					if (call_default_handler(event))
						return 1;
					else if (br->new_screen)
					{
						if (!new_briefing_screen(br, 0))
						{
							window_close(wind);
							return 1;
						}
					}
					else if (br->new_page)
						init_new_page(br);
					break;
			}
			break;
		}

#ifdef ANDROID
		case EVENT_JOYSTICK_BUTTON_DOWN:
		{
			int btn = event_joystick_get_button(event);
			if (btn == 0 &&
			    !android_screen_advance_accept_event(ANDROID_SCREEN_ADVANCE_BRIEFING, event))
				return 1;
			if (!android_screen_advance_can_activate(ANDROID_SCREEN_ADVANCE_BRIEFING))
				return 1;
			if (btn == 1) {
				coop_briefing_skip();
				window_close(wind);
				return 1;
			}
			/* Any other button (including A) advances the briefing */
			if (br->new_screen) {
				if (!new_briefing_screen(br, 0)) {
					window_close(wind);
					return 1;
				}
			} else if (br->new_page)
				init_new_page(br);
			else
				br->delay_count = 0;
			return 1;
		}
#endif

		case EVENT_WINDOW_DRAW:
			gr_set_current_canvas(NULL);

			timer_delay2(50);

			if (!(br->new_screen || br->new_page))
				while (!briefing_process_char(br) && !br->delay_count)
				{
					check_text_pos(br);
					if (br->new_page)
						break;
				}
			if (br->failed) {
				window_close(wind);
				return 1;
			}
			check_text_pos(br);

			if (br->background.bm_data)
				show_fullscr(&br->background);

			if (br->guy_bitmap_show)
				show_briefing_bitmap(&br->guy_bitmap);
			if (br->bitmap_name[0] != 0)
				show_animated_bitmap(br);
			if (br->robot_num != -1)
				show_spinning_robot_frame(br, br->robot_num);

			gr_set_curfont( GAME_FONT );

			gr_set_fontcolor(br->colors[br->color], -1);
			redraw_messagestream(br);

			if (br->new_page || br->new_screen)
				flash_cursor(br, br->flashing_cursor);
			else if (br->flashing_cursor)
				gr_printf(br->text_x, br->text_y, "_");
			break;

		case EVENT_WINDOW_CLOSE:
			free_briefing_screen(br);
			gr_free_bitmap_data(&br->guy_bitmap);
			d_free(br->text);
			d_free(br);
			break;

		default:
			break;
	}

	return 0;
}

static void run_briefing(char *filename, int level_num)
{
	briefing *br;
	window *wind;

	#ifdef __ANDROID__
	if (coop_briefing_cancelled() && !coop_briefing_planning()) return;
	#endif
	if (!filename || !*filename)
		return;

	MALLOC(br, briefing, 1);
	if (!br)
		return;

	memset(br, 0, sizeof(*br));
	briefing_init(br, level_num);

	if (!load_screen_text(filename, &br->text))
	{
		d_free(br);
		return;
	}

#ifdef __ANDROID__
	if (coop_briefing_planning()) {

        for (int i = 0; i < MAX_BRIEFING_SCREEN; ++i)
            if (Briefing_screens[i].level_num == level_num ||
                (level_num == 1 && Briefing_screens[i].level_num == 0))
                coop_briefing_plan_message(
                    get_briefing_message(br, Briefing_screens[i].message_num));
		d_free(br->text);
		d_free(br);
		return;
	}
	coop_briefing_step(0);
#endif
	wind = window_create(&grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT, (int (*)(window *, d_event *, void *))briefing_handler, br);
	if (!wind)
	{
		d_free(br->text);
		d_free(br);
		return;
	}

	if ((songs_is_playing() != SONG_BRIEFING) && (songs_is_playing() != SONG_ENDGAME))
		songs_play_song( SONG_BRIEFING, 1 );

	set_screen_mode( SCREEN_MENU );
	gr_set_current_canvas(NULL);

	if (!new_briefing_screen(br, 1))
	{
		window_close(wind);
		return;
	}

	// Stay where we are in the stack frame until briefing done
	// Too complicated otherwise
#ifdef ANDROID
	extern void game_flush_inputs(void);
	game_flush_inputs();
	android_screen_advance_begin(ANDROID_SCREEN_ADVANCE_BRIEFING, 1);
#endif
	while (window_exists(wind)) {
#ifdef __ANDROID__
		coop_briefing_pump();
		if (coop_briefing_cancelled()) {
			window_close(wind);
			break;
		}
#endif
		event_process();
	}
#ifdef ANDROID
	android_screen_advance_end(ANDROID_SCREEN_ADVANCE_BRIEFING);
#endif
}

int d1_in_d2_show_ending(char *filename)
{
	int level_num_screen = Current_level_num, showorder = 0;
	if (!d1_in_d2_use_d1_gameplay()) return 0;

	if (!filename || !*filename)
		return 1; // handled without an ending

	if (d_stricmp(filename, BIMD1_ENDING_FILE_OEM) == 0)
	{
		songs_play_song( SONG_ENDGAME, 1 );
		level_num_screen = ENDING_LEVEL_NUM_OEMSHARE;
		showorder = 1;
	}
	else if (d_stricmp(filename, BIMD1_ENDING_FILE_SHARE) == 0)
	{
		songs_play_song( SONG_BRIEFING, 1 );
		level_num_screen = ENDING_LEVEL_NUM_OEMSHARE;
		showorder = 1;
	}
	else
	{
		songs_play_song( SONG_ENDGAME, 1 );
		level_num_screen = ENDING_LEVEL_NUM_REGISTER;
	}

	run_briefing(filename, level_num_screen);
	if (showorder)
		d1_in_d2_show_order_form();
	return 1;
}

int d1_in_d2_show_briefing(char *filename, int level_num)
{
	if (!d1_in_d2_use_d1_gameplay()) return 0;
	run_briefing(filename, level_num);
	return 1;
}

int d1_in_d2_show_order_form(void)
{
	if (!d1_in_d2_use_d1_gameplay()) return 0;
	/* D2's order-form entry has no native D1 -notitles option */
	const char *names[] = { "warning.pcx", "apple.pcx", "order01.pcx" };
	char resource[PATH_MAX];
	for (unsigned i = 0; i < sizeof(names) / sizeof(names[0]); ++i) {
		if (d1_in_d2_presentation_resource(names[i], resource, sizeof(resource))) {
			show_title_screen(resource, 1, 0);
			break;
		}
	}
	return 1;
}
