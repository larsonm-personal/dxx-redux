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

int playsave_android_patch_keysettings_common(FILE *f,
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
	if (fseek(f, 0, SEEK_END) != 0 || ftell(f) < control_type_offset + 1)
		return 0;

	if (fseek(f, ks_base, SEEK_SET) != 0)
		return 0;
	fwrite(kb, 1, kb_len < MAX_CONTROLS ? kb_len : MAX_CONTROLS, f);

	if (fseek(f, ks_base + MAX_CONTROLS, SEEK_SET) != 0)
		return 0;
	fwrite(joy, 1, joy_len < MAX_CONTROLS ? joy_len : MAX_CONTROLS, f);

	if (mouse != NULL && mouse_len > 0) {
		if (fseek(f, ks_base + 5 * MAX_CONTROLS, SEEK_SET) != 0)
			return 0;
		fwrite(mouse, 1, mouse_len < MAX_CONTROLS ? mouse_len : MAX_CONTROLS, f);
	}

	if (fseek(f, control_type_offset, SEEK_SET) != 0)
		return 0;
	{
		unsigned char ct = (unsigned char) control_type;
		fwrite(&ct, 1, 1, f);
	}

	fflush(f);
	fsync(fileno(f));
	return 1;
}
