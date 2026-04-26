#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "input_demo_rng_mode.h"

static int report_failure(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

static int write_text_file(const char *path, const char *text)
{
	FILE *f = fopen(path, "wb");
	if (!f)
		return 0;
	if (fwrite(text, 1, strlen(text), f) != strlen(text))
	{
		fclose(f);
		return 0;
	}
	fclose(f);
	return 1;
}

int main(void)
{
	const char *matching_mode_name;
	const char *mismatching_mode_name;
	const char *matching_demo;
	const char *missing_demo = "{\"type\":\"header\",\"version\":1}\n";
	const char *legacy_demo = "{\"type\":\"header\",\"rng_mode\":\"per_frame_seed\"}\n";
	const char *demo_path = "test_input_demo_rng_mode.dximdemo";
	const char *error;
	int engine_mode = d_rand_get_replay_mode();
	int parsed_mode = 0;

	if (input_demo_rng_mode_parse("lcg_state") != D_RAND_REPLAY_MODE_LCG_STATE)
		return report_failure("failed to parse lcg_state rng_mode");
	if (input_demo_rng_mode_parse("libc_reseed") != D_RAND_REPLAY_MODE_LIBC_RESEED)
		return report_failure("failed to parse libc_reseed rng_mode");
	if (input_demo_rng_mode_parse("output_log") != D_RAND_REPLAY_MODE_OUTPUT_LOG)
		return report_failure("failed to parse output_log rng_mode");
	if (input_demo_rng_mode_parse("per_frame_seed") != 0)
		return report_failure("unexpectedly accepted legacy per_frame_seed rng_mode");
	if (strcmp(input_demo_rng_mode_name(D_RAND_REPLAY_MODE_LCG_STATE), "lcg_state"))
		return report_failure("wrong lcg_state name");
	if (strcmp(input_demo_rng_mode_name(D_RAND_REPLAY_MODE_LIBC_RESEED), "libc_reseed"))
		return report_failure("wrong libc_reseed name");
	if (strcmp(input_demo_rng_mode_name(D_RAND_REPLAY_MODE_OUTPUT_LOG), "output_log"))
		return report_failure("wrong output_log name");
	if (strcmp(input_demo_rng_mode_name(0), "invalid"))
		return report_failure("wrong invalid rng_mode name");
	if (input_demo_rng_mode_is_compatible(D_RAND_REPLAY_MODE_OUTPUT_LOG, engine_mode))
		return report_failure("output_log rng_mode unexpectedly marked compatible");
	matching_mode_name = input_demo_rng_mode_name(engine_mode);
	mismatching_mode_name = engine_mode == D_RAND_REPLAY_MODE_LCG_STATE ?
		"libc_reseed" : "lcg_state";
	matching_demo = engine_mode == D_RAND_REPLAY_MODE_LCG_STATE ?
		"{\"type\":\"header\",\"rng_mode\":\"lcg_state\"}\n" :
		"{\"type\":\"header\",\"rng_mode\":\"libc_reseed\"}\n";
	error = input_demo_rng_mode_parse_metadata_text(matching_demo, &parsed_mode);
	if (error)
		return report_failure(error);
	if (parsed_mode != engine_mode)
		return report_failure("demo text parsed the wrong rng_mode");
	error = input_demo_rng_mode_validate_metadata_text(matching_demo, engine_mode,
		&parsed_mode);
	if (error)
		return report_failure(error);
	error = input_demo_rng_mode_validate_metadata_text(missing_demo, engine_mode,
		&parsed_mode);
	if (!error)
		return report_failure("missing rng_mode demo unexpectedly validated");
	error = input_demo_rng_mode_validate_metadata_text(legacy_demo, engine_mode,
		&parsed_mode);
	if (!error)
		return report_failure("legacy per_frame_seed demo unexpectedly validated");
	if (!write_text_file(demo_path, matching_demo))
		return report_failure("could not write demo probe file");
	error = input_demo_rng_mode_validate_metadata_file(demo_path, engine_mode,
		&parsed_mode);
	remove(demo_path);
	if (error)
		return report_failure(error);
	if (strcmp(matching_mode_name, mismatching_mode_name) == 0)
		return report_failure("matching and mismatching rng_mode names collapsed");

#ifdef NO_WATCOM_RAND
	if (engine_mode != D_RAND_REPLAY_MODE_LIBC_RESEED)
		return report_failure("NO_WATCOM_RAND reported the wrong engine replay mode");
	if (!input_demo_rng_mode_is_compatible(D_RAND_REPLAY_MODE_LIBC_RESEED, engine_mode))
		return report_failure("libc_reseed rng_mode should match NO_WATCOM_RAND builds");
	if (input_demo_rng_mode_is_compatible(D_RAND_REPLAY_MODE_LCG_STATE, engine_mode))
		return report_failure("lcg_state rng_mode should not match NO_WATCOM_RAND builds");

	puts("PASS: input demo rng_mode matches libc reseed backend");
#else
	if (engine_mode != D_RAND_REPLAY_MODE_LCG_STATE)
		return report_failure("LCG build reported the wrong engine replay mode");
	if (!input_demo_rng_mode_is_compatible(D_RAND_REPLAY_MODE_LCG_STATE, engine_mode))
		return report_failure("lcg_state rng_mode should match LCG builds");
	if (input_demo_rng_mode_is_compatible(D_RAND_REPLAY_MODE_LIBC_RESEED, engine_mode))
		return report_failure("libc_reseed rng_mode should not match LCG builds");

	puts("PASS: input demo rng_mode matches LCG backend");
#endif

	return 0;
}