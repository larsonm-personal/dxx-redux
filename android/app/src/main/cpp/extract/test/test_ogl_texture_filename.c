#include "ogl_texture_filename.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                            \
	do {                                                                            \
		if (!(condition)) {                                                         \
			fprintf(stderr, "check failed at line %d: %s\n", __LINE__, #condition); \
			return 1;                                                               \
		}                                                                           \
	} while (0)

int main(void)
{
	char exact[8] = "keep";
	char short_buffer[7] = "keep";
	char zero_capacity[8] = "keep";
	char empty_basename[5] = "keep";
	char empty_extension[5] = "keep";
	char dxa_ordinary[18] = "keep";
	char dxa_exact_basename[247];
	char dxa_exact_filename[256] = "keep";
	char dxa_over_basename[248];
	char dxa_over_filename[256] = "keep";

	CHECK(android_ogl_texture_filename(exact, sizeof(exact), "foo", ".png"));
	CHECK(strcmp(exact, "foo.png") == 0);
	CHECK(!android_ogl_texture_filename(short_buffer, sizeof(short_buffer), "foo", ".png"));
	CHECK(strcmp(short_buffer, "keep") == 0);
	CHECK(!android_ogl_texture_filename(zero_capacity, 0, "foo", ".png"));
	CHECK(strcmp(zero_capacity, "keep") == 0);
	CHECK(android_ogl_texture_filename(empty_basename, sizeof(empty_basename), "", ".png"));
	CHECK(strcmp(empty_basename, ".png") == 0);
	CHECK(android_ogl_texture_filename(empty_extension, sizeof(empty_extension), "base", ""));
	CHECK(strcmp(empty_extension, "base") == 0);

	CHECK(android_ogl_texture_filename(dxa_ordinary, sizeof(dxa_ordinary),
	                                   "level01", "_mask.png"));
	CHECK(strcmp(dxa_ordinary, "level01_mask.png") == 0);
	memset(dxa_exact_basename, 'a', sizeof(dxa_exact_basename) - 1);
	dxa_exact_basename[sizeof(dxa_exact_basename) - 1] = '\0';
	CHECK(android_ogl_texture_filename(dxa_exact_filename,
	                                   sizeof(dxa_exact_filename),
	                                   dxa_exact_basename, "_mask.png"));
	CHECK(strlen(dxa_exact_filename) == sizeof(dxa_exact_filename) - 1);
	CHECK(strcmp(dxa_exact_filename + sizeof(dxa_exact_basename) - 1,
	             "_mask.png") == 0);
	memset(dxa_over_basename, 'b', sizeof(dxa_over_basename) - 1);
	dxa_over_basename[sizeof(dxa_over_basename) - 1] = '\0';
	CHECK(!android_ogl_texture_filename(dxa_over_filename,
	                                    sizeof(dxa_over_filename),
	                                    dxa_over_basename, "_mask.png"));
	CHECK(strcmp(dxa_over_filename, "keep") == 0);
	return 0;
}
