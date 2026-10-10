#ifndef CHECKPOINT_FILE_H
#define CHECKPOINT_FILE_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Absolute capture-time path; no engine or PhysFS globals. Flush a private
 * staging file before replacing the destination, preserving it on failure */
int checkpoint_file_publish(const char *path, const void *data, size_t size);
/* A null companion removes any previous companion as part of the transaction */
int checkpoint_file_publish_pair(const char *path, const void *data, size_t size,
                                 const char *companion_path, const void *companion, size_t companion_size);

#ifdef __cplusplus
}
#endif
#endif
