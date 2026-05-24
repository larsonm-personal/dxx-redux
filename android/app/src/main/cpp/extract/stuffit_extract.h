#ifndef DXX_ANDROID_EXTRACT_STUFFIT_EXTRACT_H
#define DXX_ANDROID_EXTRACT_STUFFIT_EXTRACT_H

#include "sti2_extract.h"

#ifdef __cplusplus
extern "C" {
#endif

int stuffit_extract(const char *sit_path, const char *output_dir,
                    const char **extensions,
                    sti2_progress_fn progress, void *user_data);

#ifdef __cplusplus
}
#endif

#endif /* DXX_ANDROID_EXTRACT_STUFFIT_EXTRACT_H */