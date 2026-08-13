#ifndef DXX_PHYSICAL_OUTPUT_FILE_H
#define DXX_PHYSICAL_OUTPUT_FILE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
	int fd;
	int parent_fd;
	void *handle;
	char leaf[256];
} dxx_physical_output_file_t;

/* Open one validated relative path beneath an existing physical directory
 * without following filesystem links in the root, parents, or final leaf */
int dxx_physical_output_open(dxx_physical_output_file_t *file,
                             const char *output_dir,
                             const char *relative_path);

int dxx_physical_output_write(dxx_physical_output_file_t *file,
                              const void *data, size_t size);

/* Finish preserves the file while abort removes the exact opened identity */
int dxx_physical_output_finish(dxx_physical_output_file_t *file);
void dxx_physical_output_abort(dxx_physical_output_file_t *file);

#ifdef __cplusplus
}
#endif

#endif
