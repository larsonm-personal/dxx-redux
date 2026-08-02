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
#include "playsave_text.h"
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

void android_get_default_boss_health_bar_prefs(int *show_boss_health_bar)
{
	if (show_boss_health_bar)
		*show_boss_health_bar = 1;
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
	char source_line[64];
	char prefer_line[32];
	char order_line[32];
	char volume_line[32];
	struct playsave_text_entry entries[4];

	source = playsave_android_clamp_int(source, 0, 3);
	play_order = playsave_android_clamp_int(play_order, 0, 2);
	volume = playsave_android_clamp_int(volume, 0, 8);
	snprintf(source_line, sizeof(source_line), "source=%s\n",
	         playsave_android_music_source_to_string(source));
	snprintf(prefer_line, sizeof(prefer_line), "prefermission=%i\n",
	         prefer_mission ? 1 : 0);
	snprintf(order_line, sizeof(order_line), "playorder=%i\n", play_order);
	snprintf(volume_line, sizeof(volume_line), "volume=%i\n", volume);
	entries[0].key = "source=";
	entries[0].line = source_line;
	entries[1].key = "prefermission=";
	entries[1].line = prefer_line;
	entries[2].key = "playorder=";
	entries[2].line = order_line;
	entries[3].key = "volume=";
	entries[3].line = volume_line;
	return playsave_text_update_section(path, playsave_android_options_header(),
	                                    "[music]", entries, 4);
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
	char line[64];
	struct playsave_text_entry entry = { "robothostagecounts=", line };

	snprintf(line, sizeof(line), "robothostagecounts=%i\n",
	         show_counts ? 1 : 0);
	return playsave_text_update_section(path, playsave_android_options_header(),
	                                    "[cockpit]", &entry, 1);
}

int plx_read_hud_prefs(const char *path, int *show_counts, int *show_boss_health_bar)
{
	FILE *f = fopen(path, "r");
	char line[256];
	int found = 0;
	int in_cockpit = 0;

	if (show_boss_health_bar)
		*show_boss_health_bar = 1;
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
			found = 1;
		} else if (!d_strnicmp(line, "bosshealthbar=", 14)) {
			if (show_boss_health_bar)
				*show_boss_health_bar = atoi(line + 14) ? 1 : 0;
			found = 1;
		}
	}

	fclose(f);
	return found;
}

int plx_write_hud_prefs(const char *path, int show_counts, int show_boss_health_bar)
{
	char counts_line[64];
	char boss_line[64];
	struct playsave_text_entry entries[2];

	snprintf(counts_line, sizeof(counts_line), "robothostagecounts=%i\n",
	         show_counts ? 1 : 0);
	snprintf(boss_line, sizeof(boss_line), "bosshealthbar=%i\n",
	         show_boss_health_bar ? 1 : 0);
	entries[0].key = "robothostagecounts=";
	entries[0].line = counts_line;
	entries[1].key = "bosshealthbar=";
	entries[1].line = boss_line;
	return playsave_text_update_section(path, playsave_android_options_header(),
	                                    "[cockpit]", entries, 2);
}

int plx_read_original_homing(const char *path, int *original_homing)
{
	FILE *f = fopen(path, "r");
	char line[256];
	int in_toggles = 0;

	if (!f) return 0;

	while (fgets(line, sizeof(line), f)) {
		if (!in_toggles) {
			if (!d_strnicmp(line, "[toggles]", 9))
				in_toggles = 1;
			continue;
		}
		if (!d_strnicmp(line, "[end]", 5))
			break;
		if (!d_strnicmp(line, "originalhoming=", 15)) {
			if (original_homing)
				*original_homing = atoi(line + 15) ? 1 : 0;
			fclose(f);
			return 1;
		}
	}

	fclose(f);
	return 0;
}

int plx_write_original_homing(const char *path, int original_homing)
{
	char line[32];
	struct playsave_text_entry entry = { "originalhoming=", line };

	snprintf(line, sizeof(line), "originalhoming=%i\n",
	         original_homing ? 1 : 0);
	return playsave_text_update_section(path, playsave_android_options_header(),
	                                    "[toggles]", &entry, 1);
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
	char alpha_line[32];
	char dynlight_line[32];
	struct playsave_text_entry entries[2];

	snprintf(alpha_line, sizeof(alpha_line), "alphaeffects=%i\n",
	         alpha_effects ? 1 : 0);
	snprintf(dynlight_line, sizeof(dynlight_line), "dynlightcolor=%i\n",
	         dynlight_color ? 1 : 0);
	entries[0].key = "alphaeffects=";
	entries[0].line = alpha_line;
	entries[1].key = "dynlightcolor=";
	entries[1].line = dynlight_line;
	return playsave_text_update_section(path, playsave_android_options_header(),
	                                    "[graphics]", entries, 2);
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
