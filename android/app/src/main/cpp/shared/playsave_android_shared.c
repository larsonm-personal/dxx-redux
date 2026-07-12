/* Shared Android playsave helper bodies for d1/main/playsave.c and d2/main/playsave.c. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef __ANDROID__
#include <unistd.h>
#endif

#include "game.h"
#include "playsave.h"
#include "strutil.h"
#include "playsave_android_shared.h"
#include "playsave_transaction.h"

static const char *playsave_android_options_header(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return "[D2X Options]\n";
#else
	return "[D1X Options]\n";
#endif
}

void android_get_default_pilot_prefs(int *cockpit_mode, int *auto_leveling)
{
	if (cockpit_mode)
		*cockpit_mode = CM_FULL_COCKPIT;
	if (auto_leveling)
		*auto_leveling = 1;
}

void android_get_default_visual_prefs(int *alpha_effects, int *dynlight_color)
{
	if (alpha_effects)
		*alpha_effects = 0;
	if (dynlight_color)
		*dynlight_color = 0;
}

void android_get_default_hud_count_prefs(int *show_counts)
{
	if (show_counts)
		*show_counts = 0;
}

void android_get_default_music_prefs(int *source, int *prefer_mission, int *play_order, int *volume)
{
	if (source)
		*source = 2;
	if (prefer_mission)
		*prefer_mission = 1;
	if (play_order)
		*play_order = 0;
	if (volume)
		*volume = 8;
}

static int playsave_android_music_source_from_string(const char *value)
{
	if (!value)
		return 2;
	if (!d_strnicmp(value, "mission", 7))
		return 0;
	if (!d_strnicmp(value, "files", 5))
		return 1;
	if (!d_strnicmp(value, "cd", 2))
		return 2;
	if (!d_strnicmp(value, "midi", 4))
		return 3;
	return 2;
}

static const char *playsave_android_music_source_to_string(int source)
{
	switch (source) {
		case 0:
			return "mission";
		case 1:
			return "files";
		case 3:
			return "midi";
		case 2:
		default:
			return "cd";
	}
}

static int playsave_android_clamp_int(int value, int min_value, int max_value)
{
	if (value < min_value)
		return min_value;
	if (value > max_value)
		return max_value;
	return value;
}

int plx_read_music_prefs(const char *path, int *source, int *prefer_mission, int *play_order, int *volume)
{
	FILE *f = fopen(path, "r");
	char line[256];
	int in_music = 0;
	int found = 0;

	if (!f) return 0;

	while (fgets(line, sizeof(line), f)) {
		if (!in_music) {
			if (!d_strnicmp(line, "[music]", 7))
				in_music = 1;
			continue;
		}
		if (!d_strnicmp(line, "[end]", 5))
			break;
		if (!d_strnicmp(line, "source=", 7)) {
			if (source)
				*source = playsave_android_music_source_from_string(line + 7);
			found = 1;
			continue;
		}
		if (!d_strnicmp(line, "prefermission=", 14)) {
			if (prefer_mission)
				*prefer_mission = atoi(line + 14) ? 1 : 0;
			found = 1;
			continue;
		}
		if (!d_strnicmp(line, "playorder=", 10)) {
			if (play_order)
				*play_order = playsave_android_clamp_int(atoi(line + 10), 0, 2);
			found = 1;
			continue;
		}
		if (!d_strnicmp(line, "volume=", 7)) {
			if (volume)
				*volume = playsave_android_clamp_int(atoi(line + 7), 0, 8);
			found = 1;
		}
	}

	fclose(f);
	return found;
}

int plx_write_music_prefs(const char *path, int source, int prefer_mission, int play_order, int volume)
{
	FILE *f = fopen(path, "r");
	char buf[32768];
	int buf_len = 0;
	int in_music = 0;
	int found_music = 0;
	int wrote_music = 0;
	char tmp[128];

	source = playsave_android_clamp_int(source, 0, 3);
	play_order = playsave_android_clamp_int(play_order, 0, 2);
	volume = playsave_android_clamp_int(volume, 0, 8);

#define PLAYSAVE_BUF_APPEND(s)                             \
	do {                                                   \
		int playsave_slen = (int) strlen(s);               \
		if (buf_len + playsave_slen < (int) sizeof(buf)) { \
			memcpy(buf + buf_len, s, playsave_slen);       \
			buf_len += playsave_slen;                      \
		}                                                  \
	} while (0)

#define PLAYSAVE_APPEND_MUSIC()                                                                     \
	do {                                                                                            \
		PLAYSAVE_BUF_APPEND("[music]\n");                                                           \
		snprintf(tmp, sizeof(tmp), "source=%s\n", playsave_android_music_source_to_string(source)); \
		PLAYSAVE_BUF_APPEND(tmp);                                                                   \
		snprintf(tmp, sizeof(tmp), "prefermission=%i\n", prefer_mission ? 1 : 0);                   \
		PLAYSAVE_BUF_APPEND(tmp);                                                                   \
		snprintf(tmp, sizeof(tmp), "playorder=%i\n", play_order);                                   \
		PLAYSAVE_BUF_APPEND(tmp);                                                                   \
		snprintf(tmp, sizeof(tmp), "volume=%i\n", volume);                                          \
		PLAYSAVE_BUF_APPEND(tmp);                                                                   \
		PLAYSAVE_BUF_APPEND("[end]\n");                                                             \
		wrote_music = 1;                                                                            \
	} while (0)

	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			if (!in_music && !d_strnicmp(line, "[music]", 7)) {
				found_music = 1;
				in_music = 1;
				PLAYSAVE_APPEND_MUSIC();
				continue;
			}
			if (in_music) {
				if (!d_strnicmp(line, "[end]", 5))
					in_music = 0;
				continue;
			}
			PLAYSAVE_BUF_APPEND(line);
		}
		fclose(f);
	}

	if (!found_music) {
		if (buf_len == 0)
			PLAYSAVE_BUF_APPEND(playsave_android_options_header());
		PLAYSAVE_APPEND_MUSIC();
	}

#undef PLAYSAVE_BUF_APPEND
#undef PLAYSAVE_APPEND_MUSIC

	f = fopen(path, "w");
	if (!f) return 0;
	fwrite(buf, 1, buf_len, f);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	return wrote_music;
}

int plx_read_robot_hostage_counts(const char *path, int *show_counts)
{
	FILE *f = fopen(path, "r");
	char line[256];
	int in_cockpit = 0;

	if (!f) return 0;

	while (fgets(line, sizeof(line), f)) {
		if (!in_cockpit) {
			if (!d_strnicmp(line, "[cockpit]", 9))
				in_cockpit = 1;
			continue;
		}
		if (!d_strnicmp(line, "[end]", 5))
			break;
		if (!d_strnicmp(line, "robothostagecounts=", 19)) {
			if (show_counts)
				*show_counts = atoi(line + 19) ? 1 : 0;
			fclose(f);
			return 1;
		}
	}

	fclose(f);
	return 0;
}

int plx_write_robot_hostage_counts(const char *path, int show_counts)
{
	FILE *f = fopen(path, "r");
	char buf[32768];
	int buf_len = 0;
	int in_cockpit = 0;
	int found_cockpit = 0;
	int wrote_counts = 0;
	char tmp[64];

#define PLAYSAVE_BUF_APPEND(s)                             \
	do {                                                   \
		int playsave_slen = (int) strlen(s);               \
		if (buf_len + playsave_slen < (int) sizeof(buf)) { \
			memcpy(buf + buf_len, s, playsave_slen);       \
			buf_len += playsave_slen;                      \
		}                                                  \
	} while (0)

#define PLAYSAVE_APPEND_COUNTS()                                                    \
	do {                                                                            \
		snprintf(tmp, sizeof(tmp), "robothostagecounts=%i\n", show_counts ? 1 : 0); \
		PLAYSAVE_BUF_APPEND(tmp);                                                   \
		wrote_counts = 1;                                                           \
	} while (0)

	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			if (!in_cockpit && !d_strnicmp(line, "[cockpit]", 9)) {
				found_cockpit = 1;
				in_cockpit = 1;
				PLAYSAVE_BUF_APPEND(line);
				continue;
			}
			if (in_cockpit && !d_strnicmp(line, "[end]", 5)) {
				if (!wrote_counts)
					PLAYSAVE_APPEND_COUNTS();
				PLAYSAVE_BUF_APPEND(line);
				in_cockpit = 0;
				continue;
			}
			if (in_cockpit && !d_strnicmp(line, "robothostagecounts=", 19)) {
				PLAYSAVE_APPEND_COUNTS();
				continue;
			}
			PLAYSAVE_BUF_APPEND(line);
		}
		fclose(f);
	}

	if (in_cockpit) {
		if (!wrote_counts)
			PLAYSAVE_APPEND_COUNTS();
		PLAYSAVE_BUF_APPEND("[end]\n");
	}

	if (!found_cockpit) {
		if (buf_len == 0)
			PLAYSAVE_BUF_APPEND(playsave_android_options_header());
		PLAYSAVE_BUF_APPEND("[cockpit]\n");
		PLAYSAVE_APPEND_COUNTS();
		PLAYSAVE_BUF_APPEND("[end]\n");
	}

#undef PLAYSAVE_BUF_APPEND
#undef PLAYSAVE_APPEND_COUNTS

	f = fopen(path, "w");
	if (!f) return 0;
	fwrite(buf, 1, buf_len, f);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	return 1;
}

int plx_read_visual_prefs(const char *path, int *alpha_effects, int *dynlight_color)
{
	FILE *f = fopen(path, "r");
	char line[256];
	int in_graphics = 0;
	int found = 0;

	if (!f) return 0;

	while (fgets(line, sizeof(line), f)) {
		if (!in_graphics) {
			if (!d_strnicmp(line, "[graphics]", 10))
				in_graphics = 1;
			continue;
		}
		if (!d_strnicmp(line, "[end]", 5))
			break;
		if (!d_strnicmp(line, "alphaeffects=", 13)) {
			if (alpha_effects)
				*alpha_effects = atoi(line + 13) ? 1 : 0;
			found = 1;
			continue;
		}
		if (!d_strnicmp(line, "dynlightcolor=", 14)) {
			if (dynlight_color)
				*dynlight_color = atoi(line + 14) ? 1 : 0;
			found = 1;
		}
	}

	fclose(f);
	return found;
}

int plx_write_visual_prefs(const char *path, int alpha_effects, int dynlight_color)
{
	FILE *f = fopen(path, "r");
	char buf[32768];
	int buf_len = 0;
	int in_graphics = 0;
	int found_graphics = 0;
	int wrote_alpha = 0;
	int wrote_dynlight = 0;
	char tmp[64];

#define PLAYSAVE_BUF_APPEND(s)                             \
	do {                                                   \
		int playsave_slen = (int) strlen(s);               \
		if (buf_len + playsave_slen < (int) sizeof(buf)) { \
			memcpy(buf + buf_len, s, playsave_slen);       \
			buf_len += playsave_slen;                      \
		}                                                  \
	} while (0)

#define PLAYSAVE_APPEND_ALPHA()                                                 \
	do {                                                                        \
		snprintf(tmp, sizeof(tmp), "alphaeffects=%i\n", alpha_effects ? 1 : 0); \
		PLAYSAVE_BUF_APPEND(tmp);                                               \
		wrote_alpha = 1;                                                        \
	} while (0)

#define PLAYSAVE_APPEND_DYNLIGHT()                                                \
	do {                                                                          \
		snprintf(tmp, sizeof(tmp), "dynlightcolor=%i\n", dynlight_color ? 1 : 0); \
		PLAYSAVE_BUF_APPEND(tmp);                                                 \
		wrote_dynlight = 1;                                                       \
	} while (0)

	if (f) {
		char line[256];
		while (fgets(line, sizeof(line), f)) {
			if (!in_graphics && !d_strnicmp(line, "[graphics]", 10)) {
				found_graphics = 1;
				in_graphics = 1;
				PLAYSAVE_BUF_APPEND(line);
				continue;
			}
			if (in_graphics && !d_strnicmp(line, "[end]", 5)) {
				if (!wrote_alpha)
					PLAYSAVE_APPEND_ALPHA();
				if (!wrote_dynlight)
					PLAYSAVE_APPEND_DYNLIGHT();
				PLAYSAVE_BUF_APPEND(line);
				in_graphics = 0;
				continue;
			}
			if (in_graphics && !d_strnicmp(line, "alphaeffects=", 13)) {
				PLAYSAVE_APPEND_ALPHA();
				continue;
			}
			if (in_graphics && !d_strnicmp(line, "dynlightcolor=", 14)) {
				PLAYSAVE_APPEND_DYNLIGHT();
				continue;
			}
			PLAYSAVE_BUF_APPEND(line);
		}
		fclose(f);
	}

	if (in_graphics) {
		if (!wrote_alpha)
			PLAYSAVE_APPEND_ALPHA();
		if (!wrote_dynlight)
			PLAYSAVE_APPEND_DYNLIGHT();
		PLAYSAVE_BUF_APPEND("[end]\n");
	}

	if (!found_graphics) {
		if (buf_len == 0)
			PLAYSAVE_BUF_APPEND(playsave_android_options_header());
		PLAYSAVE_BUF_APPEND("[graphics]\n");
		PLAYSAVE_APPEND_ALPHA();
		PLAYSAVE_APPEND_DYNLIGHT();
		PLAYSAVE_BUF_APPEND("[end]\n");
	}

#undef PLAYSAVE_BUF_APPEND
#undef PLAYSAVE_APPEND_ALPHA
#undef PLAYSAVE_APPEND_DYNLIGHT

	f = fopen(path, "w");
	if (!f) return 0;
	fwrite(buf, 1, buf_len, f);
	fflush(f);
	fsync(fileno(f));
	fclose(f);
	return 1;
}

int playsave_android_read_u16le(FILE *f, int *value)
{
	unsigned char buf[2];

	if (fread(buf, 1, 2, f) != 2)
		return 0;
	*value = buf[0] | (buf[1] << 8);
	return 1;
}

int playsave_android_read_u32le(FILE *f, unsigned int *value)
{
	unsigned char buf[4];

	if (fread(buf, 1, 4, f) != 4)
		return 0;
	*value = (unsigned) buf[0] | ((unsigned) buf[1] << 8) |
	         ((unsigned) buf[2] << 16) | ((unsigned) buf[3] << 24);
	return 1;
}

int playsave_android_patch_keysettings_common(const char *path,
                                              long ks_base,
                                              long control_type_offset,
                                              const ubyte *kb,
                                              int kb_len,
                                              const ubyte *joy,
                                              int joy_len,
                                              const ubyte *mouse,
                                              int mouse_len,
                                              int control_type)
{
	struct playsave_file_patch patches[4];
	unsigned char control = (unsigned char) control_type;
	size_t count = 0;

	if (!path || ks_base < 0 || control_type_offset < 0 || kb_len < 0 ||
	    joy_len < 0 || mouse_len < 0 || (!kb && kb_len) ||
	    (!joy && joy_len))
		return 0;

	patches[count].offset = (size_t) ks_base;
	patches[count].data = kb;
	patches[count++].size = kb_len < MAX_CONTROLS ? kb_len : MAX_CONTROLS;
	patches[count].offset = (size_t) (ks_base + MAX_CONTROLS);
	patches[count].data = joy;
	patches[count++].size = joy_len < MAX_CONTROLS ? joy_len : MAX_CONTROLS;
	if (mouse != NULL && mouse_len > 0) {
		patches[count].offset = (size_t) (ks_base + 5 * MAX_CONTROLS);
		patches[count].data = mouse;
		patches[count++].size = mouse_len < MAX_CONTROLS ? mouse_len : MAX_CONTROLS;
	}
	patches[count].offset = (size_t) control_type_offset;
	patches[count].data = &control;
	patches[count++].size = 1;
	return playsave_atomic_patch_file(path, patches, count);
}

int playsave_android_patch_u32le(const char *path, long offset,
                                 unsigned int value)
{
	unsigned char data[4] = {
		(unsigned char) (value & 0xff),
		(unsigned char) ((value >> 8) & 0xff),
		(unsigned char) ((value >> 16) & 0xff),
		(unsigned char) ((value >> 24) & 0xff)
	};
	struct playsave_file_patch patch = { (size_t) offset, data, sizeof(data) };

	return offset >= 0 && playsave_atomic_patch_file(path, &patch, 1);
}

int playsave_android_patch_u8_values(const char *path, const long *offsets,
                                     const unsigned char *values, int count)
{
	struct playsave_file_patch patches[8];
	int i;

	if (!offsets || !values || count < 0 || count > 8)
		return 0;
	for (i = 0; i < count; i++) {
		if (offsets[i] < 0)
			return 0;
		patches[i].offset = (size_t) offsets[i];
		patches[i].data = &values[i];
		patches[i].size = 1;
	}
	return playsave_atomic_patch_file(path, patches, (size_t) count);
}

int playsave_android_patch_weapon_order(const char *path, long offset,
                                        const ubyte *primary, int primary_len, const ubyte *secondary,
                                        int secondary_len)
{
	unsigned char data[2 * MAX_CONTROLS];
	struct playsave_file_patch patch;
	int count = primary_len > secondary_len ? primary_len : secondary_len;
	int i;

	if (offset < 0 || !primary || !secondary || primary_len < 0 ||
	    secondary_len < 0 || count > MAX_CONTROLS)
		return 0;
	for (i = 0; i < count; i++) {
		data[2 * i] = i < primary_len ? primary[i] : 0;
		data[2 * i + 1] = i < secondary_len ? secondary[i] : 0;
	}
	patch.offset = (size_t) offset;
	patch.data = data;
	patch.size = (size_t) (2 * count);
	return playsave_atomic_patch_file(path, &patch, 1);
}
