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
/* Explicit synchronous saves share the worker's recovery protocol */
int checkpoint_file_publish_staged_pair(const char *stage, const char *path,
                                        const char *companion_stage, const char *companion_path, int companion_present);
int checkpoint_file_recover_pair(const char *path, const char *companion_path);
/* Save-set slot naming stays shared with the launcher and engine */
int checkpoint_file_recover_save(const char *path);
void checkpoint_file_recover_directory(const char *directory);

#ifdef __cplusplus
}
#endif
#endif
