/*
 * Standalone CUE sheet parser for BIN/CUE disc images.
 *
 * Handles single-FILE and multi-FILE CUE sheets.  Multi-FILE sheets
 * have separate BIN files per track (common for rips where each track
 * is a separate .bin).  Sector offsets are relative to the parent BIN
 * file — callers must map file_index → file handle for I/O.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <limits.h>

#include "cue_parser.h"

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#define strcasecmp  _stricmp
#endif

#ifdef ANDROID
#include <android/log.h>
#define CUE_LOG(...) __android_log_print(ANDROID_LOG_INFO, "CueParser", __VA_ARGS__)
#else
#define CUE_LOG(...) ((void) 0)
#endif

int cue_msf_to_sector(const char *msf)
{
	const unsigned char *p = (const unsigned char *) msf;
	unsigned long long component[3] = { 0, 0, 0 };
	int i;
	long long sector;

	if (!p) return -1;
	for (i = 0; i < 3; i++) {
		if (!isdigit(*p)) return -1;
		do {
			unsigned int digit = (unsigned int) (*p - '0');
			if (component[i] > ((unsigned long long) INT_MAX - digit) / 10)
				return -1;
			component[i] = component[i] * 10 + digit;
			p++;
		} while (isdigit(*p));
		if (i < 2) {
			if (*p++ != ':') return -1;
		} else if (*p != '\0') {
			return -1;
		}
	}
	if (component[1] >= 60 || component[2] >= 75) return -1;
	sector = (long long) component[0] * 60 * 75 +
	         (long long) component[1] * 75 + (long long) component[2];
	return sector <= INT_MAX ? (int) sector : -1;
}

/* Extract a quoted string from a line like:  FILE "name.bin" BINARY
 * Writes into dst (max dst_len-1 chars).  Returns 1 on success. */
static int extract_quoted(const char *line, char *dst, int dst_len)
{
	const char *q1 = strchr(line, '"');
	const char *q2;
	int len;
	if (!q1) return 0;
	q1++;
	q2 = strchr(q1, '"');
	if (!q2) return 0;
	len = (int) (q2 - q1);
	if (len >= dst_len) len = dst_len - 1;
	memcpy(dst, q1, len);
	dst[len] = '\0';
	return 1;
}

int cue_parse(const char *cue_text,
              const long long *bin_sizes, int num_bin_sizes,
              cue_disc_t *out)
{
	const char *p = cue_text;
	int cur_file = -1;
	int cur_track = -1;
	unsigned char has_index[CUE_MAX_TRACKS] = { 0 };

	if (!cue_text || !out) return 0;

	memset(out, 0, sizeof(*out));

	while (*p) {
		char line[512];
		int li = 0;

		/* Read one line */
		while (*p && *p != '\n' && li < (int) sizeof(line) - 1) {
			if (*p != '\r')
				line[li++] = *p;
			p++;
		}
		if (*p == '\n') p++;
		line[li] = '\0';

		/* Skip leading whitespace for matching */
		const char *trimmed = line;
		while (*trimmed == ' ' || *trimmed == '\t') trimmed++;

		/* FILE "name.bin" BINARY */
		if (strncasecmp(trimmed, "FILE", 4) == 0 &&
		    (trimmed[4] == ' ' || trimmed[4] == '\t')) {
			if (out->num_files >= CUE_MAX_FILES) {
				memset(out, 0, sizeof(*out));
				return 0;
			}
			cur_file = out->num_files;
			out->files[cur_file].file_index = cur_file;
			out->files[cur_file].file_size = -1;
			if (bin_sizes && cur_file < num_bin_sizes)
				out->files[cur_file].file_size = bin_sizes[cur_file];
			extract_quoted(trimmed, out->files[cur_file].filename, CUE_FILENAME_LEN);
			out->num_files++;
			continue;
		}

		/* TRACK nn TYPE */
		if (strncasecmp(trimmed, "TRACK", 5) == 0 &&
		    (trimmed[5] == ' ' || trimmed[5] == '\t')) {
			int tnum;
			char ttype[32];
			if (sscanf(trimmed + 5, " %d %31s", &tnum, ttype) == 2) {
				if (tnum >= 1 && tnum <= CUE_MAX_TRACKS && cur_file >= 0) {
					if (out->num_tracks >= CUE_MAX_TRACKS) {
						memset(out, 0, sizeof(*out));
						return 0;
					}
					int idx = out->num_tracks;
					cur_track = tnum;
					out->tracks[idx].track_num = tnum;
					out->tracks[idx].file_index = cur_file;
					if (strcasecmp(ttype, "AUDIO") == 0) {
						out->tracks[idx].type = CUE_TRACK_AUDIO;
						out->tracks[idx].sector_mode = CUE_SECTOR_AUDIO;
						out->tracks[idx].sector_size = 2352;
						out->tracks[idx].user_data_offset = 0;
					} else if (strcasecmp(ttype, "MODE1/2352") == 0) {
						out->tracks[idx].type = CUE_TRACK_DATA;
						out->tracks[idx].sector_mode = CUE_SECTOR_MODE1_2352;
						out->tracks[idx].sector_size = 2352;
						out->tracks[idx].user_data_offset = 16;
					} else if (strcasecmp(ttype, "MODE1/2048") == 0) {
						out->tracks[idx].type = CUE_TRACK_DATA;
						out->tracks[idx].sector_mode = CUE_SECTOR_MODE1_2048;
						out->tracks[idx].sector_size = 2048;
						out->tracks[idx].user_data_offset = 0;
					} else if (strcasecmp(ttype, "MODE2/2352") == 0) {
						out->tracks[idx].type = CUE_TRACK_DATA;
						out->tracks[idx].sector_mode = CUE_SECTOR_MODE2_2352;
						out->tracks[idx].sector_size = 2352;
						out->tracks[idx].user_data_offset = 24;
					} else {
						goto parse_failure;
					}
					out->tracks[idx].title[0] = '\0';
					out->num_tracks++;
				}
				continue;
			}
		}

		/* TITLE "..." */
		if (strncasecmp(trimmed, "TITLE", 5) == 0 && cur_track >= 1 && out->num_tracks > 0) {
			extract_quoted(trimmed, out->tracks[out->num_tracks - 1].title, CUE_TITLE_LEN);
			continue;
		}

		/* INDEX 01 MM:SS:FF */
		if (strncasecmp(trimmed, "INDEX", 5) == 0 &&
		    (trimmed[5] == ' ' || trimmed[5] == '\t')) {
			int idx_num;
			int sector;
			char msf[32];
			if (cur_track >= 1 && out->num_tracks > 0 &&
			    sscanf(trimmed + 5, " %d %31s", &idx_num, msf) == 2 && idx_num == 1) {
				int track_index = out->num_tracks - 1;
				sector = cue_msf_to_sector(msf);
				if (sector < 0 || has_index[track_index]) goto parse_failure;
				out->tracks[track_index].start_sector = sector;
				has_index[track_index] = 1;
			}
		}
	}

	/* Compute sector counts for each track.
	 * For tracks within the same BIN file, length = next_track.start - this.start.
	 * For the last track in a BIN file, length = (file_size / SECTOR_SIZE) - start. */
	{
		int i;
		for (i = 0; i < out->num_tracks; i++) {
			int fi = out->tracks[i].file_index;
			if (!has_index[i] || fi < 0 || fi >= out->num_files)
				goto parse_failure;
			/* Find the next track in the same file */
			int j;
			int found_next = 0;
			for (j = i + 1; j < out->num_tracks; j++) {
				if (out->tracks[j].file_index == fi) {
					if (out->tracks[j].sector_size != out->tracks[i].sector_size)
						goto parse_failure;
					int ns = out->tracks[j].start_sector - out->tracks[i].start_sector;
					if (ns <= 0) goto parse_failure;
					out->tracks[i].num_sectors = ns;
					found_next = 1;
					break;
				}
			}
			if (!found_next) {
				/* Last track in this file — use file size */
				long long fsize = out->files[fi].file_size;
				if (bin_sizes && fi < num_bin_sizes) {
					long long total = fsize / out->tracks[i].sector_size;
					if (fsize <= 0 || total > INT_MAX ||
					    out->tracks[i].start_sector >= total)
						goto parse_failure;
					out->tracks[i].num_sectors =
					    (int) (total - out->tracks[i].start_sector);
				}
				/* else: leave as 0, caller can fix up */
			}
		}
	}

	CUE_LOG("Parsed %d files, %d tracks", out->num_files, out->num_tracks);
	{
		int i;
		for (i = 0; i < out->num_tracks; i++) {
			CUE_LOG("  Track %d: %s  file=%d  start=%d  len=%d  title='%s'",
			        out->tracks[i].track_num,
			        out->tracks[i].type == CUE_TRACK_AUDIO ? "audio" : "data",
			        out->tracks[i].file_index,
			        out->tracks[i].start_sector,
			        out->tracks[i].num_sectors,
			        out->tracks[i].title);
		}
	}

	return out->num_tracks;

parse_failure:
	memset(out, 0, sizeof(*out));
	return 0;
}
