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

static int expect_metadata_output(void)
{
	const char *path = "test_input_demo_metadata_demo.json5";
	const char *rng_mode = input_demo_rng_mode_name(d_rand_get_replay_mode());
	input_demo_metadata metadata;
	input_demo_stream_file stream;
	std::string text;
	std::string file_text;
	std::string expected;
	std::string error;

	metadata.version = 1;
	metadata.game = input_demo_test_game_name();
	metadata.mission = input_demo_test_game_name();
	metadata.level = 1;
	metadata.difficulty = 2;
	metadata.start_mode = "new_level";
	metadata.rng_mode = rng_mode;
	metadata.frame_count = 6;
	stream.player = 0;
	stream.input_path = "inputs.p0.jsonl";
	stream.rng_path = "rng.p0.jsonl";
	metadata.streams.push_back(stream);
	metadata.result_path = "result.json";
	if (!input_demo_metadata_to_json_text(metadata, &text, &error))
		return report_failure_string(std::string("metadata text failed: ") + error);
	expected =
		std::string("{\n") +
		"    \"version\": 1,\n" +
		"    \"game\": \"" + input_demo_test_game_name() + "\",\n" +
		"    \"mission\": \"" + input_demo_test_game_name() + "\",\n" +
		"    \"level\": 1,\n" +
		"    \"difficulty\": 2,\n" +
		"    \"start_mode\": \"new_level\",\n" +
		"    \"rng_mode\": \"" + rng_mode + "\",\n" +
		"    \"frame_count\": 6,\n" +
		"    \"streams\": [\n" +
		"        {\n" +
		"            \"player\": 0,\n" +
		"            \"input\": \"inputs.p0.jsonl\",\n" +
		"            \"rng\": \"rng.p0.jsonl\"\n" +
		"        }\n" +
		"    ],\n" +
		"    \"result\": \"result.json\"\n" +
		"}\n";
	if (text != expected)
		return report_failure_string(std::string("unexpected metadata text: ") + text);
	if (!input_demo_metadata_write_json5_file(path, metadata, &error))
		return report_failure_string(std::string("metadata file write failed: ") + error);
	if (!read_text_file(path, &file_text)) {
		remove(path);
		return report_failure("could not read metadata file");
	}
	remove(path);
	if (file_text != expected)
		return report_failure_string(std::string("unexpected metadata file text: ") + file_text);
	return 0;
}

int main(void)
{
	if (expect_rng_coalescer())
		return 1;
	if (expect_rng_file_round_trip())
		return 1;
	if (expect_metadata_output())
		return 1;
	puts("PASS");
	return 0;
}