/*
 * pkg_reader.h -- Extract game files from Mac .pkg installers (XAR+gzip+cpio)
 *
 * Mac GOG installers are XAR archives containing a gzip-compressed cpio
 * payload. Game files live at ./payload/Contents/Resources/game/ inside
 * the cpio archive.
 *
 * Streaming two-pass API: pkg_open() scans cpio headers to build a file
 * list, pkg_extract_all() re-streams to extract matching files.
 * Call pkg_close() when done.
 */

#ifndef PKG_READER_H
#define PKG_READER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define PKG_MAX_FILES 128
#define PKG_PATH_LEN  512

/* -- File entry from the cpio archive ------------------------------ */
typedef struct {
	char name[PKG_PATH_LEN]; /* basename (e.g., "DESCENT.HOG") */
	uint64_t size;           /* uncompressed file size */
} pkg_file_entry_t;

/* -- Archive handle ------------------------------------------------ */
typedef struct {
	int fd;                      /* file descriptor (kept open) */
	uint64_t scripts_abs_offset; /* absolute offset of gzip data in file */
	uint64_t scripts_length;     /* length of gzip data */

	pkg_file_entry_t files[PKG_MAX_FILES];
	int file_count;
} pkg_archive_t;

/* -- Progress callback (same signature as inno_reader) ------------- */
typedef int (*pkg_progress_fn)(const char *current_file,
                               long long bytes_done,
                               long long bytes_total,
                               void *user_data);

/*
 * Open a .pkg file and scan for game files.
 *
 * Parses the XAR header and TOC, finds the Scripts payload,
 * streams through the gzip+cpio to enumerate game files.
 *
 * Returns number of game files found, or -1 on error.
 * On success, caller must call pkg_close().
 */
int pkg_open(const char *pkg_path, pkg_archive_t *arc);

/*
 * Extract all game files to output_dir in a single streaming pass.
 * If skip_audio is non-zero, .gog/.inst files are skipped.
 *
 * Returns number of files extracted, or -1 on error.
 */
int pkg_extract_all(pkg_archive_t *arc, const char *output_dir,
                    pkg_progress_fn progress, void *user_data,
                    int skip_audio);

/*
 * Close archive and free resources.
 */
void pkg_close(pkg_archive_t *arc);

#ifdef __cplusplus
}
#endif

#endif /* PKG_READER_H */
