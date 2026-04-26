#include "input_demo_fixture.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

using ordered_json = nlohmann::ordered_json;

extern "C" void input_demo_rng_frame_clear(input_demo_rng_frame *frame)
{
	memset(frame, 0, sizeof(*frame));
}

extern "C" void input_demo_rng_record_clear(input_demo_rng_record *record)
{
	memset(record, 0, sizeof(*record));
	record->run_length = 1;
}

static bool fail(std::string *error, const std::string &message)
{
	if (error)
		*error = message;
	return false;
}

static bool parse_uint32_field(const ordered_json &value, uint32_t *out, std::string *error,
                               const char *field_name)
{
	unsigned long long parsed;

	if (!value.is_number_integer() && !value.is_number_unsigned())
		return fail(error, std::string(field_name) + " must be an unsigned integer");
	if (value.is_number_integer() && value.get<long long>() < 0)
		return fail(error, std::string(field_name) + " must be non-negative");
	parsed = value.get<unsigned long long>();
	if (parsed > UINT_MAX)
		return fail(error, std::string(field_name) + " is out of range");
	*out = (uint32_t) parsed;
	return true;
}

static bool validate_rng_record(const input_demo_rng_record &record, std::string *error)
{
	if (!record.run_length)
		return fail(error, "rng record n must be positive");
	return true;
}

static bool validate_rng_stream(const std::vector<input_demo_rng_record> &records,
                                std::string *error)
{
	uint32_t expected_frame = 0;
	size_t i;

	if (records.empty())
		return fail(error, "rng stream is empty");
	for (i = 0; i != records.size(); ++i) {
		if (!validate_rng_record(records[i], error))
			return false;
		if (records[i].frame != expected_frame) {
			if (!i)
				return fail(error, "first rng record must include f: 0");
			return fail(error, "rng records must be contiguous");
		}
		expected_frame = records[i].frame + records[i].run_length;
	}
	return true;
}

bool input_demo_rng_record_to_json_line(const input_demo_rng_record &record,
                                        std::string *line, std::string *error)
{
	ordered_json json;

	if (!line)
		return fail(error, "missing rng json line output");
	if (!validate_rng_record(record, error))
		return false;
	json["f"] = record.frame;
	if (record.run_length != 1)
		json["n"] = record.run_length;
	json["s"] = record.state;
	if (record.has_call_count)
		json["c"] = record.call_count;
	*line = json.dump();
	return true;
}

bool input_demo_rng_record_parse_json_line(const std::string &line,
                                           input_demo_rng_record *record, std::string *error)
{
	ordered_json json;
	bool have_frame = false;
	bool have_state = false;
	ordered_json::const_iterator it;

	if (!record)
		return fail(error, "missing rng record output");
	try {
		json = ordered_json::parse(line);
	} catch (const std::exception &e) {
		return fail(error, std::string("invalid rng json line: ") + e.what());
	}
	if (!json.is_object())
		return fail(error, "rng json line must be an object");
	input_demo_rng_record_clear(record);
	for (it = json.begin(); it != json.end(); ++it) {
		if (it.key() == "f") {
			uint32_t frame;

			if (!parse_uint32_field(it.value(), &frame, error, "f"))
				return false;
			record->frame = frame;
			have_frame = true;
		} else if (it.key() == "n") {
			uint32_t run_length;

			if (!parse_uint32_field(it.value(), &run_length, error, "n"))
				return false;
			record->run_length = run_length;
		} else if (it.key() == "s") {
			uint32_t state;

			if (!parse_uint32_field(it.value(), &state, error, "s"))
				return false;
			record->state = state;
			have_state = true;
		} else if (it.key() == "c") {
			uint32_t call_count;

			if (!parse_uint32_field(it.value(), &call_count, error, "c"))
				return false;
			record->call_count = call_count;
			record->has_call_count = 1;
		} else {
			return fail(error, std::string("unknown rng record key: ") + it.key());
		}
	}
	if (!have_frame)
		return fail(error, "rng json line is missing f");
	if (!have_state)
		return fail(error, "rng json line is missing s");
	return validate_rng_record(*record, error);
}

bool input_demo_rng_records_coalesce_frames(const std::vector<input_demo_rng_frame> &frames,
                                            std::vector<input_demo_rng_record> *records, std::string *error)
{
	std::vector<input_demo_rng_record> out;
	uint32_t expected_frame = 0;
	size_t i;

	if (!records)
		return fail(error, "missing rng record list output");
	if (frames.empty())
		return fail(error, "rng frame list is empty");
	for (i = 0; i != frames.size(); ++i) {
		input_demo_rng_record record;

		if (frames[i].frame != expected_frame) {
			if (!i)
				return fail(error, "first rng frame must include f: 0");
			return fail(error, "rng frames must be contiguous");
		}
		input_demo_rng_record_clear(&record);
		record.frame = frames[i].frame;
		record.state = frames[i].state;
		record.has_call_count = frames[i].has_call_count;
		record.call_count = frames[i].call_count;
		if (!out.empty() &&
		    out.back().state == record.state &&
		    out.back().has_call_count == record.has_call_count &&
		    (!record.has_call_count || out.back().call_count == record.call_count)) {
			out.back().run_length++;
		} else {
			out.push_back(record);
		}
		expected_frame = frames[i].frame + 1;
	}
	if (!validate_rng_stream(out, error))
		return false;
	*records = out;
	return true;
}

bool input_demo_rng_records_write_jsonl_file(const char *path,
                                             const std::vector<input_demo_rng_record> &records, std::string *error)
{
	std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
	std::string line;
	size_t i;

	if (!path || !path[0])
		return fail(error, "missing rng jsonl path");
	if (!validate_rng_stream(records, error))
		return false;
	if (!out)
		return fail(error, std::string("could not open rng jsonl file: ") + path);
	for (i = 0; i != records.size(); ++i) {
		if (!input_demo_rng_record_to_json_line(records[i], &line, error))
			return false;
		out << line << '\n';
	}
	if (!out.good())
		return fail(error, std::string("could not write rng jsonl file: ") + path);
	return true;
}

bool input_demo_rng_records_read_jsonl_file(const char *path,
                                            std::vector<input_demo_rng_record> *records, std::string *error)
{
	std::ifstream in(path, std::ios::in | std::ios::binary);
	std::vector<input_demo_rng_record> parsed_records;
	std::string line;
	unsigned int line_number = 0;

	if (!records)
		return fail(error, "missing rng record list output");
	if (!path || !path[0])
		return fail(error, "missing rng jsonl path");
	if (!in)
		return fail(error, std::string("could not open rng jsonl file: ") + path);
	while (std::getline(in, line)) {
		input_demo_rng_record record;
		std::string line_error;

		line_number++;
		if (line.empty())
			continue;
		if (!input_demo_rng_record_parse_json_line(line, &record, &line_error))
			return fail(error, std::string("rng jsonl line ") + std::to_string(line_number) +
			                       ": " + line_error);
		parsed_records.push_back(record);
	}
	if (!validate_rng_stream(parsed_records, error))
		return false;
	*records = parsed_records;
	return true;
}

static bool validate_metadata(const input_demo_metadata &metadata, std::string *error)
{
	size_t i;

	if (metadata.version != 1)
		return fail(error, "metadata version must be 1");
	if (metadata.game != "d1" && metadata.game != "d2")
		return fail(error, "metadata game must be d1 or d2");
	if (metadata.mission.empty())
		return fail(error, "metadata mission is required");
	if (metadata.difficulty < 0)
		return fail(error, "metadata difficulty must be non-negative");
	if (metadata.start_mode != "new_level" && metadata.start_mode != "save_checkpoint")
		return fail(error, "metadata start_mode must be new_level or save_checkpoint");
	if (metadata.start_mode == "save_checkpoint" && metadata.start_save.empty())
		return fail(error, "metadata start_save is required for save_checkpoint");
	if (metadata.start_mode == "new_level" && !metadata.start_save.empty())
		return fail(error, "metadata start_save must be omitted for new_level");
	if (metadata.rng_mode.empty())
		return fail(error, "metadata rng_mode is required");
	if (!metadata.frame_count)
		return fail(error, "metadata frame_count must be positive");
	if (metadata.streams.empty())
		return fail(error, "metadata streams must not be empty");
	for (i = 0; i != metadata.streams.size(); ++i) {
		if (metadata.streams[i].input_path.empty())
			return fail(error, "metadata stream input path is required");
		if (metadata.streams[i].rng_path.empty())
			return fail(error, "metadata stream rng path is required");
	}
	if (metadata.result_path.empty())
		return fail(error, "metadata result path is required");
	return true;
}

bool input_demo_metadata_to_json_text(const input_demo_metadata &metadata,
                                      std::string *text, std::string *error)
{
	ordered_json root = ordered_json::object();
	ordered_json streams = ordered_json::array();
	size_t i;

	if (!text)
		return fail(error, "missing metadata text output");
	if (!validate_metadata(metadata, error))
		return false;
	root["version"] = metadata.version;
	root["game"] = metadata.game;
	root["mission"] = metadata.mission;
	root["level"] = metadata.level;
	root["difficulty"] = metadata.difficulty;
	root["start_mode"] = metadata.start_mode;
	root["rng_mode"] = metadata.rng_mode;
	root["frame_count"] = metadata.frame_count;
	for (i = 0; i != metadata.streams.size(); ++i) {
		ordered_json stream = ordered_json::object();

		stream["player"] = metadata.streams[i].player;
		stream["input"] = metadata.streams[i].input_path;
		stream["rng"] = metadata.streams[i].rng_path;
		streams.push_back(stream);
	}
	root["streams"] = streams;
	if (!metadata.classic_preview.empty())
		root["classic_preview"] = metadata.classic_preview;
	if (!metadata.start_save.empty())
		root["start_save"] = metadata.start_save;
	root["result"] = metadata.result_path;
	*text = root.dump(4);
	text->push_back('\n');
	return true;
}

bool input_demo_metadata_write_json5_file(const char *path,
                                          const input_demo_metadata &metadata, std::string *error)
{
	std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
	std::string text;

	if (!path || !path[0])
		return fail(error, "missing metadata path");
	if (!input_demo_metadata_to_json_text(metadata, &text, error))
		return false;
	if (!out)
		return fail(error, std::string("could not open metadata file: ") + path);
	out << text;
	if (!out.good())
		return fail(error, std::string("could not write metadata file: ") + path);
	return true;
}