#include <stdio.h>
#include <string.h>

#include "input_demo_replay.h"
#include "input_demo_rng_mode.h"

static const char *find_arg_value(int argc, char *argv[], const char *name)
{
	for (int index = 1; index + 1 < argc; ++index)
		if (!strcmp(argv[index], name))
			return argv[index + 1];
	return NULL;
}

int main(int argc, char *argv[])
{
	char error[256] = "";
	int demo_mode = 0;
	int engine_mode = d_rand_get_replay_mode();
	const char *demo_path = find_arg_value(argc, argv, "-inputdemo-replay");
	const char *validation_error;

	if (!demo_path) {
		fprintf(stderr, "usage: %s -inputdemo-replay <demo.dximdemo>\n", argc > 0 ? argv[0] : "dxx-redux-d2-headless");
		return 1;
	}

	validation_error = input_demo_rng_mode_validate_metadata_file(demo_path, engine_mode, &demo_mode);
	if (validation_error) {
		fprintf(stderr, "HEADLESS-SCAFFOLD FAIL %s\n", validation_error);
		return 1;
	}
	if (!input_demo_replay_load(demo_path, error, sizeof(error))) {
		fprintf(stderr, "HEADLESS-SCAFFOLD FAIL %s\n", error[0] ? error : "replay load failed");
		return 1;
	}
	if (input_demo_replay_game() != INPUT_DEMO_GAME_D2) {
		fprintf(stderr, "HEADLESS-SCAFFOLD FAIL expected d2 demo\n");
		input_demo_replay_unload();
		return 1;
	}

	printf("HEADLESS-SCAFFOLD OK game=d2 mission=%s level=%d difficulty=%d start_mode=%s frames=%u rng_mode=%s\n",
	       input_demo_replay_mission() ? input_demo_replay_mission() : "",
	       input_demo_replay_level(),
	       input_demo_replay_difficulty(),
	       input_demo_replay_start_mode() ? input_demo_replay_start_mode() : "",
	       (unsigned int) input_demo_replay_frame_count(),
	       input_demo_rng_mode_name(demo_mode));
	input_demo_replay_unload();
	return 0;
}