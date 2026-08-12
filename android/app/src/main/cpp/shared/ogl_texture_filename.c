#include "ogl_texture_filename.h"

#include <string.h>

int android_ogl_texture_filename(char *filename, size_t filename_capacity,
                                 const char *basename, const char *extension)
{
	size_t basename_length;
	size_t extension_length;

	if (!filename || !filename_capacity || !basename || !extension)
		return 0;
	basename_length = strlen(basename);
	extension_length = strlen(extension);
	if (basename_length > filename_capacity - 1 ||
	    extension_length > filename_capacity - 1 - basename_length)
		return 0;
	memcpy(filename, basename, basename_length);
	memcpy(filename + basename_length, extension, extension_length + 1);
	return 1;
}
