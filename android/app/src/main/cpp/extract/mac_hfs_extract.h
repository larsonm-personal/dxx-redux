#ifndef DXX_ANDROID_EXTRACT_MAC_HFS_EXTRACT_H
#define DXX_ANDROID_EXTRACT_MAC_HFS_EXTRACT_H

#include "extract_attempt_budget.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef int (*extract_progress_fn)(const char *current_file,
                                   long long bytes_done,
                                   long long bytes_total,
                                   void *user_data);

int mac_extract_files_from_hfs_track(int bin_fd, int track_start_sector, int track_num_sectors,
                                     const char *output_dir,
                                     const char **sti2_extensions,
                                     const char **hfs_extensions,
                                     extract_progress_fn progress,
                                     void *user_data);

int mac_extract_files_from_hfs_track_with_budget(
    int bin_fd, int track_start_sector, int track_num_sectors,
    const char *output_dir, const char **sti2_extensions,
    const char **hfs_extensions, extract_progress_fn progress,
    void *user_data, dxx_extract_attempt_budget_t *budget);

#ifdef __cplusplus
}
#endif

#endif
