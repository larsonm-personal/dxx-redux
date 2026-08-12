#include "android_save_set.h"

#include <ctype.h>
#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const char *android_save_set_root(int use_players_dir)
{
	return use_players_dir ? ANDROID_SAVE_SET_ROOT_PLAYERS
	                       : ANDROID_SAVE_SET_ROOT_LOCAL;
}

void android_save_set_sanitize_component(char *dst, size_t dst_size,
                                         const char *src,
                                         const char *fallback)
{
	size_t i;
	size_t out = 0;
	int has_name_char = 0;

	if (!dst || !dst_size)
		return;
	dst[0] = '\0';
	if (!src || !src[0])
		src = fallback;
	if (!src)
		src = "set";

	for (i = 0; src[i] && out + 1 < dst_size; i++) {
		unsigned char c = (unsigned char) src[i];
		if (isalnum(c) || c == '-' || c == '_' ||
		    (c == '.' && out > 0 && dst[out - 1] != '.')) {
			dst[out++] = (char) tolower(c);
			if (c != '.')
				has_name_char = 1;
		} else if (out > 0 && dst[out - 1] != '_') {
			dst[out++] = '_';
		}
	}
	dst[out] = '\0';

	if (!has_name_char) {
		const char *name = (fallback && fallback[0]) ? fallback : "set";
		snprintf(dst, dst_size, "%s", name);
		for (i = 0; dst[i]; i++) {
			unsigned char c = (unsigned char) dst[i];
			dst[i] = (isalnum(c) || c == '-' || c == '_' || c == '.')
			             ? (char) tolower(c)
			             : '_';
		}
	}
}

static void android_save_set_mission_component(char *dst, size_t dst_size,
                                               const char *mission)
{
	char full[PATH_MAX];
	uint64_t hash = UINT64_C(1469598103934665603);
	size_t i;
	size_t prefix;

	android_save_set_sanitize_component(full, sizeof(full), mission, "default");
	if (strlen(full) < dst_size) {
		snprintf(dst, dst_size, "%s", full);
		return;
	}
	for (i = 0; full[i]; ++i) {
		hash ^= (unsigned char) full[i];
		hash *= UINT64_C(1099511628211);
	}
	prefix = dst_size > 18 ? dst_size - 18 : 0;
	snprintf(dst, dst_size, "%.*s-%016llx", (int) prefix, full,
	         (unsigned long long) hash);
}

int android_save_set_build_slot_path(char *dst, size_t dst_size,
                                     int use_players_dir,
                                     const char *scope,
                                     const char *pilot,
                                     const char *mission,
                                     int slotnum,
                                     int coop)
{
	char pilot_dir[32];
	char mission_dir[32];
	char file_pilot[32];

	if (!dst || !dst_size || slotnum < 0 || slotnum > 9)
		return 0;

	android_save_set_mission_component(mission_dir, sizeof(mission_dir), mission);
	if (coop) {
		android_save_set_sanitize_component(file_pilot, sizeof(file_pilot),
		                                    pilot, ANDROID_SAVE_SET_COOP_CALLSIGN);
		return snprintf(dst, dst_size, "%s/%s/%s/%s.mg%x",
		                android_save_set_root(use_players_dir), scope,
		                mission_dir, file_pilot, slotnum) < (int) dst_size;
	}

	android_save_set_sanitize_component(pilot_dir, sizeof(pilot_dir),
	                                    pilot, "player");
	android_save_set_sanitize_component(file_pilot, sizeof(file_pilot),
	                                    pilot, "player");
	return snprintf(dst, dst_size, "%s/%s/%s/%s/%s.sg%x",
	                android_save_set_root(use_players_dir), scope, pilot_dir,
	                mission_dir, file_pilot, slotnum) < (int) dst_size;
}

int android_save_set_build_secret_path(char *dst, size_t dst_size,
                                       int use_players_dir,
                                       const char *pilot,
                                       const char *mission,
                                       int slotnum)
{
	char pilot_dir[32];
	char mission_dir[32];
	char fc;

	if (!dst || !dst_size || slotnum < 0 || slotnum > 15)
		return 0;
	fc = slotnum >= 10 ? (char) ('a' + slotnum - 10) : (char) ('0' + slotnum);
	android_save_set_sanitize_component(pilot_dir, sizeof(pilot_dir),
	                                    pilot, "player");
	android_save_set_mission_component(mission_dir, sizeof(mission_dir), mission);
	return snprintf(dst, dst_size, "%s/single/%s/%s/%csecret.sgc",
	                android_save_set_root(use_players_dir), pilot_dir,
	                mission_dir, fc) < (int) dst_size;
}

int android_save_set_build_sidecar_path(char *dst, size_t dst_size,
                                        int use_players_dir,
                                        const char *scope,
                                        const char *pilot,
                                        const char *mission,
                                        const char *filename)
{
	char pilot_dir[32];
	char mission_dir[32];
	char safe_name[64];

	if (!dst || !dst_size || !filename || !filename[0])
		return 0;
	android_save_set_mission_component(mission_dir, sizeof(mission_dir), mission);
	android_save_set_sanitize_component(safe_name, sizeof(safe_name),
	                                    filename, "sidecar");
	if (scope && !strcmp(scope, "coop"))
		return snprintf(dst, dst_size, "%s/%s/%s/%s",
		                android_save_set_root(use_players_dir), scope,
		                mission_dir, safe_name) < (int) dst_size;

	android_save_set_sanitize_component(pilot_dir, sizeof(pilot_dir),
	                                    pilot, "player");
	return snprintf(dst, dst_size, "%s/%s/%s/%s/%s",
	                android_save_set_root(use_players_dir), scope, pilot_dir,
	                mission_dir, safe_name) < (int) dst_size;
}
