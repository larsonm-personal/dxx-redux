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

/* Maximum number of files and directories we can list from an ISO.
 * Some supported retail discs exceed 512 catalog entries. */
#define ISO_MAX_FILES 4096

/* Maximum total file sections across one ISO catalog */
#define ISO_MAX_EXTENTS 4096

/* Maximum path length for an extracted file */
#define ISO_PATH_LEN 256

typedef struct {
	unsigned int lba;
	unsigned int size;
} iso_file_extent_t;

/* One file entry found in the ISO filesystem */
typedef struct {
	char path[ISO_PATH_LEN]; /* relative path, e.g. "MISSIONS/D2X.HOG" */
	unsigned int lba;        /* first logical block address */
	unsigned int size;       /* complete logical file size in bytes */
	int is_dir;              /* 1 = directory, 0 = file */
	unsigned int first_extent;
	unsigned int extent_count;
} iso_file_entry_t;

/* Result of scanning an ISO data track */
typedef struct {
	iso_file_entry_t files[ISO_MAX_FILES];
	iso_file_extent_t extents[ISO_MAX_EXTENTS];
	int num_files;
	int num_extents;
} iso_file_list_t;

/* Catalog storage is intentionally heap allocated because the bounded retail
 * capacity is too large for Android's native thread stack. */
iso_file_list_t *iso_file_list_create(void);
void iso_file_list_destroy(iso_file_list_t *list);

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
 * List files from a standalone ISO image file.
 *
 * iso_fd          : open file descriptor for the ISO file (read access)
 * out             : receives the file listing
 *
 * Returns number of files found, or -1 on error.
 */
int iso_list_image_files(int iso_fd, iso_file_list_t *out);

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

/*
 * Extract files from a standalone ISO image file to an output directory.
 *
 * iso_fd          : open file descriptor for the ISO file
 * file_list       : file listing from iso_list_image_files()
 * output_dir      : directory to extract files into (must exist)
 * extensions      : NULL-terminated array of lowercase extensions to extract
 *                   (NULL = extract all)
 * progress        : optional progress callback (may be NULL)
 * user_data       : passed to progress callback
 *
 * Returns number of files extracted, or -1 on error.
 */
int iso_extract_image_files(int iso_fd,
                            const iso_file_list_t *file_list,
                            const char *output_dir,
                            const char **extensions,
                            iso_progress_fn progress, void *user_data);

#ifdef ISO9660_READER_TESTING
int iso_test_append_extent_sizes(unsigned int first_size,
                                 unsigned int second_size);
#endif

#ifdef __cplusplus
}
#endif

#endif /* ISO9660_READER_H */
