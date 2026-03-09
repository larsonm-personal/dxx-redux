/*
 * inno_reader.h — Read and extract files from InnoSetup installers.
 *
 * Supports InnoSetup 5.3.x through 5.6.x (Unicode builds) as used by
 * GOG.com game installers.  Only LZMA1 and zlib decompression paths are
 * implemented; other methods (BZip2, LZMA2) return errors at runtime.
 *
 * Two-pass API: call inno_open() to parse the archive, then
 * inno_extract_file() for each file you want.  Call inno_close() when done.
 */

#ifndef INNO_READER_H
#define INNO_READER_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── Limits ──────────────────────────────────────────────────────── */
#define INNO_MAX_FILES  512
#define INNO_PATH_LEN   512

/* ── Compression method enum (full, from InnoSetup source) ─────── */
typedef enum {
    INNO_COMPRESS_STORED = 0,
    INNO_COMPRESS_ZLIB   = 1,
    INNO_COMPRESS_BZIP2  = 2,
    INNO_COMPRESS_LZMA1  = 3,
    INNO_COMPRESS_LZMA2  = 4,
} inno_compress_method_t;

/* ── InnoSetup version ───────────────────────────────────────────── */
typedef struct {
    int major, minor, patch;
    int unicode;   /* 1 if "(u)" or "(U)" suffix */
} inno_version_t;

/* ── File entry (from the decompressed header) ───────────────────── */
typedef struct {
    char     destination[INNO_PATH_LEN];  /* install-relative path */
    uint32_t location;                    /* index into data entries */
    uint64_t external_size;               /* expected file size      */
    uint8_t  gog_galaxy;                  /* needs zlib decompression */
} inno_file_entry_t;

/* ── Data entry (from the second block stream) ───────────────────── */
typedef struct {
    uint32_t first_slice;
    uint32_t last_slice;
    uint64_t chunk_offset;         /* byte offset in setup-1.bin     */
    uint64_t file_offset;          /* offset within decompressed chunk */
    uint64_t file_size;            /* decompressed file size          */
    uint64_t chunk_compressed_size;
    uint8_t  sha1[20];
    uint8_t  chunk_compressed;     /* ChunkCompressed flag            */
    uint8_t  call_instruction_optimized; /* filter flag               */
} inno_data_entry_t;

/* ── Archive handle ──────────────────────────────────────────────── */
typedef struct {
    /* parsed version */
    inno_version_t version;
    inno_compress_method_t compression;

    /* offset table */
    uint64_t header_offset;  /* start of setup-0.bin in file */
    uint64_t data_offset;    /* start of setup-1.bin in file */

    /* entries */
    inno_file_entry_t  files[INNO_MAX_FILES];
    int                file_count;
    inno_data_entry_t  *data_entries;    /* heap-allocated */
    int                data_entry_count;

    /* file handle (kept open for extraction) */
    int fd;
} inno_archive_t;

/* ── Progress callback (same signature as iso/sow modules) ────── */
typedef int (*inno_progress_fn)(const char *current_file,
                                 long long bytes_done,
                                 long long bytes_total,
                                 void *user_data);

/*
 * Open an InnoSetup installer and parse its metadata.
 *
 * Populates arc->files[] and arc->data_entries.
 * Returns number of file entries on success, or -1 on error.
 * On success, the caller must call inno_close(arc) when done.
 */
int inno_open(const char *exe_path, inno_archive_t *arc);

/*
 * Extract a single file by index (0 .. arc->file_count-1).
 *
 * output_path : full path of the output file to create.
 * progress    : optional callback (may be NULL).
 * user_data   : passed to callback.
 *
 * Returns 0 on success, -1 on error.
 */
int inno_extract_file(inno_archive_t *arc, int file_index,
                      const char *output_path,
                      inno_progress_fn progress, void *user_data);

/*
 * Close an archive and free resources.
 */
void inno_close(inno_archive_t *arc);

#ifdef __cplusplus
}
#endif

#endif /* INNO_READER_H */
