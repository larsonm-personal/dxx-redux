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
	input_demo_result parsed;
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
	result.has_game_time64 = 1;
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
		"  \"version\": 2,\n" +
		"  \"game\": \"d2\",\n" +
		"  \"mission\": \"d2\",\n" +
		"  \"level\": 1,\n" +
		"  \"difficulty\": 2,\n" +
		"  \"frame_count\": 3,\n" +
		"  \"game_time64\": 120,\n" +
		"  \"player0\": {\n" +
		"    \"energy\": 67,\n" +
		"    \"shields\": 42,\n" +
		"    \"score\": 12500,\n" +
		"    \"lives\": 3,\n" +
		"    \"laser_level\": 1,\n" +
		"    \"primary_weapon\": 0,\n" +
		"    \"secondary_weapon\": 1,\n" +
		"    \"flags\": 0,\n" +
		"    \"hostages\": 0,\n" +
		"    \"primary_ammo\": [\n" +
		"      0,\n" +
		"      200,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0\n" +
		"    ],\n" +
		"    \"secondary_ammo\": [\n" +
		"      4,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0,\n" +
		"      0\n" +
		"    ]\n" +
		"  },\n" +
		"  \"position\": {\n" +
		"    \"segment\": 142,\n" +
		"    \"x\": 12345678,\n" +
		"    \"y\": -8765432,\n" +
		"    \"z\": 3456789,\n" +
		"    \"forward_x\": 65536,\n" +
		"    \"forward_y\": 0,\n" +
		"    \"forward_z\": 0\n" +
		"  },\n" +
		"  \"level_summary\": {\n" +
		"    \"robots_alive\": 23,\n" +
		"    \"robots_killed\": 8,\n" +
		"    \"hostages_remaining\": 2,\n" +
		"    \"powerups_remaining\": 15,\n" +
		"    \"control_center_destroyed\": true,\n" +
		"    \"endlevel_completed\": false\n" +
		"  }\n" +
		"}\n";
	if (text != expected) {
		remove(result_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("unexpected result output: ") + text);
	}
	if (!input_demo_result_read_json_file(result_path.c_str(), &parsed, error, sizeof(error))) {
		remove(result_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("result read failed: ") + error);
	}
	if (!input_demo_result_compare(&result, &parsed, error, sizeof(error))) {
		remove(result_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("result round-trip compare failed: ") + error);
	}
	remove(result_path.c_str());
	remove_test_dir(dir);
	return 0;
}

static int expect_result_compare(void)
{
	const char *dir = "test_input_demo_result_compare_fixture";
	const std::string expected_path = std::string(dir) + "/expected.json";
	const std::string actual_path = std::string(dir) + "/actual.json";
	input_demo_result expected;
	input_demo_result actual;
	char error[256] = "";

	if (!make_test_dir(dir))
		return report_failure("could not create result compare test directory");

	input_demo_result_clear(&expected);
	snprintf(expected.game, sizeof(expected.game), "%s", "d2");
	snprintf(expected.mission, sizeof(expected.mission), "%s", "d2");
	expected.level = 1;
	expected.difficulty = 2;
	expected.frame_count = 3;
	if (!input_demo_result_write_json_file(expected_path.c_str(), &expected, error, sizeof(error))) {
		remove_test_dir(dir);
		return report_failure_string(std::string("minimal expected result write failed: ") + error);
	}

	input_demo_result_clear(&actual);
	snprintf(actual.game, sizeof(actual.game), "%s", "d2");
	snprintf(actual.mission, sizeof(actual.mission), "%s", "d2");
	actual.level = 1;
	actual.difficulty = 2;
	actual.frame_count = 3;
	actual.has_game_time64 = 1;
	actual.game_time64 = 120;
	actual.player0.present = 1;
	actual.player0.score = 12500;
	if (!input_demo_result_write_json_file(actual_path.c_str(), &actual, error, sizeof(error))) {
		remove(expected_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("actual result write failed: ") + error);
	}
	if (!input_demo_result_compare_files(expected_path.c_str(), actual_path.c_str(), error, sizeof(error))) {
		remove(expected_path.c_str());
		remove(actual_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("baseline-driven compare unexpectedly failed: ") + error);
	}

	expected.player0.present = 1;
	expected.player0.score = 12500;
	if (!input_demo_result_write_json_file(expected_path.c_str(), &expected, error, sizeof(error))) {
		remove(expected_path.c_str());
		remove(actual_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("richer expected result write failed: ") + error);
	}
	actual.player0.score = 12501;
	if (!input_demo_result_write_json_file(actual_path.c_str(), &actual, error, sizeof(error))) {
		remove(expected_path.c_str());
		remove(actual_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("mismatch actual result write failed: ") + error);
	}
	if (input_demo_result_compare_files(expected_path.c_str(), actual_path.c_str(), error, sizeof(error))) {
		remove(expected_path.c_str());
		remove(actual_path.c_str());
		remove_test_dir(dir);
		return report_failure("result compare unexpectedly passed a score mismatch");
	}
	if (!strstr(error, "player0.score")) {
		remove(expected_path.c_str());
		remove(actual_path.c_str());
		remove_test_dir(dir);
		return report_failure_string(std::string("result compare mismatch label missing: ") + error);
	}

	remove(expected_path.c_str());
	remove(actual_path.c_str());
	remove_test_dir(dir);
	return 0;
}

int main(void)
{
	if (expect_result_writer())
		return 1;
	if (expect_result_compare())
		return 1;
	puts("PASS");
	return 0;
}