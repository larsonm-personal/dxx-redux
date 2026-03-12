/*
 * sow_extract.h -- Extract game files from Descent .sow (ARJ) archives.
 *
 * SOW files are ARJ-compressed archives used by Interplay's installer on
 * original Descent II CD-ROMs.  This module wraps libarchive to extract
 * game assets from them.
 *
 * Three on-disc patterns exist:
 *   1. d2data/descent2.sow              (most retail discs)
 *   2. sowstuff/d2_1.sow … d2_3.sow     (3-Level Preview)
 *   3. descent1.sow + descent2.sow      (Test Flight, at root)
 */

#ifndef SOW_EXTRACT_H
#define SOW_EXTRACT_H

#ifdef __cplusplus
extern "C" {
#endif

#define SOW_MAX_FILES 64 /* max .sow files found in one scan */
#define SOW_PATH_LEN  512

/* Result of scanning a directory tree for .sow files */
typedef struct {
	char paths[SOW_MAX_FILES][SOW_PATH_LEN]; /* absolute paths */
	int count;
} sow_file_list_t;

/* Progress callback -- same signature as iso_progress_fn.
 * Return 0 to continue, non-zero to cancel. */
typedef int (*sow_progress_fn)(const char *current_file,
                               long long bytes_done,
                               long long bytes_total,
                               void *user_data);

/*
 * Recursively scan a directory for .sow files.
 * Populates out->paths[] and out->count.
 * Returns number of .sow files found, or -1 on error.
 */
int sow_scan_dir(const char *dir_path, sow_file_list_t *out);

/*
 * Extract files from a .sow (ARJ) archive to an output directory.
 *
 * sow_path   : path to the .sow file
 * output_dir : directory to extract into (created if needed)
 * extensions : NULL-terminated array of lowercase extensions to extract
 *              (e.g. {"hog","ham","pig",NULL}).  NULL = extract all.
 * progress   : optional progress callback (may be NULL)
 * user_data  : passed to progress callback
 *
 * Internal archive paths are flattened -- only the filename is used
 * (e.g. "zero/descent2.hog" → "descent2.hog").
 *
 * Returns number of files extracted, or -1 on error.
 */
int sow_extract(const char *sow_path, const char *output_dir,
                const char **extensions,
                sow_progress_fn progress, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* SOW_EXTRACT_H */
