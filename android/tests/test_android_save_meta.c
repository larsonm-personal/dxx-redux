#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "android_save_meta.h"

static int report_failure(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

static int write_file_with_meta(const char *path, const android_save_meta_disk *meta)
{
	FILE *f = fopen(path, "wb");
	const char base_bytes[] = "base-save-body";

	if (!f)
		return 0;
	if (fwrite(base_bytes, 1, sizeof(base_bytes) - 1, f) != sizeof(base_bytes) - 1) {
		fclose(f);
		return 0;
	}
	if (meta && fwrite(meta, sizeof(*meta), 1, f) != 1) {
		fclose(f);
		return 0;
	}
	fclose(f);
	return 1;
}

static int expect_string(const char *label, const char *expected, const char *actual)
{
	if (strcmp(expected, actual) == 0)
		return 0;
	fprintf(stderr, "%s expected '%s' got '%s'\n", label, expected, actual);
	return 1;
}

int main(void)
{
	android_save_meta_disk d1_meta;
	android_save_meta_disk d2_meta;
	android_save_meta_disk parsed;
	android_save_meta_candidate newest;
	android_save_meta_write_params params;
	uint8_t thumb_a[ANDROID_SAVE_META_THUMB_RGB6_BYTES];
	uint8_t thumb_b[ANDROID_SAVE_META_THUMB_RGB6_BYTES];
	const char *paths[4];
	FILE *corrupt;
	int failures = 0;

	memset(thumb_a, 7, sizeof(thumb_a));
	memset(thumb_b, 19, sizeof(thumb_b));

	memset(&params, 0, sizeof(params));
	params.game_id = ANDROID_SAVE_META_GAME_D1;
	params.save_kind = ANDROID_SAVE_META_KIND_MANUAL;
	params.wall_clock_unix_seconds = 100;
	params.callsign = "ace";
	params.description = "AUTO SAVE";
	params.mission_name = "descent";
	params.level_num = 3;
	params.level_name = "Lunar Outpost";
	params.level_seconds = 123;
	params.total_seconds = 456;
	params.thumbnail_rgb6 = thumb_a;
	params.thumbnail_width = ANDROID_SAVE_META_THUMB_W;
	params.thumbnail_height = ANDROID_SAVE_META_THUMB_H;
	if (!android_save_meta_build(&d1_meta, &params))
		return report_failure("failed to build D1 metadata");

	memset(&params, 0, sizeof(params));
	params.game_id = ANDROID_SAVE_META_GAME_D2;
	params.save_kind = ANDROID_SAVE_META_KIND_AUTO_EXIT;
	params.wall_clock_unix_seconds = 200;
	params.callsign = "zen";
	params.description = "AUTO EXIT";
	params.mission_name = "d2";
	params.level_num = 9;
	params.level_name = "Fueling Center";
	params.level_seconds = 999;
	params.total_seconds = 2222;
	params.thumbnail_rgb6 = thumb_b;
	params.thumbnail_width = ANDROID_SAVE_META_THUMB_W;
	params.thumbnail_height = ANDROID_SAVE_META_THUMB_H;
	if (!android_save_meta_build(&d2_meta, &params))
		return report_failure("failed to build D2 metadata");

	if (!write_file_with_meta("test_android_save_meta_d1.sav", &d1_meta))
		return report_failure("failed to write D1 test file");
	if (!write_file_with_meta("test_android_save_meta_d2.sav", &d2_meta))
		return report_failure("failed to write D2 test file");
	if (!write_file_with_meta("test_android_save_meta_missing.sav", NULL))
		return report_failure("failed to write missing-metadata file");

	corrupt = fopen("test_android_save_meta_corrupt.sav", "wb");
	if (!corrupt)
		return report_failure("failed to create corrupt metadata file");
	if (fwrite("base-save-body", 1, 14, corrupt) != 14) {
		fclose(corrupt);
		return report_failure("failed to seed corrupt metadata file");
	}
	d2_meta.footer.tag = 0;
	if (fwrite(&d2_meta, sizeof(d2_meta), 1, corrupt) != 1) {
		fclose(corrupt);
		return report_failure("failed to write corrupt metadata file");
	}
	fclose(corrupt);
	d2_meta.footer.tag = ANDROID_SAVE_META_TAG;

	if (!android_save_meta_read_path("test_android_save_meta_d1.sav", &parsed))
		return report_failure("could not parse D1 metadata trailer");
	failures += expect_string("callsign", "ace", parsed.callsign);
	failures += expect_string("description", "AUTO SAVE", parsed.description);
	failures += expect_string("level name", "Lunar Outpost", parsed.level_name);
	if (parsed.game_id != ANDROID_SAVE_META_GAME_D1)
		failures += report_failure("parsed wrong game id for D1 trailer");
	if (parsed.thumbnail_format != ANDROID_SAVE_META_THUMB_RGB6)
		failures += report_failure("thumbnail format missing on D1 trailer");
	if (parsed.thumbnail_rgb6[0] != 7)
		failures += report_failure("thumbnail payload did not round-trip");

	paths[0] = "test_android_save_meta_missing.sav";
	paths[1] = "test_android_save_meta_d1.sav";
	paths[2] = "test_android_save_meta_corrupt.sav";
	paths[3] = "test_android_save_meta_d2.sav";
	if (!android_save_meta_select_newest(paths, 4, &newest))
		failures += report_failure("failed to select newest metadata-backed save");
	else {
		failures += expect_string("newest path", "test_android_save_meta_d2.sav", newest.path);
		failures += expect_string("newest callsign", "zen", newest.meta.callsign);
		if (newest.meta.save_kind != ANDROID_SAVE_META_KIND_AUTO_EXIT)
			failures += report_failure("newest candidate kept the wrong save kind");
		if (newest.meta.total_seconds != 2222)
			failures += report_failure("newest candidate kept the wrong total seconds");
	}

	remove("test_android_save_meta_d1.sav");
	remove("test_android_save_meta_d2.sav");
	remove("test_android_save_meta_missing.sav");
	remove("test_android_save_meta_corrupt.sav");

	if (failures)
		return 1;

	puts("PASS: android save metadata trailer");
	return 0;
}