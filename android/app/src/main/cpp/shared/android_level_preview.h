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
void android_robot_preview_request_close(void);
void android_robot_preview_rotate(int heading_delta, int pitch_delta);
void android_robot_preview_reset(void);
int android_robot_preview_select(int direction);
void android_robot_preview_set_sounds(int enabled);
void android_robot_preview_set_attack(int enabled);
const char *android_robot_preview_attack_summary(void);

#ifdef __cplusplus
}
#endif

#endif
