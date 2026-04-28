#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#include "input_demo_fixture.h"
#include "input_demo_rng_mode.h"

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

static const char *input_demo_test_game_name(void)
{
#if defined(INPUT_DEMO_TEST_D2)
	return "d2";
#else
	return "d1";
#endif
}

static int expect_rng_coalescer(void)
{
	std::vector<input_demo_rng_frame> frames(6);
	std::vector<input_demo_rng_record> records;
	std::string line0;
	std::string line1;
	std::string line2;
	std::string error;
	size_t i;

	for (i = 0; i != frames.size(); ++i) {
		input_demo_rng_frame_clear(&frames[i]);
		frames[i].frame = (uint32_t) i;
	}
	frames[0].state = 305419896u;
	frames[1].state = 305419896u;
	frames[2].state = 305419896u;
	frames[3].state = 305420112u;
	frames[3].has_call_count = 1;
	frames[3].call_count = 3;
	frames[4].state = 305420112u;
	frames[5].state = 305420112u;
	if (!input_demo_rng_records_coalesce_frames(frames, &records, &error))
		return report_failure_string(std::string("rng coalescer failed: ") + error);
	if (records.size() != 3)
		return report_failure("rng coalescer did not produce three records");
	if (!input_demo_rng_record_to_json_line(records[0], &line0, &error) ||
		!input_demo_rng_record_to_json_line(records[1], &line1, &error) ||
		!input_demo_rng_record_to_json_line(records[2], &line2, &error))
		return report_failure_string(std::string("rng line write failed: ") + error);
	if (line0 != "{\"f\":0,\"n\":3,\"s\":305419896}")
		return report_failure_string(std::string("unexpected rng line 0: ") + line0);
	if (line1 != "{\"f\":3,\"s\":305420112,\"c\":3}")
		return report_failure_string(std::string("unexpected rng line 1: ") + line1);
	if (line2 != "{\"f\":4,\"n\":2,\"s\":305420112}")
		return report_failure_string(std::string("unexpected rng line 2: ") + line2);
	return 0;
}

static int expect_rng_file_round_trip(void)
{
	const char *path = "test_input_demo_rng.jsonl";
	std::vector<input_demo_rng_record> records;
	std::vector<input_demo_rng_record> parsed_records;
	std::string error;
	input_demo_rng_record record0;
	input_demo_rng_record record1;

	input_demo_rng_record_clear(&record0);
	record0.frame = 0;
	record0.run_length = 2;
	record0.state = 99;
	input_demo_rng_record_clear(&record1);
	record1.frame = 2;
	record1.state = 123;
	record1.has_call_count = 1;
	record1.call_count = 7;
	records.push_back(record0);
	records.push_back(record1);
	if (!input_demo_rng_records_write_jsonl_file(path, records, &error))
		return report_failure_string(std::string("rng file write failed: ") + error);
	if (!input_demo_rng_records_read_jsonl_file(path, &parsed_records, &error)) {
		remove(path);
		return report_failure_string(std::string("rng file read failed: ") + error);
	}
	remove(path);
	if (parsed_records.size() != 2)
		return report_failure("rng file round trip produced the wrong record count");
	if (parsed_records[0].frame != 0 || parsed_records[0].run_length != 2 ||
		parsed_records[0].state != 99 || parsed_records[0].has_call_count)
		return report_failure("rng file round trip corrupted first record");
	if (parsed_records[1].frame != 2 || parsed_records[1].run_length != 1 ||
		parsed_records[1].state != 123 || !parsed_records[1].has_call_count ||
		parsed_records[1].call_count != 7)
		return report_failure("rng file round trip corrupted second record");
	return 0;
}

static int expect_demo_file_output(void)
{
	const char *path = "test_input_demo_fixture.dximdemo";
	const char *rng_mode = input_demo_rng_mode_name(d_rand_get_replay_mode());
	input_demo_file demo;
	input_demo_file parsed;
	std::string text;
	std::string file_text;
	std::string expected;
	std::string error;

	demo.metadata.version = 1;
	demo.metadata.game = input_demo_test_game_name();
	demo.metadata.mission = input_demo_test_game_name();
	demo.metadata.level = 1;
	demo.metadata.difficulty = 2;
	demo.metadata.start_mode = "new_level";
	demo.metadata.rng_mode = rng_mode;
	demo.metadata.frame_count = 2;
	demo.frames.resize(2);
	input_demo_control_record_clear(&demo.frames[0].input);
	demo.frames[0].input.frame = 0;
	demo.frames[0].input.has_frame_time = 1;
	demo.frames[0].input.frame_time = 3276;
	demo.frames[0].input.held.has_forward_thrust_time = 1;
	demo.frames[0].input.held.forward_thrust_time = 44;
	input_demo_rng_record_clear(&demo.frames[0].rng);
	demo.frames[0].rng.frame = 0;
	demo.frames[0].rng.state = 100;
	input_demo_control_record_clear(&demo.frames[1].input);
	demo.frames[1].input.frame = 1;
	demo.frames[1].input.pulse.has_fire_primary_count = 1;
	demo.frames[1].input.pulse.fire_primary_count = 1;
	input_demo_rng_record_clear(&demo.frames[1].rng);
	demo.frames[1].rng.frame = 1;
	demo.frames[1].rng.state = 101;
	demo.frames[1].rng.has_call_count = 1;
	demo.frames[1].rng.call_count = 3;
	demo.has_result = true;
	input_demo_result_clear(&demo.result);
	snprintf(demo.result.game, sizeof(demo.result.game), "%s", input_demo_test_game_name());
	snprintf(demo.result.mission, sizeof(demo.result.mission), "%s", input_demo_test_game_name());
	demo.result.level = 1;
	demo.result.difficulty = 2;
	demo.result.frame_count = 2;
	if (!input_demo_file_to_text(demo, &text, &error))
		return report_failure_string(std::string("demo file text failed: ") + error);
	expected =
		std::string("{\"type\":\"header\",\"version\":1,\"game\":\"") + input_demo_test_game_name() +
		"\",\"mission\":\"" + input_demo_test_game_name() +
		"\",\"level\":1,\"difficulty\":2,\"start_mode\":\"new_level\",\"rng_mode\":\"" + rng_mode +
		"\",\"frame_count\":2}\n" +
		"{\"type\":\"frame\",\"f\":0,\"ft\":3276,\"input\":{\"s\":{\"f\":44}},\"rng\":{\"s\":100}}\n" +
		"{\"type\":\"frame\",\"f\":1,\"input\":{\"p\":{\"f1\":1}},\"rng\":{\"s\":101,\"c\":3}}\n" +
		"{\"type\":\"result\",\"result\":{\"v\":1,\"g\":\"" + input_demo_test_game_name() +
		"\",\"m\":\"" + input_demo_test_game_name() + "\",\"l\":1,\"d\":2,\"fr\":2}}\n";
	if (text != expected)
		return report_failure_string(std::string("unexpected demo file text: ") + text);
	if (!input_demo_file_write(path, demo, &error))
		return report_failure_string(std::string("demo file write failed: ") + error);
	if (!read_text_file(path, &file_text)) {
		remove(path);
		return report_failure("could not read demo file");
	}
	if (file_text != expected)
		return report_failure_string(std::string("unexpected demo file text from disk: ") + file_text);
	if (!input_demo_file_read(path, &parsed, &error)) {
		remove(path);
		return report_failure_string(std::string("demo file read failed: ") + error);
	}
	remove(path);
	if (parsed.frames.size() != 2 || parsed.frames[1].rng.state != 101 || !parsed.has_result)
		return report_failure("demo file round trip corrupted content");
	return 0;
}

static int expect_checkpoint_demo_file_output(void)
{
	const char *path = "test_input_demo_checkpoint_fixture.dximdemo";
	const char *rng_mode = input_demo_rng_mode_name(d_rand_get_replay_mode());
	input_demo_file demo;
	input_demo_file parsed;
	input_demo_file invalid;
	std::string text;
	std::string file_text;
	std::string expected;
	std::string error;
	std::string reordered;
	size_t frame_offset;
	size_t result_offset;

	demo.metadata.version = 1;
	demo.metadata.game = input_demo_test_game_name();
	demo.metadata.mission = input_demo_test_game_name();
	demo.metadata.level = 1;
	demo.metadata.difficulty = 2;
	demo.metadata.start_mode = "save_checkpoint";
	demo.metadata.rng_mode = rng_mode;
	demo.metadata.frame_count = 1;
	demo.metadata.start_save = "inputdemo_start.dgss";
	demo.has_checkpoint = true;
	demo.checkpoint.format = "dgss";
	demo.checkpoint.encoding = "base64";
	demo.checkpoint.size = 8;
	demo.checkpoint.sha256 = "077c5f8a7bd52bba7beb0ea8153f1005401b5ba52b797e04952bf14e542fd3b5";
	demo.checkpoint.save_name = "inputdemo_start.dgss";
	demo.checkpoint.has_start_gt = 1;
	demo.checkpoint.start_gt = 124125;
	demo.checkpoint.data = "REdTUxgAAAA=";
	demo.frames.resize(1);
	input_demo_control_record_clear(&demo.frames[0].input);
	demo.frames[0].input.frame = 0;
	demo.frames[0].input.has_frame_time = 1;
	demo.frames[0].input.frame_time = 3276;
	demo.frames[0].input.held.has_forward_thrust_time = 1;
	demo.frames[0].input.held.forward_thrust_time = 44;
	input_demo_rng_record_clear(&demo.frames[0].rng);
	demo.frames[0].rng.frame = 0;
	demo.frames[0].rng.state = 100;
	demo.has_result = true;
	input_demo_result_clear(&demo.result);
	snprintf(demo.result.game, sizeof(demo.result.game), "%s", input_demo_test_game_name());
	snprintf(demo.result.mission, sizeof(demo.result.mission), "%s", input_demo_test_game_name());
	demo.result.level = 1;
	demo.result.difficulty = 2;
	demo.result.frame_count = 1;
	if (!input_demo_file_to_text(demo, &text, &error))
		return report_failure_string(std::string("checkpoint demo file text failed: ") + error);
	expected =
		std::string("{\"type\":\"header\",\"version\":1,\"game\":\"") + input_demo_test_game_name() +
		"\",\"mission\":\"" + input_demo_test_game_name() +
		"\",\"level\":1,\"difficulty\":2,\"start_mode\":\"save_checkpoint\",\"rng_mode\":\"" + rng_mode +
		"\",\"frame_count\":1,\"start_save\":\"inputdemo_start.dgss\"}\n" +
		"{\"type\":\"checkpoint\",\"format\":\"dgss\",\"encoding\":\"base64\",\"size\":8,\"sha256\":\"077c5f8a7bd52bba7beb0ea8153f1005401b5ba52b797e04952bf14e542fd3b5\",\"save_name\":\"inputdemo_start.dgss\",\"start_gt\":124125,\"data\":\"REdTUxgAAAA=\"}\n" +
		"{\"type\":\"frame\",\"f\":0,\"ft\":3276,\"input\":{\"s\":{\"f\":44}},\"rng\":{\"s\":100}}\n" +
		"{\"type\":\"result\",\"result\":{\"v\":1,\"g\":\"" + input_demo_test_game_name() +
		"\",\"m\":\"" + input_demo_test_game_name() + "\",\"l\":1,\"d\":2,\"fr\":1}}\n";
	if (text != expected)
		return report_failure_string(std::string("unexpected checkpoint demo file text: ") + text);
	if (!input_demo_file_write(path, demo, &error))
		return report_failure_string(std::string("checkpoint demo file write failed: ") + error);
	if (!read_text_file(path, &file_text)) {
		remove(path);
		return report_failure("could not read checkpoint demo file");
	}
	if (file_text != expected) {
		remove(path);
		return report_failure_string(std::string("unexpected checkpoint demo file text from disk: ") + file_text);
	}
	if (!input_demo_file_read(path, &parsed, &error)) {
		remove(path);
		return report_failure_string(std::string("checkpoint demo file read failed: ") + error);
	}
	remove(path);
	if (!parsed.has_checkpoint || parsed.frames.size() != 1 || parsed.checkpoint.size != 8 ||
		parsed.checkpoint.data != "REdTUxgAAAA=" || !parsed.checkpoint.has_start_gt ||
		parsed.checkpoint.start_gt != 124125)
		return report_failure("checkpoint demo file round trip corrupted content");
	reordered =
		std::string("{\"type\":\"header\",\"version\":1,\"game\":\"") + input_demo_test_game_name() +
		"\",\"mission\":\"" + input_demo_test_game_name() +
		"\",\"level\":1,\"difficulty\":2,\"start_mode\":\"save_checkpoint\",\"rng_mode\":\"" + rng_mode +
		"\",\"frame_count\":1,\"start_save\":\"inputdemo_start.dgss\"}\n" +
		"{\"type\":\"checkpoint\",\"format\":\"dgss\",\"encoding\":\"base64\",\"size\":8,\"sha256\":\"077c5f8a7bd52bba7beb0ea8153f1005401b5ba52b797e04952bf14e542fd3b5\",\"save_name\":\"inputdemo_start.dgss\",\"start_gt\":124125,\"next_laser_fire_delta\":0,\"next_missile_fire_delta\":0,\"last_laser_fired_delta\":0,\"auto_fire_fusion_delta\":0,\"data\":\"REdTUxgAAAA=\"}\n" +
		"{\"type\":\"frame\",\"f\":0,\"ft\":3276,\"input\":{\"s\":{\"f\":44}},\"rng\":{\"s\":100}}\n" +
		"{\"type\":\"result\",\"result\":{\"v\":1,\"g\":\"" + input_demo_test_game_name() +
		"\",\"m\":\"" + input_demo_test_game_name() + "\",\"l\":1,\"d\":2,\"fr\":1}}\n";
	if (input_demo_file_parse_text(reordered, &parsed, &error))
		return report_failure("legacy checkpoint demo unexpectedly accepted timing metadata");
	invalid = demo;
	invalid.has_checkpoint = false;
	if (input_demo_file_to_text(invalid, &text, &error))
		return report_failure("save_checkpoint demo unexpectedly validated without checkpoint record");
	invalid = demo;
	invalid.metadata.start_mode = "new_level";
	invalid.metadata.start_save.clear();
	if (input_demo_file_to_text(invalid, &text, &error))
		return report_failure("new_level demo unexpectedly validated with checkpoint record");
	frame_offset = expected.find("{\"type\":\"frame\"");
	result_offset = expected.find("{\"type\":\"result\"");
	if (frame_offset == std::string::npos || result_offset == std::string::npos)
		return report_failure("checkpoint expected text missing frame or result records");
	reordered = expected.substr(0, expected.find("{\"type\":\"checkpoint\"")) +
		expected.substr(frame_offset, result_offset - frame_offset) +
		expected.substr(expected.find("{\"type\":\"checkpoint\""), frame_offset - expected.find("{\"type\":\"checkpoint\"")) +
		expected.substr(result_offset);
	if (input_demo_file_parse_text(reordered, &parsed, &error))
		return report_failure("checkpoint record unexpectedly validated after frame record");
	return 0;
}

int main(void)
{
	if (expect_rng_coalescer())
		return 1;
	if (expect_rng_file_round_trip())
		return 1;
	if (expect_demo_file_output())
		return 1;
	if (expect_checkpoint_demo_file_output())
		return 1;
	puts("PASS");
	return 0;
}