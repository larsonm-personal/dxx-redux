/*
 * midi_enumeration.c -- Enumerate MIDI/HMP tracks from game HOG files
 *                       and mission directories.
 *
 * Reads HOG files directly (no PHYSFS) and parses .msn/.mn2 for mission
 * names using the same "name=" / "xname=" / "zname=" convention as
 * d2/main/mission.c.
 *
 * Keep mission name parsing in sync with d2/main/mission.c:read_mission_file().
 */

#include "midi_enumeration.h"
#include "midi_preview.h" /* hmp2mid_mem, hog_read_entry, hog_list_entries */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/stat.h>
#include <android/log.h>

#define TSF_NO_STDIO
#include "tsf.h"
#define TML_NO_STDIO
#include "tml.h"

#define TAG       "DXX-MidiEnum"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, TAG, __VA_ARGS__)
#define LOGW(...) __android_log_print(ANDROID_LOG_WARN, TAG, __VA_ARGS__)

/* ── Dynamic string builder ──────────────────────────────────────────── */

typedef struct {
	char *buf;
	int len;
	int cap;
} strbuf_t;

static void sb_init(strbuf_t *sb)
{
	sb->buf = (char *) malloc(4096);
	sb->len = 0;
	sb->cap = 4096;
	sb->buf[0] = '\0';
}

static void sb_ensure(strbuf_t *sb, int need)
{
	if (sb->len + need >= sb->cap) {
		int newcap = sb->cap * 2;
		while (newcap < sb->len + need + 1) newcap *= 2;
		sb->buf = (char *) realloc(sb->buf, newcap);
		sb->cap = newcap;
	}
}

static void sb_append(strbuf_t *sb, const char *s)
{
	int slen = (int) strlen(s);
	sb_ensure(sb, slen);
	memcpy(sb->buf + sb->len, s, slen + 1);
	sb->len += slen;
}

static void sb_appendf(strbuf_t *sb, const char *fmt, ...)
{
	char tmp[512];
	va_list ap;
	va_start(ap, fmt);
	vsnprintf(tmp, sizeof(tmp), fmt, ap);
	va_end(ap);
	sb_append(sb, tmp);
}

/* Escape a string for JSON (handle quotes and backslashes) */
static void sb_append_json_str(strbuf_t *sb, const char *s)
{
	sb_append(sb, "\"");
	for (const char *p = s; *p; p++) {
		if (*p == '"') sb_append(sb, "\\\"");
		else if (*p == '\\') sb_append(sb, "\\\\");
		else if (*p == '\n') sb_append(sb, "\\n");
		else if (*p == '\r') sb_append(sb, "\\r");
		else if (*p == '\t') sb_append(sb, "\\t");
		else {
			char c[2] = { *p, '\0' };
			sb_append(sb, c);
		}
	}
	sb_append(sb, "\"");
}

/* ── Track duration via TML ──────────────────────────────────────────── */

/* hmp2mid_mem is in d2/misc/hmp.c (and d1/misc/hmp.c).
 * hog_read_entry and hog_list_entries are in midi_preview.c. */
extern int hmp2mid_mem(const unsigned char *hmp, int hmp_len,
                       unsigned char **out_midi, int *out_len);
extern int hog_read_entry(const char *hog_path, const char *entry_name,
                          unsigned char **out_data, int *out_len);
extern int hog_list_entries(const char *hog_path, const char *ext,
                            char (*names)[14], int *sizes, int max_entries);

static int compute_hmp_duration(const unsigned char *hmp_data, int hmp_len)
{
	unsigned char *midi = NULL;
	int midi_len = 0;

	if (!hmp2mid_mem(hmp_data, hmp_len, &midi, &midi_len)) return -1;

	tml_message *msg = tml_load_memory(midi, midi_len);
	free(midi);
	if (!msg) return -1;

	unsigned int last_time = 0;
	for (tml_message *m = msg; m; m = m->next)
		if (m->time > last_time) last_time = m->time;
	tml_free(msg);
	return (int) last_time;
}

/* ── Mission name parsing ────────────────────────────────────────────── */

/*
 * Parse .msn or .mn2 to extract mission name.
 * Format: first non-comment line with "name=value" (or xname, zname, !name).
 * Keep in sync with d2/main/mission.c:read_mission_file().
 */
static int parse_mission_name(const char *path, char *out_name, int max_len)
{
	FILE *fp = fopen(path, "r");
	if (!fp) return 0;

	char line[256];
	const char *prefixes[] = { "name=", "xname=", "zname=", "!name=", NULL };

	while (fgets(line, sizeof(line), fp)) {
		/* Skip comments and blank lines */
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (*p == ';' || *p == '#' || *p == '\0' || *p == '\n') continue;

		for (int i = 0; prefixes[i]; i++) {
			int plen = (int) strlen(prefixes[i]);
			if (strncasecmp(p, prefixes[i], plen) == 0) {
				char *val = p + plen;
				/* Trim trailing whitespace, semicolons, newlines */
				char *end = val + strlen(val) - 1;
				while (end >= val && (*end == '\n' || *end == '\r' ||
				                      *end == ' ' || *end == '\t' || *end == ';'))
					end--;
				int vlen = (int) (end - val + 1);
				if (vlen > max_len - 1) vlen = max_len - 1;
				if (vlen > 0) {
					memcpy(out_name, val, vlen);
					out_name[vlen] = '\0';
					fclose(fp);
					return 1;
				}
			}
		}
	}
	fclose(fp);
	return 0;
}

/* ── HOG scanning ────────────────────────────────────────────────────── */

#define MAX_HOG_TRACKS 64

static void enumerate_hog_tracks(const char *hog_path, const char *source_id,
                                 const char *label, const char *game,
                                 strbuf_t *sb, int *first_source)
{
	char names[MAX_HOG_TRACKS][14];
	int sizes[MAX_HOG_TRACKS];

	int count = hog_list_entries(hog_path, ".hmp", names, sizes, MAX_HOG_TRACKS);
	if (count <= 0) {
		/* Also try .mid */
		count = hog_list_entries(hog_path, ".mid", names, sizes, MAX_HOG_TRACKS);
	}
	if (count <= 0) return;

	if (!*first_source) sb_append(sb, ",");
	*first_source = 0;

	sb_append(sb, "{\"id\":");
	sb_append_json_str(sb, source_id);
	sb_append(sb, ",\"label\":");
	sb_append_json_str(sb, label);
	sb_append(sb, ",\"game\":");
	sb_append_json_str(sb, game);
	sb_append(sb, ",\"hog\":");
	sb_append_json_str(sb, hog_path);
	sb_append(sb, ",\"tracks\":[");

	for (int i = 0; i < count; i++) {
		if (i > 0) sb_append(sb, ",");
		sb_append(sb, "{\"filename\":");
		sb_append_json_str(sb, names[i]);

		/* Compute duration */
		unsigned char *data = NULL;
		int data_len = 0;
		int dur_ms = -1;
		if (hog_read_entry(hog_path, names[i], &data, &data_len)) {
			/* Check extension for format */
			int nlen = (int) strlen(names[i]);
			int is_hmp = (nlen >= 4 && strncasecmp(names[i] + nlen - 4, ".hmp", 4) == 0);
			if (is_hmp) {
				dur_ms = compute_hmp_duration(data, data_len);
			} else {
				/* Standard MIDI -- parse directly */
				tml_message *msg = tml_load_memory(data, data_len);
				if (msg) {
					unsigned int last = 0;
					for (tml_message *m = msg; m; m = m->next)
						if (m->time > last) last = m->time;
					dur_ms = (int) last;
					tml_free(msg);
				}
			}
			free(data);
		}
		sb_appendf(sb, ",\"duration_ms\":%d}", dur_ms);
	}
	sb_append(sb, "]}");
}

/* ── Mission directory scanning ──────────────────────────────────────── */

static int has_extension(const char *name, const char *ext)
{
	int nlen = (int) strlen(name);
	int elen = (int) strlen(ext);
	return nlen > elen && strncasecmp(name + nlen - elen, ext, elen) == 0;
}

static void enumerate_missions_dir(const char *base_dir, const char *game,
                                   strbuf_t *sb, int *first_source)
{
	char missions_path[512];
	snprintf(missions_path, sizeof(missions_path), "%s/missions", base_dir);

	DIR *dir = opendir(missions_path);
	if (!dir) return;

	struct dirent *ent;
	while ((ent = readdir(dir)) != NULL) {
		if (ent->d_name[0] == '.') continue;

		/* Look for .hog files that might contain HMP tracks */
		if (!has_extension(ent->d_name, ".hog")) continue;

		char hog_path[512];
		snprintf(hog_path, sizeof(hog_path), "%s/%s", missions_path, ent->d_name);

		/* Check if this HOG has any HMP/MID entries */
		char test_names[1][14];
		int test_count = hog_list_entries(hog_path, ".hmp", test_names, NULL, 1);
		if (test_count <= 0)
			test_count = hog_list_entries(hog_path, ".mid", test_names, NULL, 1);
		if (test_count <= 0) continue;

		/* Try to find a mission descriptor for a friendly name */
		char mission_name[64];
		int got_name = 0;
		char base_name[256];
		strncpy(base_name, ent->d_name, sizeof(base_name) - 1);
		base_name[sizeof(base_name) - 1] = '\0';
		char *dot = strrchr(base_name, '.');
		if (dot) *dot = '\0';

		/* Try .mn2 then .msn */
		char desc_path[512];
		snprintf(desc_path, sizeof(desc_path), "%s/%s.mn2", missions_path, base_name);
		got_name = parse_mission_name(desc_path, mission_name, sizeof(mission_name));
		if (!got_name) {
			snprintf(desc_path, sizeof(desc_path), "%s/%s.msn", missions_path, base_name);
			got_name = parse_mission_name(desc_path, mission_name, sizeof(mission_name));
		}

		char source_id[128];
		snprintf(source_id, sizeof(source_id), "%s-mission-%s", game, base_name);

		char label[128];
		if (got_name)
			snprintf(label, sizeof(label), "%s", mission_name);
		else
			snprintf(label, sizeof(label), "%s", base_name);

		enumerate_hog_tracks(hog_path, source_id, label, game, sb, first_source);
	}
	closedir(dir);
}

/* ── Case-insensitive file finder ─────────────────────────────────────── */

/*
 * Find a file in dir by case-insensitive name match (Android/Linux is
 * case-sensitive, but GOG extraction may produce uppercase filenames).
 * Returns 1 and fills out_path on success, 0 if not found.
 */
static int find_file_ci(const char *dir, const char *name, char *out_path, int max_len)
{
	DIR *d = opendir(dir);
	if (!d) return 0;
	struct dirent *ent;
	while ((ent = readdir(d)) != NULL) {
		if (strcasecmp(ent->d_name, name) == 0) {
			snprintf(out_path, max_len, "%s/%s", dir, ent->d_name);
			closedir(d);
			return 1;
		}
	}
	closedir(d);
	return 0;
}

/* ── Public API ──────────────────────────────────────────────────────── */

char *midi_enumerate_tracks(const char *files_dir)
{
	strbuf_t sb;
	sb_init(&sb);
	sb_append(&sb, "{\"sources\":[");
	int first = 1;

	/* D2 built-in music from descent2.hog (case-insensitive lookup) */
	char hog_path[512];
	if (find_file_ci(files_dir, "descent2.hog", hog_path, sizeof(hog_path))) {
		enumerate_hog_tracks(hog_path, "d2-builtin", "Descent 2 (built-in)",
		                     "d2", &sb, &first);
	}

	/* D1 built-in music from descent.hog */
	if (find_file_ci(files_dir, "descent.hog", hog_path, sizeof(hog_path))) {
		enumerate_hog_tracks(hog_path, "d1-builtin", "Descent 1 (built-in)",
		                     "d1", &sb, &first);
	}

	/* D2X Vertigo expansion */
	if (find_file_ci(files_dir, "d2x.hog", hog_path, sizeof(hog_path))) {
		enumerate_hog_tracks(hog_path, "d2-vertigo", "Vertigo Series",
		                     "d2", &sb, &first);
	}

	/* Mission add-ons */
	enumerate_missions_dir(files_dir, "d2", &sb, &first);
	enumerate_missions_dir(files_dir, "d1", &sb, &first);

	sb_append(&sb, "]}");

	LOGI("Enumerated MIDI tracks: %d bytes JSON", sb.len);
	return sb.buf;
}
