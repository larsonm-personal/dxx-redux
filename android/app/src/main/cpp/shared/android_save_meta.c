#include "android_save_meta.h"

#include <stdio.h>
#include <string.h>
#include <time.h>

static uint8_t g_cached_thumbnail_rgb6[ANDROID_SAVE_META_THUMB_RGB6_BYTES];
static int g_cached_thumbnail_valid = 0;

void android_save_meta_clear_cached_thumbnail(void)
{
	g_cached_thumbnail_valid = 0;
}

void android_save_meta_set_cached_thumbnail_rgb6(const uint8_t *rgb6,
                                                 uint16_t width,
                                                 uint16_t height)
{
	if (!rgb6 ||
	    width != ANDROID_SAVE_META_THUMB_W ||
	    height != ANDROID_SAVE_META_THUMB_H) {
		android_save_meta_clear_cached_thumbnail();
		return;
	}
	memcpy(g_cached_thumbnail_rgb6, rgb6, sizeof(g_cached_thumbnail_rgb6));
	g_cached_thumbnail_valid = 1;
}

void android_save_meta_apply_cached_thumbnail(android_save_meta_write_params *params)
{
	if (!params || !g_cached_thumbnail_valid)
		return;
	params->thumbnail_rgb6 = g_cached_thumbnail_rgb6;
	params->thumbnail_width = ANDROID_SAVE_META_THUMB_W;
	params->thumbnail_height = ANDROID_SAVE_META_THUMB_H;
}

static void android_save_meta_copy_string(char *dst, int dst_size, const char *src)
{
	if (!dst || dst_size <= 0)
		return;

	memset(dst, 0, dst_size);
	if (!src)
		return;
	strncpy(dst, src, (size_t) (dst_size - 1));
}

static void android_save_meta_sanitize(android_save_meta_disk *meta)
{
	meta->callsign[ANDROID_SAVE_META_CALLSIGN_LEN] = '\0';
	meta->description[ANDROID_SAVE_META_DESC_LEN] = '\0';
	meta->mission_name[ANDROID_SAVE_META_MISSION_LEN] = '\0';
	meta->level_name[ANDROID_SAVE_META_LEVEL_NAME_LEN - 1] = '\0';
}

static int android_save_meta_path_precedes(const char *lhs, const char *rhs)
{
	if (!lhs)
		return 0;
	if (!rhs)
		return 1;
	return strcmp(lhs, rhs) < 0;
}

static int android_save_meta_kind_priority(uint8_t save_kind)
{
	switch (save_kind) {
		case ANDROID_SAVE_META_KIND_AUTO_ABORT:
			return 5;
		case ANDROID_SAVE_META_KIND_AUTO_EXIT:
			return 4;
		case ANDROID_SAVE_META_KIND_AUTO_MINIMIZE:
			return 3;
		case ANDROID_SAVE_META_KIND_AUTO_PROGRESS:
			return 2;
		default:
			return 1;
	}
}

static int android_save_meta_is_newer(const char *path,
                                      const android_save_meta_disk *meta,
                                      const android_save_meta_candidate *best)
{
	int priority, best_priority;

	if (meta->wall_clock_unix_seconds != best->meta.wall_clock_unix_seconds)
		return meta->wall_clock_unix_seconds > best->meta.wall_clock_unix_seconds;
	priority = android_save_meta_kind_priority(meta->save_kind);
	best_priority = android_save_meta_kind_priority(best->meta.save_kind);
	if (priority != best_priority)
		return priority > best_priority;
	return android_save_meta_path_precedes(path, best->path);
}

int android_save_meta_build(android_save_meta_disk *out,
                            const android_save_meta_write_params *params)
{
	if (!out || !params)
		return 0;
	if (params->game_id != ANDROID_SAVE_META_GAME_D1 &&
	    params->game_id != ANDROID_SAVE_META_GAME_D2)
		return 0;
	if (params->save_kind > ANDROID_SAVE_META_KIND_AUTO_ABORT)
		return 0;

	memset(out, 0, sizeof(*out));
	out->game_id = params->game_id;
	out->save_kind = params->save_kind;
	out->wall_clock_unix_seconds = params->wall_clock_unix_seconds ? params->wall_clock_unix_seconds : (uint64_t) time(NULL);
	android_save_meta_copy_string(out->callsign, sizeof(out->callsign), params->callsign);
	android_save_meta_copy_string(out->description, sizeof(out->description), params->description);
	android_save_meta_copy_string(out->mission_name, sizeof(out->mission_name), params->mission_name);
	android_save_meta_copy_string(out->level_name, sizeof(out->level_name), params->level_name);
	out->level_num = params->level_num;
	out->level_seconds = params->level_seconds;
	out->total_seconds = params->total_seconds;
	if (params->thumbnail_rgb6 &&
	    params->thumbnail_width == ANDROID_SAVE_META_THUMB_W &&
	    params->thumbnail_height == ANDROID_SAVE_META_THUMB_H) {
		out->thumbnail_format = ANDROID_SAVE_META_THUMB_RGB6;
		out->thumbnail_width = params->thumbnail_width;
		out->thumbnail_height = params->thumbnail_height;
		memcpy(out->thumbnail_rgb6, params->thumbnail_rgb6, sizeof(out->thumbnail_rgb6));
	}
	out->footer.tag = ANDROID_SAVE_META_TAG;
	out->footer.version = ANDROID_SAVE_META_VERSION;
	out->footer.trailer_bytes = (uint16_t) sizeof(*out);
	android_save_meta_sanitize(out);
	return 1;
}

int android_save_meta_is_valid(const android_save_meta_disk *meta)
{
	if (!meta)
		return 0;
	if (meta->footer.tag != ANDROID_SAVE_META_TAG)
		return 0;
	if (meta->footer.version != ANDROID_SAVE_META_VERSION)
		return 0;
	if (meta->footer.trailer_bytes != sizeof(*meta))
		return 0;
	if (meta->game_id != ANDROID_SAVE_META_GAME_D1 &&
	    meta->game_id != ANDROID_SAVE_META_GAME_D2)
		return 0;
	if (meta->save_kind > ANDROID_SAVE_META_KIND_AUTO_ABORT)
		return 0;
	if (meta->thumbnail_format == ANDROID_SAVE_META_THUMB_NONE)
		return 1;
	if (meta->thumbnail_format != ANDROID_SAVE_META_THUMB_RGB6)
		return 0;
	if (meta->thumbnail_width != ANDROID_SAVE_META_THUMB_W)
		return 0;
	if (meta->thumbnail_height != ANDROID_SAVE_META_THUMB_H)
		return 0;
	return 1;
}

int android_save_meta_write_physfs(PHYSFS_file *fp,
                                   const android_save_meta_write_params *params)
{
	android_save_meta_disk meta;

	if (!fp || !params)
		return 0;
	if (!android_save_meta_build(&meta, params))
		return 0;
	return PHYSFS_write(fp, &meta, sizeof(meta), 1) == 1;
}

int android_save_meta_read_physfs(PHYSFS_file *fp, PHYSFS_sint64 file_len,
                                  android_save_meta_disk *out)
{
	PHYSFS_sint64 saved_pos;
	PHYSFS_sint64 meta_start;
	android_save_meta_footer footer;

	if (!fp || !out || file_len < (PHYSFS_sint64) sizeof(footer))
		return 0;

	saved_pos = PHYSFS_tell(fp);
	meta_start = file_len - (PHYSFS_sint64) sizeof(footer);
	if (!PHYSFS_seek(fp, meta_start))
		goto fail;
	if (PHYSFS_read(fp, &footer, sizeof(footer), 1) != 1)
		goto fail;
	if (footer.tag != ANDROID_SAVE_META_TAG ||
	    footer.version != ANDROID_SAVE_META_VERSION ||
	    footer.trailer_bytes != sizeof(*out) ||
	    file_len < (PHYSFS_sint64) footer.trailer_bytes)
		goto fail;
	meta_start = file_len - (PHYSFS_sint64) footer.trailer_bytes;
	if (!PHYSFS_seek(fp, meta_start))
		goto fail;
	if (PHYSFS_read(fp, out, sizeof(*out), 1) != 1)
		goto fail;
	if (!PHYSFS_seek(fp, saved_pos))
		return 0;
	return android_save_meta_is_valid(out);

fail:
	PHYSFS_seek(fp, saved_pos);
	return 0;
}

int android_save_meta_read_path(const char *path, android_save_meta_disk *out)
{
	FILE *f;
	long file_len;
	android_save_meta_footer footer;

	if (!path || !out)
		return 0;

	f = fopen(path, "rb");
	if (!f)
		return 0;
	if (fseek(f, 0, SEEK_END) != 0) {
		fclose(f);
		return 0;
	}
	file_len = ftell(f);
	if (file_len < (long) sizeof(footer)) {
		fclose(f);
		return 0;
	}
	if (fseek(f, file_len - (long) sizeof(footer), SEEK_SET) != 0) {
		fclose(f);
		return 0;
	}
	if (fread(&footer, sizeof(footer), 1, f) != 1) {
		fclose(f);
		return 0;
	}
	if (footer.tag != ANDROID_SAVE_META_TAG ||
	    footer.version != ANDROID_SAVE_META_VERSION ||
	    footer.trailer_bytes != sizeof(*out) ||
	    file_len < (long) footer.trailer_bytes) {
		fclose(f);
		return 0;
	}
	if (fseek(f, file_len - (long) footer.trailer_bytes, SEEK_SET) != 0) {
		fclose(f);
		return 0;
	}
	if (fread(out, sizeof(*out), 1, f) != 1) {
		fclose(f);
		return 0;
	}
	fclose(f);
	android_save_meta_sanitize(out);
	return android_save_meta_is_valid(out);
}

int android_save_meta_select_newest(const char *const *paths, int path_count,
                                    android_save_meta_candidate *out)
{
	android_save_meta_candidate best;
	int found = 0;
	int i;

	if (!paths || path_count < 0 || !out)
		return 0;

	memset(&best, 0, sizeof(best));
	for (i = 0; i < path_count; i++) {
		android_save_meta_disk meta;

		if (!android_save_meta_read_path(paths[i], &meta))
			continue;
		if (!found || android_save_meta_is_newer(paths[i], &meta, &best)) {
			memset(&best, 0, sizeof(best));
			strncpy(best.path, paths[i], sizeof(best.path) - 1);
			best.meta = meta;
			found = 1;
		}
	}
	if (!found)
		return 0;
	*out = best;
	return 1;
}
