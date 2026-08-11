#include "cd_read_contract.h"

#include <stdio.h>
#include <stdlib.h>

#define CD_SECTOR_BYTES 2352LL

int cd_read_file_exact(const char *path, size_t max_bytes, char **data, size_t *length)
{
	FILE *file;
	long file_length;
	char *buffer;

	if (!path || !data || max_bytes == 0) return 0;
	*data = NULL;
	if (length) *length = 0;
	file = fopen(path, "rb");
	if (!file) return 0;
	if (fseek(file, 0, SEEK_END) != 0 || (file_length = ftell(file)) <= 0 ||
	    (unsigned long long) file_length > (unsigned long long) max_bytes ||
	    fseek(file, 0, SEEK_SET) != 0) {
		fclose(file);
		return 0;
	}
	buffer = (char *) malloc((size_t) file_length + 1);
	if (!buffer) {
		fclose(file);
		return 0;
	}
	if (fread(buffer, 1, (size_t) file_length, file) != (size_t) file_length ||
	    fgetc(file) != EOF || ferror(file)) {
		free(buffer);
		fclose(file);
		return 0;
	}
	buffer[file_length] = '\0';
	fclose(file);
	*data = buffer;
	if (length) *length = (size_t) file_length;
	return 1;
}

int cd_track_span(int start_sector, int num_sectors, long long file_size,
                  long long *byte_offset, long long *byte_length)
{
	return cd_track_span_with_stride(start_sector, num_sectors, CD_SECTOR_BYTES,
	                                 file_size, byte_offset, byte_length);
}

int cd_track_span_with_stride(int start_sector, int num_sectors, int sector_size,
                              long long file_size, long long *byte_offset,
                              long long *byte_length)
{
	long long offset;
	long long length;

	if (start_sector < 0 || num_sectors <= 0 || sector_size <= 0 || file_size < 0)
		return 0;
	offset = (long long) start_sector * sector_size;
	length = (long long) num_sectors * sector_size;
	if (offset > file_size || length > file_size - offset) return 0;
	if (byte_offset) *byte_offset = offset;
	if (byte_length) *byte_length = length;
	return 1;
}
