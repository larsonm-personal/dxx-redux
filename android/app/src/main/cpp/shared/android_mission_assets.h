#ifndef ANDROID_MISSION_ASSETS_H
#define ANDROID_MISSION_ASSETS_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

void android_mission_assets_init(void);
void android_mission_assets_shutdown(void);
/* Preflight and begin teardown, returning the selected descriptor's real virtual path */
int android_mission_assets_prepare(const char *descriptor, char *resolved, size_t capacity);
int android_mission_assets_activate(void);
void android_mission_assets_free_begin(void);
void android_mission_assets_free_end(void);
int android_mission_assets_discover(const char *descriptor);
const char *android_mission_assets_owner(void);
const char *android_mission_assets_key(void);
int android_mission_assets_resolve_key(const char *key, char *mission_path, size_t capacity);
unsigned android_mission_assets_generation(void);

/* Engine-specific data lifetime hooks, implemented in android_mission_asset_reset.c */
void android_mission_asset_reset_before(void);
void android_mission_asset_reset_baseline(void);

#ifdef __cplusplus
}
#endif
#endif
