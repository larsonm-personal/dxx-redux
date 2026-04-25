#include "inno_reader.h"

#include <stdio.h>
#include <string.h>

#ifdef _WIN32
#include <fcntl.h>
#include <io.h>
#define OPEN_RB(path) _open((path), _O_RDONLY | _O_BINARY)
#define CLOSE_FD(fd)  _close(fd)
#define ci_cmp        _stricmp
#else
#include <fcntl.h>
#include <strings.h>
#include <unistd.h>
#define OPEN_RB(path) open((path), O_RDONLY)
#define CLOSE_FD(fd)  close(fd)
#define ci_cmp        strcasecmp
#endif

static const char *game_extensions[] = {
	".hog", ".pig", ".ham", ".s11", ".s22", ".dem",
	".mvl", ".msn", ".mn2", ".gog", ".inst",
	NULL
};

static int has_game_extension(const char *path)
{
	const char *dot = strrchr(path, '.');
	if (!dot) return 0;
	for (const char **ext = game_extensions; *ext; ext++) {
		if (ci_cmp(dot, *ext) == 0) return 1;
	}
	return 0;
}

static const char *basename_only(const char *path)
{
	const char *last = path;
	for (const char *p = path; *p; p++) {
		if (*p == '/' || *p == '\\') last = p + 1;
	}
	return last;
}

static int contains_file(inno_archive_t *arc, const char *expected)
{
	for (int i = 0; i < arc->file_count; i++) {
		if (!has_game_extension(arc->files[i].destination)) continue;
		if (ci_cmp(basename_only(arc->files[i].destination), expected) == 0)
			return 1;
	}
	return 0;
}

static int check_installer(const char *path, const char **expected, int expected_count,
                           int expected_game_count)
{
	int fd = OPEN_RB(path);
	if (fd < 0) {
		printf("skip missing fixture: %s\n", path);
		return 0;
	}

	inno_archive_t arc;
	int n = inno_open_fd(fd, &arc);
	CLOSE_FD(fd);
	if (n < 0) {
		fprintf(stderr, "failed to open fd fixture: %s\n", path);
		return 1;
	}

	int game_count = 0;
	int failures = 0;
	for (int i = 0; i < arc.file_count; i++) {
		if (has_game_extension(arc.files[i].destination)) game_count++;
	}
	if (game_count != expected_game_count) {
		fprintf(stderr, "%s: expected %d game files, got %d\n",
		        path, expected_game_count, game_count);
		failures++;
	}
	for (int i = 0; i < expected_count; i++) {
		if (!contains_file(&arc, expected[i])) {
			fprintf(stderr, "%s: missing %s\n", path, expected[i]);
			failures++;
		}
	}
	inno_close(&arc);
	return failures ? 1 : 0;
}

int main(int argc, char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "usage: test_gog_fd <d1 installer> <d2 installer>\n");
		return 1;
	}

	const char *d1_expected[] = { "DESCENT.HOG", "DESCENT.PIG" };
	const char *d2_expected[] = {
		"DESCENT2.HAM", "DESCENT2.HOG", "DESCENT2.S11",
		"DESCENT2.S22", "GROUPA.PIG"
	};

	int failures = 0;
	failures += check_installer(argv[1], d1_expected, 2, 7);
	failures += check_installer(argv[2], d2_expected, 5, 21);
	return failures ? 1 : 0;
}