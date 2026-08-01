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
#include <errno.h>
#include <limits.h>

#ifdef _WIN32
#include <io.h>
#include <windows.h>
#else
#include <dirent.h>
#define PATH_SEP '/'
#endif

#include "fingerprint_gen.h"
#include "json_writer.h"
#include "pcm_decoders.h"

/* Maximum files we'll handle in one directory */
#define MAX_FILES 4096

typedef struct {
	char *name;
#ifdef _WIN32
	wchar_t *wide_name;
#endif
} audio_file_t;

/* Filename collection for sorting */
static audio_file_t s_files[MAX_FILES];
static int s_file_count = 0;

enum {
	COLLECT_ERROR = -1,
	COLLECT_CAPACITY_EXCEEDED = -2
};

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
	const audio_file_t *left = (const audio_file_t *) a;
	const audio_file_t *right = (const audio_file_t *) b;
	return strcmp(left->name, right->name);
}

static void clear_audio_files(void)
{
	for (int i = 0; i < s_file_count; i++) {
		free(s_files[i].name);
#ifdef _WIN32
		free(s_files[i].wide_name);
#endif
	}
	memset(s_files, 0, sizeof(s_files));
	s_file_count = 0;
}

#ifdef FINGERPRINT_AUDIO_TESTING
static int should_inject_enumeration_error(void)
{
	const char *raw = getenv("DXX_FINGERPRINT_AUDIO_TEST_FAIL_AFTER");
	if (!raw || !*raw) return 0;
	char *end = NULL;
	errno = 0;
	long fail_after = strtol(raw, &end, 10);
	return errno == 0 && end != raw && *end == '\0' &&
	       fail_after >= 0 && s_file_count >= fail_after;
}
#endif

#ifdef _WIN32
static char *wide_to_utf8(const wchar_t *value)
{
	int length = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1,
	                                 NULL, 0, NULL, NULL);
	if (length <= 0) return NULL;
	char *utf8 = (char *) malloc((size_t) length);
	if (!utf8 ||
	    WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, value, -1, utf8,
	                        length, NULL, NULL) != length) {
		free(utf8);
		return NULL;
	}
	return utf8;
}

static int add_audio_file(const wchar_t *name)
{
	if (s_file_count >= MAX_FILES) return COLLECT_CAPACITY_EXCEEDED;
	char *utf8_name = wide_to_utf8(name);
	size_t wide_bytes = (wcslen(name) + 1) * sizeof(wchar_t);
	wchar_t *wide_name = (wchar_t *) malloc(wide_bytes);
	if (!utf8_name || !wide_name) {
		free(utf8_name);
		free(wide_name);
		return COLLECT_ERROR;
	}
	memcpy(wide_name, name, wide_bytes);
	s_files[s_file_count].name = utf8_name;
	s_files[s_file_count].wide_name = wide_name;
	s_file_count++;
	return 0;
}

static int collect_audio_files(const wchar_t *dir)
{
	size_t dir_len = wcslen(dir);
	if (dir_len > (SIZE_MAX / sizeof(wchar_t)) - 3) return COLLECT_ERROR;
	wchar_t *pattern =
	    (wchar_t *) malloc((dir_len + 3) * sizeof(wchar_t));
	if (!pattern) return COLLECT_ERROR;
	memcpy(pattern, dir, dir_len * sizeof(wchar_t));
	pattern[dir_len] = L'\\';
	pattern[dir_len + 1] = L'*';
	pattern[dir_len + 2] = L'\0';

	WIN32_FIND_DATAW fd;
	HANDLE handle = FindFirstFileW(pattern, &fd);
	free(pattern);
	if (handle == INVALID_HANDLE_VALUE) {
		fwprintf(stderr, L"Directory enumeration failed for %ls (error %lu)\n",
		         dir, GetLastError());
		return COLLECT_ERROR;
	}

	int result = 0;
	for (;;) {
		if (!(fd.dwFileAttributes & FILE_ATTRIBUTE_DIRECTORY)) {
			char *utf8_name = wide_to_utf8(fd.cFileName);
			if (!utf8_name) {
				result = COLLECT_ERROR;
				break;
			}
			int is_audio = has_audio_ext(utf8_name);
			free(utf8_name);
			if (is_audio && (result = add_audio_file(fd.cFileName)) != 0)
				break;
		}
#ifdef FINGERPRINT_AUDIO_TESTING
		if (should_inject_enumeration_error()) {
			fprintf(stderr, "Injected directory enumeration failure\n");
			result = COLLECT_ERROR;
			break;
		}
#endif
		if (!FindNextFileW(handle, &fd)) {
			DWORD error = GetLastError();
			if (error != ERROR_NO_MORE_FILES) {
				fprintf(stderr, "Directory enumeration failed (error %lu)\n",
				        error);
				result = COLLECT_ERROR;
			}
			break;
		}
	}
	if (!FindClose(handle) && result == 0) {
		fprintf(stderr, "Directory enumeration close failed (error %lu)\n",
		        GetLastError());
		result = COLLECT_ERROR;
	}
	if (result == COLLECT_CAPACITY_EXCEEDED)
		fprintf(stderr, "Directory contains more than %d audio files\n",
		        MAX_FILES);
	if (result != 0) {
		clear_audio_files();
		return result;
	}

	qsort(s_files, (size_t) s_file_count, sizeof(s_files[0]), cmp_strings);
	return s_file_count;
}

static wchar_t *join_wide_path(const wchar_t *dir, const wchar_t *name)
{
	size_t dir_len = wcslen(dir);
	size_t name_len = wcslen(name);
	if (dir_len > (SIZE_MAX / sizeof(wchar_t)) - name_len - 2) return NULL;
	wchar_t *path =
	    (wchar_t *) malloc((dir_len + name_len + 2) * sizeof(wchar_t));
	if (!path) return NULL;
	memcpy(path, dir, dir_len * sizeof(wchar_t));
	path[dir_len] = L'\\';
	memcpy(path + dir_len + 1, name, (name_len + 1) * sizeof(wchar_t));
	return path;
}

static int fingerprint_audio_file(const wchar_t *dir, const audio_file_t *file,
                                  fingerprint_result_t *out)
{
	int result = -1;
	wchar_t *path = join_wide_path(dir, file->wide_name);
	FILE *input = path ? _wfopen(path, L"rb") : NULL;
	free(path);
	if (!input) return -1;

	if (_fseeki64(input, 0, SEEK_END) != 0) goto done;
	__int64 length = _ftelli64(input);
	if (length <= 0 || length > INT_MAX ||
	    _fseeki64(input, 0, SEEK_SET) != 0)
		goto done;

	unsigned char *data = (unsigned char *) malloc((size_t) length);
	if (!data) goto done;
	if (fread(data, 1, (size_t) length, input) != (size_t) length) {
		free(data);
		goto done;
	}

	pcm_decode_result_t pcm = { 0 };
	const char *extension = strrchr(file->name, '.');
	if (extension &&
	    pcm_decode_memory_prefix(data, (size_t) length, extension,
	                             FINGERPRINT_MAX_SECONDS, &pcm) == 0) {
		result = fingerprint_from_pcm_prefix(pcm.pcm_data, pcm.pcm_samples,
		                                     pcm.total_samples, pcm.sample_rate,
		                                     pcm.channels, out);
		pcm_decode_free(&pcm);
	}
	free(data);
done:
	if (fclose(input) != 0) result = -1;
	return result;
}
#else
static int add_audio_file(const char *name)
{
	if (s_file_count >= MAX_FILES) return COLLECT_CAPACITY_EXCEEDED;
	char *copy = strdup(name);
	if (!copy) return COLLECT_ERROR;
	s_files[s_file_count++].name = copy;
	return 0;
}

static int collect_audio_files(const char *dir)
{
	DIR *directory = opendir(dir);
	if (!directory) {
		fprintf(stderr, "Directory enumeration failed for %s: %s\n", dir,
		        strerror(errno));
		return COLLECT_ERROR;
	}

	int result = 0;
	for (;;) {
		errno = 0;
		struct dirent *entry = readdir(directory);
		if (!entry) {
			if (errno != 0) {
				fprintf(stderr, "Directory enumeration failed for %s: %s\n",
				        dir, strerror(errno));
				result = COLLECT_ERROR;
			}
			break;
		}
		if (entry->d_type == DT_DIR || !has_audio_ext(entry->d_name))
			continue;
		result = add_audio_file(entry->d_name);
		if (result != 0) break;
#ifdef FINGERPRINT_AUDIO_TESTING
		if (should_inject_enumeration_error()) {
			fprintf(stderr, "Injected directory enumeration failure\n");
			result = COLLECT_ERROR;
			break;
		}
#endif
	}
	if (closedir(directory) != 0 && result == 0) {
		fprintf(stderr, "Directory enumeration close failed for %s: %s\n",
		        dir, strerror(errno));
		result = COLLECT_ERROR;
	}
	if (result == COLLECT_CAPACITY_EXCEEDED)
		fprintf(stderr, "Directory contains more than %d audio files\n",
		        MAX_FILES);
	if (result != 0) {
		clear_audio_files();
		return result;
	}

	qsort(s_files, (size_t) s_file_count, sizeof(s_files[0]), cmp_strings);
	return s_file_count;
}

static char *join_path(const char *dir, const char *name)
{
	size_t dir_len = strlen(dir);
	size_t name_len = strlen(name);
	if (dir_len > SIZE_MAX - name_len - 2) return NULL;
	char *path = (char *) malloc(dir_len + name_len + 2);
	if (!path) return NULL;
	memcpy(path, dir, dir_len);
	path[dir_len] = PATH_SEP;
	memcpy(path + dir_len + 1, name, name_len + 1);
	return path;
}

static int fingerprint_audio_file(const char *dir, const audio_file_t *file,
                                  fingerprint_result_t *out)
{
	char *path = join_path(dir, file->name);
	if (!path) return -1;
	int result = fingerprint_from_audio_file(path, out);
	free(path);
	return result;
}
#endif

/* Escape a string for JSON output */
static void json_escape(FILE *output, const char *src)
{
	json_write_string(output, src);
}

#ifdef _WIN32
int wmain(int argc, wchar_t *argv[])
#else
int main(int argc, char *argv[])
#endif
{
	int errors = 0;
	int i;
	int printed = 0;

	if (argc < 2) {
#ifdef _WIN32
		fwprintf(stderr, L"Usage: fingerprint_audio <directory>\n");
#else
		fprintf(stderr, "Usage: fingerprint_audio <directory>\n");
#endif
		return 1;
	}

	const
#ifdef _WIN32
	    wchar_t
#else
	    char
#endif
	        *dir = argv[1];
	int count = collect_audio_files(dir);
	if (count < 0) return 1;
	if (count == 0) {
#ifdef _WIN32
		fwprintf(stderr, L"No audio files found in: %ls\n", dir);
#else
		fprintf(stderr, "No audio files found in: %s\n", dir);
#endif
		printf("[]\n");
		return 0;
	}

#ifdef _WIN32
	fwprintf(stderr, L"Found %d audio files in %ls\n", count, dir);
#else
	fprintf(stderr, "Found %d audio files in %s\n", count, dir);
#endif

	printf("[\n");
	for (i = 0; i < count; i++) {
		fprintf(stderr, "  [%d/%d] %s ...", i + 1, count, s_files[i].name);

		fingerprint_result_t fp = { 0 };
		int rc = fingerprint_audio_file(dir, &s_files[i], &fp);

		if (rc == 0 && fp.encoded) {
			if (printed > 0)
				printf(",\n");
			printf("  {\"filename\": ");
			json_escape(stdout, s_files[i].name);
			printf(", \"chromaprint\": ");
			json_escape(stdout, fp.encoded);
			printf(", \"duration_ms\": %d}", fp.duration_ms);
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
	clear_audio_files();
	return errors > 0 ? 1 : 0;
}
