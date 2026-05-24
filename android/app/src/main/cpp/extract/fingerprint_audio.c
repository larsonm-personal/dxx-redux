/*
 * fingerprint_audio.c -- Standalone tool to fingerprint loose audio files
 *                        (MP3/OGG/FLAC) using Chromaprint.
 *
 * Usage: fingerprint_audio <directory>
 *
 * Scans the directory for .mp3, .ogg, .flac files, fingerprints each one,
 * and outputs a JSON array to stdout sorted by filename.
 *
 * Build (via CMake from repo root):
 *   cmake -S android/app/src/main/cpp/extract -B android/tests/build
 *   cmake --build android/tests/build --config Release --target fingerprint_audio
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#define PATH_SEP '\\'
#else
#include <dirent.h>
#define PATH_SEP '/'
#endif

#include "fingerprint_gen.h"

/* Maximum files we'll handle in one directory */
#define MAX_FILES    4096
#define MAX_PATH_LEN 1024

/* Simple filename collection for sorting */
static char s_filenames[MAX_FILES][MAX_PATH_LEN];
static int s_file_count = 0;

static int str_iequal(const char *a, const char *b)
{
	while (*a && *b) {
		char ca = *a >= 'A' && *a <= 'Z' ? (*a + 32) : *a;
		char cb = *b >= 'A' && *b <= 'Z' ? (*b + 32) : *b;
		if (ca != cb) return 0;
		a++;
		b++;
	}
	return *a == *b;
}

static int has_audio_ext(const char *name)
{
	const char *dot = strrchr(name, '.');
	if (!dot) return 0;
	if (str_iequal(dot, ".mp3")) return 1;
	if (str_iequal(dot, ".ogg")) return 1;
	if (str_iequal(dot, ".flac")) return 1;
	return 0;
}

static int cmp_strings(const void *a, const void *b)
{
	return strcmp((const char *) a, (const char *) b);
}

static int join_path(char *out, size_t out_size, const char *dir,
	const char *name)
{
	size_t dir_len = strlen(dir);
	size_t name_len = strlen(name);

	if (dir_len + 1 + name_len >= out_size)
		return -1;
	memcpy(out, dir, dir_len);
	out[dir_len] = PATH_SEP;
	memcpy(out + dir_len + 1, name, name_len + 1);
	return 0;
}

static int collect_audio_files(const char *dir)
{
	s_file_count = 0;

#ifdef _WIN32
	char pattern[MAX_PATH_LEN];
	WIN32_FIND_DATAA fd;
	HANDLE h;

	snprintf(pattern, sizeof(pattern), "%s\\*", dir);
	h = FindFirstFileA(pattern, &fd);
	if (h == INVALID_HANDLE_VALUE) return 0;

	do {
		if (fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY) continue;
		if (!has_audio_ext(fd.cFileName)) continue;
		if (s_file_count >= MAX_FILES) break;
		strncpy(s_filenames[s_file_count], fd.cFileName, MAX_PATH_LEN - 1);
		s_filenames[s_file_count][MAX_PATH_LEN - 1] = '\0';
		s_file_count++;
	} while (FindNextFileA(h, &fd));
	FindClose(h);
#else
	DIR *d = opendir(dir);
	struct dirent *ent;
	if (!d) return 0;

	while ((ent = readdir(d)) != NULL) {
		if (ent->d_type == DT_DIR) continue;
		if (!has_audio_ext(ent->d_name)) continue;
		if (s_file_count >= MAX_FILES) break;
		strncpy(s_filenames[s_file_count], ent->d_name, MAX_PATH_LEN - 1);
		s_filenames[s_file_count][MAX_PATH_LEN - 1] = '\0';
		s_file_count++;
	}
	closedir(d);
#endif

	qsort(s_filenames, s_file_count, MAX_PATH_LEN, cmp_strings);
	return s_file_count;
}

/* Escape a string for JSON output */
static void json_escape(const char *src, char *dst, int dst_len)
{
	int i = 0;
	while (*src && i < dst_len - 2) {
		if (*src == '"' || *src == '\\') dst[i++] = '\\';
		dst[i++] = *src++;
	}
	dst[i] = '\0';
}

int main(int argc, char *argv[])
{
	char full_path[MAX_PATH_LEN];
	int errors = 0;
	int i;
	int printed = 0;

	if (argc < 2) {
		fprintf(stderr, "Usage: fingerprint_audio <directory>\n");
		return 1;
	}

	const char *dir = argv[1];
	int count = collect_audio_files(dir);
	if (count == 0) {
		fprintf(stderr, "No audio files found in: %s\n", dir);
		printf("[]\n");
		return 0;
	}

	fprintf(stderr, "Found %d audio files in %s\n", count, dir);

	printf("[\n");
	for (i = 0; i < count; i++) {
		char escaped[MAX_PATH_LEN * 2];
		json_escape(s_filenames[i], escaped, sizeof(escaped));

		fprintf(stderr, "  [%d/%d] %s ...", i + 1, count, s_filenames[i]);
		if (join_path(full_path, sizeof(full_path), dir, s_filenames[i]) < 0) {
			fprintf(stderr, " PATH TOO LONG\n");
			errors++;
			continue;
		}

		fingerprint_result_t fp = { 0 };
		int rc = fingerprint_from_audio_file(full_path, &fp);

		if (rc == 0 && fp.encoded) {
			if (printed > 0)
				printf(",\n");
			printf("  {\"filename\": \"%s\", \"chromaprint\": \"%s\", \"duration_ms\": %d}",
			       escaped, fp.encoded, fp.duration_ms);
			printed++;
			fprintf(stderr, " %dms\n", fp.duration_ms);
		} else {
			fprintf(stderr, " FAILED\n");
			errors++;
		}
		fingerprint_free(&fp);
	}
	if (printed > 0)
		printf("\n");
	printf("]\n");

	fprintf(stderr, "\nDone: %d files, %d errors\n", count, errors);
	return errors > 0 ? 1 : 0;
}
