#ifndef DXX_ANDROID_EXTRACT_STUFFIT_EXTRACT_H
#define DXX_ANDROID_EXTRACT_STUFFIT_EXTRACT_H

#include "sti2_extract.h"

#ifdef __cplusplus
extern "C" {
#endif

int stuffit_extract(const char *sit_path, const char *output_dir,
                    const char **extensions,
                    sti2_progress_fn progress, void *user_data);

#ifdef STUFFIT_EXTRACT_TESTING
int stuffit_test_sit5_list_entries(const unsigned char *archive_data,
                                   size_t archive_size);
#endif

#ifdef __cplusplus
}
#endif

#endif /* DXX_ANDROID_EXTRACT_STUFFIT_EXTRACT_H */
