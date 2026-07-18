#ifndef DXX_ANDROID_LEVEL_PREVIEW_H
#define DXX_ANDROID_LEVEL_PREVIEW_H

#ifdef __cplusplus
extern "C" {
#endif

const char *android_level_preview_request_path(void);
int android_level_preview_run(const char *request_path);
const char *android_level_preview_last_error(void);
int android_level_preview_active(void);
void android_level_preview_request_close(void);
const char *android_level_preview_introspection_json(void);

#ifdef __cplusplus
}
#endif

#endif
