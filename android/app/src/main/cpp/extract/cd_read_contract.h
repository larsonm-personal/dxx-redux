#ifndef DXX_CD_READ_CONTRACT_H
#define DXX_CD_READ_CONTRACT_H

#include <stddef.h>

#define CD_CUE_MAX_BYTES (1024 * 1024)

int cd_read_file_exact(const char *path, size_t max_bytes, char **data, size_t *length);
int cd_track_span(int start_sector, int num_sectors, long long file_size,
                  long long *byte_offset, long long *byte_length);
int cd_track_span_with_stride(int start_sector, int num_sectors, int sector_size,
                              long long file_size, long long *byte_offset,
                              long long *byte_length);

#endif
