#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#if defined(_WIN32)
#include <direct.h>
#else
#include <sys/stat.h>
#include <unistd.h>
#endif

#include <string>

#include "input_demo_result.h"

static int report_failure(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

static int report_failure_string(const std::string &message)
{
	return report_failure(message.c_str());
}

static bool read_text_file(const char *path, std::string *text)
{
	FILE *f = fopen(path, "rb");
	char buffer[256];
	size_t bytes;

	if (!f)
		return false;
	text->clear();
	while ((bytes = fread(buffer, 1, sizeof(buffer), f)) != 0)
		text->append(buffer, bytes);
	fclose(f);
	return true;
}

static int make_test_dir(const char *path)
{
#if defined(_WIN32)
	if (_mkdir(path) == 0 || errno == EEXIST)
		return 1;
#else
	if (mkdir(path, 0777) == 0 || errno == EEXIST)
		return 1;
#endif
	return 0;
}

static void remove_test_dir(const char *path)
{
#if defined(_WIN32)
	_rmdir(path);
#else
	rmdir(path);
#endif
}

static int expect_result_writer(void)
{
	const char *dir = "test_input_demo_result_fixture";
	const std::string result_path = std::string(dir) + "/result.json";
	input_demo_result result;
	char error[256] = "";
	std::string text;
	std::string expected;

	if (!make_test_dir(dir))
		return report_failure("could not create result test directory");
	input_demo_result_clear(&result);
	snprintf(result.game, sizeof(result.game), "%s", "d2");
	snprintf(result.mission, sizeof(result.mission), "%s", "d2");
	result.level = 1;
	result.difficulty = 2;
	result.frame_count = 3;
	result.game_time64 = 120;
	result.player0.present = 1;
	result.player0.energy = 67;
	result.player0.shields = 42;
	result.player0.score = 12500;
	result.player0.lives = 3;
	result.player0.laser_level = 1;
	result.player0.secondary_weapon = 1;
	result.player0.primary_ammo[1] = 200;
	result.player0.secondary_ammo[0] = 4;
	result.position.present = 1;
	result.position.segment = 142;
	result.position.x = 12345678;
	result.position.y = -8765432;
	result.position.z = 3456789;
	result.position.has_forward = 1;
	result.position.fx = 65536;
	result.level_summary.present = 1;
	result.level_summary.robots_alive = 23;
	result.level_summary.robots_killed = 8;
	result.level_summary.hostages_remaining = 2;
	result.level_summary.powerups_remaining = 15;
	result.level_summary.control_center_destroyed = 1;
	if (!input_demo_result_write_json_file(result_path.c_str(), &result, error, sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("result write failed: ") + error);
	}
	if (!read_text_file(result_path.c_str(), &text)) {
		remove_test_dir(dir);
		return report_failure("could not read result output");
	}
	expected = std::string("{\n") +
		"  \"v\": 1,\n" +
		"  \"g\": \"d2\",\n" +
		"  \"m\": \"d2\",\n" +
		"  \"l\": 1,\n" +
		"  \"d\": 2,\n" +
		"  \"fr\": 3,\n" +
		"  \"gt\": 120,\n" +
		"  \"p0\": {\n" +
		"    \"e\": 67,\n" +
		"    \"s\": 42,\n" +
		"    \"sc\": 12500,\n" +
		"    \"li\": 3,\n" +
		"    \"ll\": 1,\n" +
		"    \"sw\": 1,\n" +
		"    \"pa\": {\n" +
		"      \"1\": 200\n" +
		"    },\n" +
		"    \"sa\": {\n" +
		"      \"0\": 4\n" +
		"    }\n" +
		"  },\n" +
		"  \"pos\": {\n" +
		"    \"sg\": 142,\n" +
		"    \"x\": 12345678,\n" +
		"    \"y\": -8765432,\n" +
		"    \"z\": 3456789,\n" +
		"    \"fx\": 65536,\n" +
		"    \"fy\": 0,\n" +
		"    \"fz\": 0\n" +
		"  },\n" +
		"  \"lv\": {\n" +
		"    \"ra\": 23,\n" +
		"    \"rk\": 8,\n" +
		"    \"hr\": 2,\n" +
		"    \"pr\": 15,\n" +
		"    \"cc\": true\n" +
		"  }\n" +
		"}\n";
	if (text != expected) {
		remove(result_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("unexpected result output: ") + text);
	}
	remove(result_path.c_str());
	remove_test_dir(dir);
	return 0;
}

int main(void)
{
	if (expect_result_writer())
		return 1;
	puts("PASS");
	return 0;
}