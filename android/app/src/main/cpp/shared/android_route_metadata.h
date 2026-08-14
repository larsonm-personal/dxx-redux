#ifndef DXX_ANDROID_ROUTE_METADATA_H
#define DXX_ANDROID_ROUTE_METADATA_H

#include "android_route_metadata_progress_policy.h"

void android_route_metadata_request(
    const char *game,
    const char *mission,
    int level_num,
    const char *level_file,
    const char *const *normal_level_files,
    int normal_level_count,
    const char *const *secret_level_files,
    const int *secret_entry_levels,
    int secret_level_count);
void android_route_metadata_invalidate_pending(void);
int android_route_metadata_get_progress_permille(void);
int android_route_metadata_get_progress_state(void);
int android_route_metadata_get_request_generation(void);

#endif
