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

#include "cue_parser.h"

#ifdef _MSC_VER
#define strncasecmp _strnicmp
#endif

#ifdef ANDROID
#include <android/log.h>
#define CUE_LOG(...) __android_log_print(ANDROID_LOG_INFO, "CueParser", __VA_ARGS__)
#else
#define CUE_LOG(...) ((void)0)
#endif

int cue_msf_to_sector(const char *msf)
{
	int m = 0, s = 0, f = 0;
	if (sscanf(msf, "%d:%d:%d", &m, &s, &f) < 3) return 0;
	return m * 60 * 75 + s * 75 + f;
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
	len = (int)(q2 - q1);
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

	if (!cue_text || !out) return 0;

	memset(out, 0, sizeof(*out));

	while (*p) {
		char line[512];
		int li = 0;

		/* Read one line */
		while (*p && *p != '\n' && li < (int)sizeof(line) - 1) {
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
		    (trimmed[4] == ' ' || trimmed[4] == '\t'))
		{
			if (out->num_files >= CUE_MAX_FILES) continue;
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
		{
			int tnum;
			char ttype[32];
			if (sscanf(trimmed, "TRACK %d %31s", &tnum, ttype) == 2) {
				if (tnum >= 1 && tnum <= CUE_MAX_TRACKS && cur_file >= 0) {
					int idx = out->num_tracks;
					cur_track = tnum;
					out->tracks[idx].track_num = tnum;
					out->tracks[idx].file_index = cur_file;
					out->tracks[idx].type =
						(strstr(ttype, "AUDIO") != NULL) ? CUE_TRACK_AUDIO : CUE_TRACK_DATA;
					out->tracks[idx].title[0] = '\0';
					if (out->num_tracks < CUE_MAX_TRACKS)
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
		{
			int idx_num;
			char msf[32];
			if (cur_track >= 1 && out->num_tracks > 0 &&
			    sscanf(trimmed, "INDEX %d %31s", &idx_num, msf) == 2 && idx_num == 1)
			{
				out->tracks[out->num_tracks - 1].start_sector = cue_msf_to_sector(msf);
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
			/* Find the next track in the same file */
			int j;
			int found_next = 0;
			for (j = i + 1; j < out->num_tracks; j++) {
				if (out->tracks[j].file_index == fi) {
					int ns = out->tracks[j].start_sector - out->tracks[i].start_sector;
					out->tracks[i].num_sectors = (ns > 0) ? ns : 0;
					found_next = 1;
					break;
				}
			}
			if (!found_next) {
				/* Last track in this file — use file size */
				long long fsize = out->files[fi].file_size;
				if (fsize > 0) {
					int total = (int)(fsize / CUE_SECTOR_SIZE);
					int ns = total - out->tracks[i].start_sector;
					out->tracks[i].num_sectors = (ns > 0) ? ns : 0;
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
}
