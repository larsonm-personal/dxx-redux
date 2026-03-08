/*
 * Standalone CUE sheet parser for BIN/CUE disc images.
 *
 * Supports single-FILE and multi-FILE CUE sheets.  Returns a parsed
 * disc structure with per-track metadata (type, sector range, parent
 * BIN file index).  Used by both the C engine (rbaudio_bin.c) and the
 * JNI import flow.
 */

#ifndef CUE_PARSER_H
#define CUE_PARSER_H

#ifdef __cplusplus
extern "C" {
#endif

#define CUE_MAX_TRACKS   100
#define CUE_MAX_FILES    50
#define CUE_TITLE_LEN    64
#define CUE_FILENAME_LEN 256

#define CUE_TRACK_DATA   0
#define CUE_TRACK_AUDIO  1

/* Raw sector size on all CD formats */
#define CUE_SECTOR_SIZE  2352

/* One BIN file referenced by the CUE sheet */
typedef struct {
	char filename[CUE_FILENAME_LEN];
	int  file_index;          /* 0-based index in cue_disc_t.files[] */
	long long file_size;      /* bytes, -1 if unknown */
} cue_bin_file_t;

/* One track in the CUE sheet */
typedef struct {
	int  track_num;           /* 1-based */
	int  type;                /* CUE_TRACK_DATA or CUE_TRACK_AUDIO */
	int  file_index;          /* index into cue_disc_t.files[] */
	int  start_sector;        /* sector offset within the parent BIN file */
	int  num_sectors;         /* computed after parsing */
	char title[CUE_TITLE_LEN];
} cue_track_info_t;

/* Complete parsed CUE sheet */
typedef struct {
	cue_bin_file_t   files[CUE_MAX_FILES];
	int              num_files;
	cue_track_info_t tracks[CUE_MAX_TRACKS];
	int              num_tracks;
} cue_disc_t;

/*
 * Parse a CUE sheet from a buffer.
 *
 * cue_text     : NUL-terminated CUE sheet contents
 * bin_sizes    : array of BIN file sizes in bytes, indexed by file_index.
 *                Used to compute the last track's sector count in each file.
 *                May be NULL (sector counts left as 0 for last-in-file tracks).
 * num_bin_sizes: number of entries in bin_sizes
 * out          : receives the parsed disc structure
 *
 * Returns the number of tracks parsed, or 0 on failure.
 */
int cue_parse(const char *cue_text,
              const long long *bin_sizes, int num_bin_sizes,
              cue_disc_t *out);

/*
 * Convert MM:SS:FF to an absolute sector number.
 * 75 frames per second, 60 seconds per minute.
 */
int cue_msf_to_sector(const char *msf);

#ifdef __cplusplus
}
#endif

#endif /* CUE_PARSER_H */
