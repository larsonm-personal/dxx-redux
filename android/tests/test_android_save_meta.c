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
	android_save_meta_disk abort_meta;
	android_save_meta_disk progress_meta;
	android_save_meta_disk wrong_thumb_meta;
	android_save_meta_disk parsed;
	android_save_meta_candidate newest;
	android_save_meta_write_params params;
	uint8_t thumb_a[ANDROID_SAVE_META_THUMB_RGB6_BYTES];
	uint8_t thumb_b[ANDROID_SAVE_META_THUMB_RGB6_BYTES];
	const char *paths[6];
	FILE *corrupt;
	int failures = 0;

	memset(thumb_a, 7, sizeof(thumb_a));
	memset(thumb_b, 19, sizeof(thumb_b));
	if (ANDROID_SAVE_META_THUMB_W != 200 || ANDROID_SAVE_META_THUMB_H != 100)
		failures += report_failure("launcher metadata thumbnail is not 200x100");

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
	params.thumbnail_width = ANDROID_SAVE_META_THUMB_W / 2;
	params.thumbnail_height = ANDROID_SAVE_META_THUMB_H / 2;
	if (!android_save_meta_build(&wrong_thumb_meta, &params))
		return report_failure("failed to build wrong-size thumbnail metadata");
	if (wrong_thumb_meta.thumbnail_format != ANDROID_SAVE_META_THUMB_NONE)
		failures += report_failure("wrong-size thumbnail was embedded in metadata");

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
	abort_meta = d2_meta;
	abort_meta.save_kind = ANDROID_SAVE_META_KIND_AUTO_ABORT;
	memset(abort_meta.description, 0, sizeof(abort_meta.description));
	strncpy(abort_meta.description, "AUTO ABORT", sizeof(abort_meta.description) - 1);

	if (!write_file_with_meta("test_android_save_meta_d1.sav", &d1_meta))
		return report_failure("failed to write D1 test file");
	if (!write_file_with_meta("test_android_save_meta_d2.sav", &d2_meta))
		return report_failure("failed to write D2 test file");
	if (!write_file_with_meta("test_android_save_meta_abort.sav", &abort_meta))
		return report_failure("failed to write abort test file");
	progress_meta = d2_meta;
	progress_meta.save_kind = ANDROID_SAVE_META_KIND_AUTO_PROGRESS;
	memset(progress_meta.description, 0, sizeof(progress_meta.description));
	strncpy(progress_meta.description, "AUTO SAVE", sizeof(progress_meta.description) - 1);
	if (!write_file_with_meta("test_android_save_meta_progress.sav", &progress_meta))
		return report_failure("failed to write progress test file");
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
	wrong_thumb_meta = d1_meta;
	wrong_thumb_meta.thumbnail_width = ANDROID_SAVE_META_THUMB_W / 2;
	wrong_thumb_meta.thumbnail_height = ANDROID_SAVE_META_THUMB_H / 2;
	if (android_save_meta_is_valid(&wrong_thumb_meta))
		failures += report_failure("wrong-size RGB thumbnail metadata was accepted");

	paths[0] = "test_android_save_meta_missing.sav";
	paths[1] = "test_android_save_meta_d1.sav";
	paths[2] = "test_android_save_meta_corrupt.sav";
	paths[3] = "test_android_save_meta_d2.sav";
	paths[4] = "test_android_save_meta_progress.sav";
	paths[5] = "test_android_save_meta_abort.sav";
	if (!android_save_meta_select_newest(paths, 6, &newest))
		failures += report_failure("failed to select newest metadata-backed save");
	else {
		failures += expect_string("newest path", "test_android_save_meta_abort.sav", newest.path);
		failures += expect_string("newest callsign", "zen", newest.meta.callsign);
		if (newest.meta.save_kind != ANDROID_SAVE_META_KIND_AUTO_ABORT)
			failures += report_failure("newest candidate kept the wrong save kind");
		if (newest.meta.total_seconds != 2222)
			failures += report_failure("newest candidate kept the wrong total seconds");
	}

	remove("test_android_save_meta_d1.sav");
	remove("test_android_save_meta_d2.sav");
	remove("test_android_save_meta_abort.sav");
	remove("test_android_save_meta_progress.sav");
	remove("test_android_save_meta_missing.sav");
	remove("test_android_save_meta_corrupt.sav");

	if (failures)
		return 1;

	puts("PASS: android save metadata trailer");
	return 0;
}
