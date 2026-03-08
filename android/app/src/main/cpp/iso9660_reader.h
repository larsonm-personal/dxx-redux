/*
 * ISO 9660 reader for Mode 1 data tracks in raw BIN/CUE disc images.
 *
 * Reads the ISO 9660 primary volume descriptor and directory tree from
 * raw 2352-byte sectors (Mode 1: 12 sync + 4 header + 2048 data + 288 ECC).
 * Extracts game files (.hog, .ham, .pig, etc.) preserving directory structure.
 *
 * For multi-data-track images, files from later tracks overlay earlier ones
 * (later track wins on name collision).
 */

#ifndef ISO9660_READER_H
#define ISO9660_READER_H

#ifdef __cplusplus
extern "C" {
#endif

/* Maximum number of files we can list from an ISO */
#define ISO_MAX_FILES 512

/* Maximum path length for an extracted file */
#define ISO_PATH_LEN  256

/* One file entry found in the ISO filesystem */
typedef struct {
	char     path[ISO_PATH_LEN];  /* relative path, e.g. "MISSIONS/D2X.HOG" */
	unsigned int lba;             /* logical block address within the data track */
	unsigned int size;            /* file size in bytes */
	int          is_dir;          /* 1 = directory, 0 = file */
} iso_file_entry_t;

/* Result of scanning an ISO data track */
typedef struct {
	iso_file_entry_t files[ISO_MAX_FILES];
	int              num_files;
} iso_file_list_t;

/* Progress callback for extraction.
 * current_file: name of file being extracted
 * bytes_done:   bytes extracted so far (across all files)
 * bytes_total:  total bytes to extract
 * Return 0 to continue, non-zero to cancel. */
typedef int (*iso_progress_fn)(const char *current_file,
                               long long bytes_done,
                               long long bytes_total,
                               void *user_data);

/*
 * List files on an ISO 9660 data track within a raw BIN file.
 *
 * bin_fd          : open file descriptor for the BIN file (read access)
 * track_start_sector : first sector of the data track in the BIN
 * track_num_sectors  : number of sectors in the data track
 * out             : receives the file listing
 *
 * Returns number of files found, or -1 on error.
 */
int iso_list_files(int bin_fd, int track_start_sector, int track_num_sectors,
                   iso_file_list_t *out);

/*
 * Extract files from an ISO 9660 data track to an output directory.
 *
 * bin_fd          : open file descriptor for the BIN file
 * track_start_sector : first sector of the data track in the BIN
 * track_num_sectors  : number of sectors in the data track
 * file_list       : file listing from iso_list_files()
 * output_dir      : directory to extract files into (must exist)
 * extensions      : NULL-terminated array of lowercase extensions to extract
 *                   (e.g., {"hog","ham","pig",NULL}).  NULL = extract all.
 * progress        : optional progress callback (may be NULL)
 * user_data       : passed to progress callback
 *
 * Returns number of files extracted, or -1 on error.
 */
int iso_extract_files(int bin_fd, int track_start_sector, int track_num_sectors,
                      const iso_file_list_t *file_list,
                      const char *output_dir,
                      const char **extensions,
                      iso_progress_fn progress, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* ISO9660_READER_H */
