/*
 * extract_gog.c — Extract game files from GOG.com installers
 * this is a single file AI-slop implementation in order to keep this app simple and not have boost
 * the AI tool mostly used the innoextract codebase, which is zlib-style-licensed
 * because the tool didn't use much else, I'm choosing to include the original license notice
 * https://github.com/dscharrer/innoextract
 *
 * Usage: extract_gog <installer.exe|installer.pkg> [output_dir]
 *
 * Detects format from extension:
 *   .exe → InnoSetup (Windows GOG installer)
 *   .pkg → Mac .pkg (XAR+gzip+cpio)
 *
 * Lists all files in the installer, then extracts game-relevant files
 * (by extension) to the output directory.
 *
 * Build (via CMake from repo root):
 *   cmake -S android/app/src/main/cpp/extract -B android/tests/build
 *   cmake --build android/tests/build --config Release --target extract_gog
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
#include "pkg_reader.h"
#include "game_file_extensions.h"
#include "json_writer.h"

/* ── Cross-platform case-insensitive compare ─────────────────────── */
#ifndef _WIN32
#include <strings.h>
#define _stricmp strcasecmp
#endif

static int has_game_extension(const char *path)
{
	return dxx_has_android_game_file_extension(path);
}

/* Get just the filename from a path */
static const char *basename_only(const char *path)
{
	const char *p = path;
	const char *last = path;
	while (*p) {
		if (*p == '/' || *p == '\\') last = p + 1;
		p++;
	}
	return last;
}

static int progress_cb(const char *filename, long long done, long long total, void *ud)
{
	(void) ud;
	if (done == 0 && total > 0)
		fprintf(stderr, "  Extracting %s (%lld compressed bytes)...\n", filename, total);
	return 0;
}

/* ── Detect format from extension ────────────────────────────────── */
static int is_pkg_file(const char *path)
{
	const char *dot = strrchr(path, '.');
	return dot && _stricmp(dot, ".pkg") == 0;
}

/* ── InnoSetup (.exe) extraction ─────────────────────────────────── */
static int extract_exe(const char *exe_path, const char *out_dir)
{
	inno_archive_t arc;
	int nfiles = inno_open(exe_path, &arc);
	if (nfiles < 0) {
		fprintf(stderr, "ERROR: Failed to open %s\n", exe_path);
		return 1;
	}

	fprintf(stderr, "InnoSetup %d.%d.%d%s — %u files, %u data entries\n",
	        arc.version.major, arc.version.minor, arc.version.patch,
	        arc.version.unicode ? " (unicode)" : "",
	        arc.file_count, arc.data_entry_count);
	fprintf(stderr, "Compression: %d, header@0x%llx, data@0x%llx\n",
	        arc.compression,
	        (unsigned long long) arc.header_offset,
	        (unsigned long long) arc.data_offset);

	/* List all files */
	printf("[\n");
	for (uint32_t i = 0; i < arc.file_count; i++) {
		const char *dest = arc.files[i].destination;
		int is_game = has_game_extension(dest);
		uint64_t size = 0;
		const inno_data_entry_t *data = inno_file_data_entry(&arc, i);
		if (data)
			size = data->file_size;

		printf("  {\"index\": %u, \"dest\": ", i);
		json_write_string(stdout, dest);
		printf(", \"size\": %llu, \"game\": %s}%s\n",
		       (unsigned long long) size,
		       is_game ? "true" : "false",
		       (i < arc.file_count - 1) ? "," : "");
	}
	printf("]\n");

	/* Extract game files */
	mkdir_p(out_dir);
	int extracted = 0, errors = 0;

	for (uint32_t i = 0; i < arc.file_count; i++) {
		const char *dest = arc.files[i].destination;
		if (!has_game_extension(dest)) continue;

		const char *fname = basename_only(dest);
		char out_path[1024];
		snprintf(out_path, sizeof(out_path), "%s/%s", out_dir, fname);

		if (inno_extract_file(&arc, (int) i, out_path, progress_cb, NULL) == 0) {
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

/* ── Mac .pkg extraction ─────────────────────────────────────────── */
static int extract_pkg(const char *pkg_path, const char *out_dir)
{
	pkg_archive_t arc;
	int nfiles = pkg_open(pkg_path, &arc);
	if (nfiles < 0) {
		fprintf(stderr, "ERROR: Failed to open %s\n", pkg_path);
		return 1;
	}

	fprintf(stderr, "Mac .pkg — %d game files found\n", arc.file_count);

	/* List game files */
	printf("[\n");
	for (int i = 0; i < arc.file_count; i++) {
		printf("  {\"index\": %d, \"dest\": ", i);
		json_write_string(stdout, arc.files[i].name);
		printf(", \"size\": %llu, \"game\": true}%s\n",
		       (unsigned long long) arc.files[i].size,
		       (i < arc.file_count - 1) ? "," : "");
	}
	printf("]\n");

	/* Extract all game files */
	int extracted = pkg_extract_all(&arc, out_dir, progress_cb, NULL, 0);
	if (extracted < 0) {
		fprintf(stderr, "ERROR: Extraction failed\n");
		pkg_close(&arc);
		return 1;
	}

	fprintf(stderr, "\nDone: %d files extracted\n", extracted);
	pkg_close(&arc);
	return 0;
}

int main(int argc, char *argv[])
{
	if (argc < 2) {
		fprintf(stderr, "Usage: extract_gog <installer.exe|installer.pkg> [output_dir]\n");
		return 1;
	}

	const char *path = argv[1];
	const char *out_dir = (argc >= 3) ? argv[2] : "gog_extracted";

	if (is_pkg_file(path))
		return extract_pkg(path, out_dir);
	else
		return extract_exe(path, out_dir);
}
