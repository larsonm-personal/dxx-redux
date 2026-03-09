/*
 * extract_gog.c — Extract game files from GOG.com InnoSetup installers.
 *
 * Usage: extract_gog <installer.exe> [output_dir]
 *
 * Lists all files in the installer, then extracts game-relevant files
 * (by extension) to the output directory.
 *
 * Build (via CMake from repo root):
 *   cmake -S android/app/src/main/cpp/extract -B android/tests/build
 *   cmake --build android/tests/build --config Release --target extract_gog
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>

#ifdef _WIN32
  #include <direct.h>
  #define mkdir_p(d) _mkdir(d)
#else
  #include <sys/stat.h>
  #define mkdir_p(d) mkdir((d), 0755)
#endif

#include "inno_reader.h"

/* ── Cross-platform case-insensitive compare ─────────────────────── */
#ifndef _WIN32
  #include <strings.h>
  #define _stricmp strcasecmp
#endif

/* ── Game file extensions we care about ──────────────────────────── */
static const char *game_extensions[] = {
    ".hog", ".pig", ".ham", ".s11", ".s22", ".dem",
    ".mvl", ".msn", ".mn2", ".gog", ".inst",
    NULL
};

static int has_game_extension(const char *path) {
    const char *dot = strrchr(path, '.');
    if (!dot) return 0;
    for (const char **ext = game_extensions; *ext; ext++) {
        if (_stricmp(dot, *ext) == 0) return 1;
    }
    return 0;
}

/* Get just the filename from a path */
static const char *basename_only(const char *path) {
    const char *p = path;
    const char *last = path;
    while (*p) {
        if (*p == '/' || *p == '\\') last = p + 1;
        p++;
    }
    return last;
}

static int progress_cb(const char *filename, long long done, long long total, void *ud) {
    (void)ud;
    if (done == 0 && total > 0)
        fprintf(stderr, "  Extracting %s (%lld bytes)...\n", filename, total);
    return 0;
}

int main(int argc, char *argv[]) {
    if (argc < 2) {
        fprintf(stderr, "Usage: extract_gog <installer.exe> [output_dir]\n");
        return 1;
    }

    const char *exe_path = argv[1];
    const char *out_dir = (argc >= 3) ? argv[2] : "gog_extracted";

    inno_archive_t arc;
    int nfiles = inno_open(exe_path, &arc);
    if (nfiles < 0) {
        fprintf(stderr, "ERROR: Failed to open %s\n", exe_path);
        return 1;
    }

    fprintf(stderr, "InnoSetup %d.%d.%d%s — %d files, %d data entries\n",
            arc.version.major, arc.version.minor, arc.version.patch,
            arc.version.unicode ? " (unicode)" : "",
            arc.file_count, arc.data_entry_count);
    fprintf(stderr, "Compression: %d, header@0x%llx, data@0x%llx\n",
            arc.compression,
            (unsigned long long)arc.header_offset,
            (unsigned long long)arc.data_offset);

    /* List all files */
    printf("[\n");
    for (int i = 0; i < arc.file_count; i++) {
        const char *dest = arc.files[i].destination;
        int is_game = has_game_extension(dest);
        uint64_t size = 0;
        if (arc.files[i].location < (uint32_t)arc.data_entry_count)
            size = arc.data_entries[arc.files[i].location].file_size;

        printf("  {\"index\": %d, \"dest\": \"%s\", \"size\": %llu, \"game\": %s}%s\n",
               i, dest, (unsigned long long)size,
               is_game ? "true" : "false",
               (i < arc.file_count - 1) ? "," : "");
    }
    printf("]\n");

    /* Extract game files */
    mkdir_p(out_dir);
    int extracted = 0, errors = 0;

    for (int i = 0; i < arc.file_count; i++) {
        const char *dest = arc.files[i].destination;
        if (!has_game_extension(dest)) continue;

        const char *fname = basename_only(dest);
        char out_path[1024];
        snprintf(out_path, sizeof(out_path), "%s/%s", out_dir, fname);

        if (inno_extract_file(&arc, i, out_path, progress_cb, NULL) == 0) {
            extracted++;
        } else {
            fprintf(stderr, "ERROR: Failed to extract %s\n", dest);
            errors++;
        }
    }

    fprintf(stderr, "\nDone: %d files extracted, %d errors\n", extracted, errors);

    inno_close(&arc);
    return errors > 0 ? 1 : 0;
}
