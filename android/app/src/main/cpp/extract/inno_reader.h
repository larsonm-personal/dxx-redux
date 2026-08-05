/*
 * inno_reader.h -- Read and extract files from InnoSetup installers
 * this is a single file AI-slop implementation in order to keep this app simple and not have boost
 * the AI tool mostly used the innoextract codebase, which is zlib-style-licensed
 * because the tool didn't use much else, I'm choosing to include the original license notice
 * https://github.com/dscharrer/innoextract
 *
 * Supports InnoSetup 5.3.x through 5.6.x (Unicode builds) as used by
 * GOG.com game installers.  Only LZMA1 and zlib decompression paths are
 * implemented; other methods (BZip2, LZMA2) return errors at runtime.
 *
 * Two-pass API: call inno_open() to parse the archive, then
 * inno_extract_file() for each file you want.  Call inno_close() when done.
 */

/*
 * Maintenance note: the historical preamble above is preserved verbatim.
 * Its capability claims are superseded by the implementation-checked matrix
 * in INNO_READER_CAPABILITIES.md.
 */

/*
 * Copyright (C) 2011-2020 Daniel Scharrer
 *
 * This software is provided 'as-is', without any express or implied
 * warranty.  In no event will the author(s) be held liable for any damages
 * arising from the use of this software.
 *
 * Permission is granted to anyone to use this software for any purpose,
 * including commercial applications, and to alter it and redistribute it
 * freely, subject to the following restrictions:
 *
 * 1. The origin of this software must not be misrepresented; you must not
 *    claim that you wrote the original software. If you use this software
 *    in a product, an acknowledgment in the product documentation would be
 *    appreciated but is not required.
 * 2. Altered source versions must be plainly marked as such, and must not be
 *    misrepresented as being the original software.
 * 3. This notice may not be removed or altered from any source distribution.
 */

#ifndef INNO_READER_H
#define INNO_READER_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -- Limits -------------------------------------------------------- */
#define INNO_PATH_LEN 512 /* UTF-8 bytes including the terminator */

/* -- Compression method enum (full, from InnoSetup source) ------- */
typedef enum {
	INNO_COMPRESS_STORED = 0,
	INNO_COMPRESS_ZLIB = 1,
	INNO_COMPRESS_BZIP2 = 2,
	INNO_COMPRESS_LZMA1 = 3,
	INNO_COMPRESS_LZMA2 = 4,
} inno_compress_method_t;

typedef enum {
	INNO_CHECKSUM_NONE = 0,
	INNO_CHECKSUM_MD5 = 1,
	INNO_CHECKSUM_SHA1 = 2,
} inno_checksum_type_t;

/* -- InnoSetup version --------------------------------------------- */
typedef struct {
	int major, minor, patch;
	int unicode; /* 1 if "(u)" or "(U)" suffix */
} inno_version_t;

/* -- File entry (from the decompressed header) --------------------- */
typedef struct {
	char destination[INNO_PATH_LEN]; /* install-relative path */
	uint32_t location;               /* index into data entries */
	uint64_t external_size;          /* expected file size      */
	uint8_t gog_galaxy;              /* needs zlib decompression */
} inno_file_entry_t;

/* -- Data entry (from the second block stream) --------------------- */
typedef struct {
	uint32_t first_slice;
	uint32_t last_slice;
	uint64_t chunk_offset; /* byte offset in setup-1.bin     */
	uint64_t file_offset;  /* offset within decompressed chunk */
	uint64_t file_size;    /* decompressed file size          */
	uint64_t chunk_compressed_size;
	uint8_t checksum[20];
	inno_checksum_type_t checksum_type;
	uint8_t chunk_compressed;           /* ChunkCompressed flag            */
	uint8_t chunk_encrypted;            /* unsupported ChunkEncrypted flag */
	uint8_t call_instruction_optimized; /* filter flag               */
} inno_data_entry_t;

/* -- Archive handle ------------------------------------------------ */
typedef struct {
	/* parsed version */
	inno_version_t version;
	inno_compress_method_t compression;

	/* offset table */
	uint64_t header_offset; /* start of setup-0.bin in file */
	uint64_t data_offset;   /* start of setup-1.bin in file */
	uint64_t source_size;   /* complete installer size      */

	/* entries */
	inno_file_entry_t *files; /* heap-allocated */
	uint32_t file_count;
	inno_data_entry_t *data_entries; /* heap-allocated */
	uint32_t data_entry_count;
	uint64_t extracted_bytes;
	uint32_t extracted_files;

	/* file handle (kept open for extraction) */
	int fd;
} inno_archive_t;

/* -- Progress callback (same signature as iso/sow modules) ------ */
/*
 * Reports compressed-input progress. The return value is currently ignored,
 * so this callback is informational and cannot cancel extraction.
 */
typedef int (*inno_progress_fn)(const char *current_file,
                                long long bytes_done,
                                long long bytes_total,
                                void *user_data);
typedef int (*inno_path_select_fn)(const char *path, void *user_data);

/*
 * Open an InnoSetup installer and parse its metadata.
 *
 * Populates arc->files[] and arc->data_entries.
 * Returns number of file entries on success, or -1 on error.
 * On success, the caller must call inno_close(arc) when done.
 */
int inno_open(const char *exe_path, inno_archive_t *arc);

/*
 * Open an InnoSetup installer from an already-open fd.
 * The fd is duplicated, so the caller keeps ownership of source_fd
 */
int inno_open_fd(int source_fd, inno_archive_t *arc);

/* Returns the file's validated data entry, or NULL for no-data/invalid indices */
const inno_data_entry_t *inno_file_data_entry(const inno_archive_t *arc,
                                              uint32_t file_index);

/*
 * Return nonzero when selected files have distinct flattened output basenames.
 * ASCII case differences collide so the result is safe on Windows and Android.
 */
int inno_output_names_unique(const inno_archive_t *arc,
                             inno_path_select_fn select_path,
                             void *user_data);

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

#ifdef INNO_READER_TESTING
void inno_test_set_allocation_fail_after(int allocations);
int inno_test_find_pe_resource_11111(int fd, uint64_t source_size,
                                     uint64_t *offset_out);
int inno_test_read_string(const uint8_t *buffer, size_t buffer_size,
                          char *output, size_t output_size, int unicode);
int inno_test_destination_path_valid(const char *path);
int inno_test_parse_gog_galaxy_before_install(const char *script,
                                              char *destination,
                                              size_t destination_size,
                                              uint32_t *part_count);
int inno_test_parse_version_id(const uint8_t id[64],
                               inno_version_t *version);
int inno_test_checksum_layout(const inno_version_t *version,
                              inno_checksum_type_t *type_out,
                              size_t *digest_size_out);
int inno_test_parse_header_stream(const uint8_t *buffer, size_t buffer_size,
                                  const inno_version_t *version,
                                  inno_compress_method_t *compression_out);
int inno_test_parse_file_catalog(const uint8_t *buffer, size_t buffer_size,
                                 const inno_version_t *version,
                                 uint32_t *file_count_out,
                                 char *last_destination,
                                 size_t last_destination_size,
                                 int *last_gog_galaxy);
int inno_test_parse_data_entries(const uint8_t *buffer, size_t buffer_size,
                                 const inno_version_t *version,
                                 inno_data_entry_t *entries,
                                 uint32_t entry_count);
int inno_test_entry_count_allowed(uint32_t entry_count);
int inno_test_data_location_valid(uint32_t data_entry_count,
                                  int has_backing_array,
                                  uint32_t location);
#endif

#ifdef __cplusplus
}
#endif

#endif /* INNO_READER_H */
