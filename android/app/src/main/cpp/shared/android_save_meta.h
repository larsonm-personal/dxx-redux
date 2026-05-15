#ifndef ANDROID_SAVE_META_H
#define ANDROID_SAVE_META_H

#include <stdint.h>
#include <physfs.h>

#define ANDROID_SAVE_META_TAG     0x44584153u /* "DXAS" */
#define ANDROID_SAVE_META_VERSION 1

#define ANDROID_SAVE_META_CALLSIGN_LEN     8
#define ANDROID_SAVE_META_DESC_LEN         20
#define ANDROID_SAVE_META_MISSION_LEN      8
#define ANDROID_SAVE_META_LEVEL_NAME_LEN   36
#define ANDROID_SAVE_META_THUMB_W          100
#define ANDROID_SAVE_META_THUMB_H          50
#define ANDROID_SAVE_META_THUMB_RGB6_BYTES (ANDROID_SAVE_META_THUMB_W * ANDROID_SAVE_META_THUMB_H * 3)
#define ANDROID_SAVE_META_PATH_LEN         1024

enum {
	ANDROID_SAVE_META_GAME_D1 = 1,
	ANDROID_SAVE_META_GAME_D2 = 2
};

enum {
	ANDROID_SAVE_META_KIND_MANUAL = 0,
	ANDROID_SAVE_META_KIND_AUTO_MINIMIZE = 1,
	ANDROID_SAVE_META_KIND_AUTO_EXIT = 2
};

enum {
	ANDROID_SAVE_META_THUMB_NONE = 0,
	ANDROID_SAVE_META_THUMB_RGB6 = 1
};

typedef struct android_save_meta_write_params {
	uint8_t game_id;
	uint8_t save_kind;
	uint64_t wall_clock_unix_seconds;
	const char *callsign;
	const char *description;
	const char *mission_name;
	int level_num;
	const char *level_name;
	uint32_t level_seconds;
	uint32_t total_seconds;
	const uint8_t *thumbnail_rgb6;
	uint16_t thumbnail_width;
	uint16_t thumbnail_height;
} android_save_meta_write_params;

#pragma pack(push, 1)
typedef struct android_save_meta_footer {
	uint32_t tag;
	uint16_t version;
	uint16_t trailer_bytes;
} android_save_meta_footer;

typedef struct android_save_meta_disk {
	uint8_t game_id;
	uint8_t save_kind;
	uint8_t thumbnail_format;
	uint8_t reserved0;
	uint16_t thumbnail_width;
	uint16_t thumbnail_height;
	uint64_t wall_clock_unix_seconds;
	char callsign[ANDROID_SAVE_META_CALLSIGN_LEN + 1];
	char description[ANDROID_SAVE_META_DESC_LEN + 1];
	char mission_name[ANDROID_SAVE_META_MISSION_LEN + 1];
	int32_t level_num;
	char level_name[ANDROID_SAVE_META_LEVEL_NAME_LEN];
	uint32_t level_seconds;
	uint32_t total_seconds;
	uint8_t thumbnail_rgb6[ANDROID_SAVE_META_THUMB_RGB6_BYTES];
	android_save_meta_footer footer;
} android_save_meta_disk;
#pragma pack(pop)

typedef struct android_save_meta_candidate {
	char path[ANDROID_SAVE_META_PATH_LEN];
	android_save_meta_disk meta;
} android_save_meta_candidate;

int android_save_meta_build(android_save_meta_disk *out,
                            const android_save_meta_write_params *params);
int android_save_meta_is_valid(const android_save_meta_disk *meta);
int android_save_meta_read_path(const char *path, android_save_meta_disk *out);
int android_save_meta_write_physfs(PHYSFS_file *fp,
                                   const android_save_meta_write_params *params);
int android_save_meta_read_physfs(PHYSFS_file *fp, PHYSFS_sint64 file_len,
                                  android_save_meta_disk *out);
int android_save_meta_select_newest(const char *const *paths, int path_count,
                                    android_save_meta_candidate *out);

#endif