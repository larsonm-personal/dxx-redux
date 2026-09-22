/* D1 resource interpretation and presentation; no D2 asset substitutions */

#include <limits.h>
#include <string.h>
#include "inferno.h"
#include "args.h"
#include "physfsx.h"
#include "u_mem.h"
#include "dxxerror.h"
#include "text.h"
#include "titles.h"
#include "songs.h"
#include "gr.h"
#include "game.h"
#include "gamefont.h"
#include "gamepal.h"
#include "newmenu.h"
#include "d1_in_d2.h"
#include "d1_in_d2_presentation.h"

#define D1_TEXT_COUNT 622
#define D1_TEXT_MIN 514

static char *D1_text;
static char *D1_strings[N_TEXT_STRINGS];
static char *D2_strings[N_TEXT_STRINGS];
static int D2_text_loaded;
static int Presentation_game;

/* Separate mount points keep same-named resources independent of root HOG order */
static const char *Presentation_mounts[] = { "__dxx_d1_presentation__.hog", "__dxx_d2_presentation__.hog" };

const char *d1_in_d2_presentation_prefix(void)
{
	const int d1 = d1_in_d2_use_d1_gameplay();
	const int index = d1 ? 0 : 1;
	const char *prefix = d1 ? "d1-original/" : "d2-original/";
	PHYSFS_file *file;
	if (!d1 && !D1_text)
		return ""; /* Preserve normal D2 resource overrides until a profile switch */
	if (!PHYSFS_getMountPoint(Presentation_mounts[index])) {
		file = PHYSFSX_openReadBuffered(d1 ? "descent.hog" :
			PHYSFSX_exists("descent2.hog", 1) ? "descent2.hog" : "d2demo.hog");
		if (!file)
			Error("Cannot open the %s presentation archive", d1 ? "D1" : "D2");
		if (!PHYSFS_mountHandle(file, Presentation_mounts[index], prefix, 1)) {
			PHYSFS_close(file);
			Error("Cannot mount the %s presentation archive", d1 ? "D1" : "D2");
		}
	}
	return prefix;
}

int d1_in_d2_presentation_resource(const char *name, char *path, size_t size)
{
	char source_name[PATH_MAX];
	const char *source, *base, *prefix = "";
	int length;
	snprintf(source_name, sizeof(source_name), "%s", name);
	PHYSFSEXT_locateCorrectCase(source_name);
	source = PHYSFS_getRealDir(source_name);
	base = source;
	if (source) {
		const char *p;
		for (p = source; *p; ++p)
			if (*p == '/' || *p == '\\')
				base = p + 1;
	}
	/* Only base archives are interchangeable names; custom content keeps precedence */
	if (!base || !d_stricmp(base, "descent.hog") ||
		!d_stricmp(base, "descent2.hog") || !d_stricmp(base, "d2demo.hog"))
		prefix = d1_in_d2_presentation_prefix();
	length = snprintf(path, size, "%s%s", prefix, name);
	if (length < 0 || (size_t)length >= size)
		Error("Presentation resource path is too long: %s", name);
	return PHYSFSX_exists(path, 1);
}

char *d1_in_d2_menu_resource(enum d1_menu_resource kind, const char *default_name)
{
	static char paths[3][PATH_MAX];
	static const char *low[] = { "menu.pcx", "stars.pcx", "scores.pcx" };
	static const char *high[] = { "menuh.pcx", "starsb.pcx", "scoresb.pcx" };
	const int d1 = d1_in_d2_use_d1_gameplay();
	if (kind == D1_MENU_PALETTE)
		return d1 ? D1_DEFAULT_PALETTE : (char *)default_name;
	if (!d1)
		d1_in_d2_presentation_resource(default_name, paths[kind], sizeof(paths[kind]));
	else if (!(SWIDTH >= 640 && SHEIGHT >= 480 &&
		d1_in_d2_presentation_resource(high[kind], paths[kind], sizeof(paths[kind]))))
		d1_in_d2_presentation_resource(low[kind], paths[kind], sizeof(paths[kind]));
	return paths[kind];
}

void d1_in_d2_note_d2_text_loaded(void)
{
	memcpy(D2_strings, Text_string, sizeof(D2_strings));
	D2_text_loaded = 1;
	Presentation_game = 2;
}

void d1_in_d2_activate_presentation(void)
{
	const int game = d1_in_d2_use_d1_gameplay() ? 1 : 2;
	extern int Gamefont_installed;
	/* Headless definitions readers need no text/font initialization */
	if (!Presentation_game || Presentation_game == game)
		return;
	if (game == 1 && D1_text)
		memcpy(Text_string, D1_strings, sizeof(D1_strings));
	else if (game == 2 && D2_text_loaded)
		memcpy(Text_string, D2_strings, sizeof(D2_strings));
	else
		load_text();
	Presentation_game = game;
	if (Gamefont_installed) {
		newmenu_free_background();
		gamefont_close();
		gamefont_init();
		gr_set_curfont(GAME_FONT);
	}
}

/* Symbolic correspondence in d1/main/text.h and d2/main/text.h
 * Missing D2-only strings use built-in text, never a D1 ordinal */
static int d1_text_index(int d2)
{
	static const struct { short first, last, offset; } ranges[] = {
		{0, 108, 0}, {114, 118, -5}, {124, 128, -10}, {134, 138, -15},
		{144, 168, -20}, {170, 173, -21}, {192, 376, -20},
		{379, 489, -20}, {490, 490, -18}, {491, 492, -21},
		{493, 565, -20}, {567, 640, -20}, {644, 644, -23}
	};
	unsigned i;
	/* D2's help message expects a program-name argument; D1's does not */
	if (d2 == 6)
		return -1;
	for (i = 0; i < sizeof(ranges) / sizeof(ranges[0]); ++i)
		if (d2 >= ranges[i].first && d2 <= ranges[i].last)
			return d2 + ranges[i].offset;
	return -1;
}

int d1_in_d2_free_text(void)
{
	const int owned_d1 = D1_text != NULL;
	const int owned_d2 = D2_text_loaded;
	int i;
	d_free(D1_text);
	/* Engine text.c retains ownership of its D2 buffer and separately allocated label */
	if (owned_d2)
		memcpy(Text_string, D2_strings, sizeof(D2_strings));
	else if (owned_d1)
		memset(Text_string, 0, sizeof(D1_strings));
	memset(D1_strings, 0, sizeof(D1_strings));
	memset(D2_strings, 0, sizeof(D2_strings));
	D2_text_loaded = Presentation_game = 0;
	for (i = 0; i < 2; ++i)
		if (PHYSFS_getMountPoint(Presentation_mounts[i]))
			PHYSFS_unmount(Presentation_mounts[i]);
	return owned_d1 && !owned_d2;
}

int d1_in_d2_read_text(const char *filename, int encoded)
{
	PHYSFS_file *file = PHYSFSX_openReadBuffered(filename);
	PHYSFS_sint64 length;
	char *data, *cursor, *end, *source[D1_TEXT_COUNT] = {0};
	int count = 0, i;

	if (!file)
		return 0;
	length = PHYSFS_fileLength(file);
	if (length <= 0 || length >= INT_MAX) {
		PHYSFS_close(file);
		return 0;
	}
	data = d_malloc((size_t)length + 1);
	if (!data) {
		PHYSFS_close(file);
		return 0;
	}
	if (PHYSFS_readBytes(file, data, length) != length) {
		PHYSFS_close(file);
		d_free(data);
		return 0;
	}
	PHYSFS_close(file);
	data[length] = 0;
	cursor = data;
	end = data + length;
	while (cursor < end && count < D1_TEXT_COUNT) {
		char *next = memchr(cursor, '\n', end - cursor);
		char *read, *write;
		if (next)
			*next++ = 0;
		else
			next = end;
		if (encoded)
			decode_text_line(cursor);
		source[count++] = cursor;
		for (read = write = cursor; *read; ++read) {
			if (*read == '\r' && !encoded)
				continue;
			if (*read == '\\') {
				++read;
				if (*read == 'n') *write++ = '\n';
				else if (*read == 't') *write++ = '\t';
				else if (*read == '\\') *write++ = '\\';
				else {
					d_free(data);
					return 0;
				}
			} else
				*write++ = *read;
		}
		*write = 0;
		cursor = next;
	}
	if (count < D1_TEXT_MIN) {
		d_free(data);
		return 0;
	}
	/* Native D1 shortens this label to fit the weapon box */
	if (!strcmp(source[116], "SPREADFIRE"))
		strcpy(source[116], "SPREAD");
	d_free(D1_text);
	D1_text = data;
	for (i = 0; i < N_TEXT_STRINGS; ++i) {
		const int index = d1_text_index(i);
		Text_string[i] = index >= 0 && index < count ? source[index] : NULL;
	}
	memcpy(D1_strings, Text_string, sizeof(D1_strings));
	Presentation_game = 1;
	return 1;
}

int d1_in_d2_load_text(void)
{
	char plain[PATH_MAX], encoded[PATH_MAX];
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	d1_in_d2_presentation_resource("descent.tex", plain, sizeof(plain));
	d1_in_d2_presentation_resource("descent.txb", encoded, sizeof(encoded));
	if (!d1_in_d2_read_text(GameArg.DbgAltTex ? GameArg.DbgAltTex : plain, 0) &&
		!d1_in_d2_read_text(encoded, 1))
		Error("Cannot load original D1 text from DESCENT.TEX or DESCENT.TXB");
	return 1;
}

int d1_in_d2_show_titles(void)
{
	char screen[PATH_MAX];
	extern int g_startup_title_song_requested;
#ifdef __ANDROID__
	extern volatile int g_intro_active;
	extern volatile int g_skip_intro_pref;
	extern volatile int g_intro_skip_applied;
#endif
	if (!d1_in_d2_use_d1_gameplay())
		return 0;
	if (GameArg.DbgNoRun)
		return 1;
	g_startup_title_song_requested = 1;
	songs_play_song(SONG_TITLE, 1);
#ifdef __ANDROID__
	g_intro_skip_applied = 0;
	g_intro_active = 1;
#define D1_TITLE_SKIP() do { if (g_skip_intro_pref) { g_intro_skip_applied = 1; goto done; } } while (0)
	D1_TITLE_SKIP();
#endif
	if (!d1_in_d2_presentation_resource("macplay.pcx", screen, sizeof(screen)) &&
		!d1_in_d2_presentation_resource("mplaycd.pcx", screen, sizeof(screen)))
		d1_in_d2_presentation_resource("iplogo1.pcx", screen, sizeof(screen));
	show_title_screen(screen, 1, 1);
#ifdef __ANDROID__
	D1_TITLE_SKIP();
#endif
	if (!(SWIDTH >= 640 && SHEIGHT >= 480 && d1_in_d2_presentation_resource("logoh.pcx", screen, sizeof(screen))))
		d1_in_d2_presentation_resource("logo.pcx", screen, sizeof(screen));
	show_title_screen(screen, 1, 1);
#ifdef __ANDROID__
	D1_TITLE_SKIP();
#endif
	if (!(SWIDTH >= 640 && SHEIGHT >= 480 && d1_in_d2_presentation_resource("descenth.pcx", screen, sizeof(screen))))
		d1_in_d2_presentation_resource("descent.pcx", screen, sizeof(screen));
	show_title_screen(screen, 1, 1);
#ifdef __ANDROID__
done:
	g_intro_active = 0;
#undef D1_TITLE_SKIP
#endif
	return 1;
}
