#define SDL_MAIN_HANDLED
#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "input_demo_control_info.h"

static int report_failure(const char *message)
{
	fprintf(stderr, "%s\n", message);
	return 1;
}

static int report_failure_string(const std::string &message)
{
	fprintf(stderr, "%s\n", message.c_str());
	return 1;
}

static int expect_round_trip_json(void)
{
	control_info original;
	control_info round_trip;
	input_demo_control_state held;
	input_demo_control_pulse pulse;
	input_demo_control_state held_after;
	input_demo_control_pulse pulse_after;
	input_demo_control_record record;
	input_demo_control_record parsed;
	std::string line;
	std::string error;

	memset(&original, 0, sizeof(original));
	original.pitch_time = 11;
	original.heading_time = -22;
	original.bank_time = 33;
	original.forward_thrust_time = 44;
	original.sideways_thrust_time = -55;
	original.vertical_thrust_time = 66;
	original.fire_primary_state = 1;
	original.fire_secondary_state = 1;
	original.rear_view_state = 1;
	original.automap_state = 1;
	original.fire_primary_count = 2;
	original.fire_secondary_count = 3;
	original.fire_flare_count = 4;
	original.drop_bomb_count = 5;
	original.cycle_primary_count = 6;
	original.cycle_secondary_count = 7;
	original.select_weapon_count = 8;
	original.rear_view_count = 9;
	original.automap_count = 10;
#if defined(INPUT_DEMO_TEST_D2)
	original.afterburner_state = 1;
	original.energy_to_shield_state = 1;
	original.toggle_bomb_count = 11;
	original.headlight_count = 12;
#endif
	input_demo_control_state_from_control_info(&held, &pulse, &original);
	input_demo_control_record_clear(&record);
	record.frame = 0;
	record.run_length = 2;
	record.has_frame_time = 1;
	record.frame_time = 3276;
	input_demo_control_state_update_from_state(&record.held, &held, input_demo_control_info_game());
	input_demo_control_pulse_update_from_pulse(&record.pulse, &pulse, input_demo_control_info_game());
	if (!input_demo_control_record_to_json_line(record, input_demo_control_info_game(), &line, &error))
		return report_failure_string(std::string("round_trip serialize: ") + error);
#if defined(INPUT_DEMO_TEST_D2)
	if (line != "{\"f\":0,\"n\":2,\"ft\":3276,\"s\":{\"p\":11,\"h\":-22,\"b\":33,\"f\":44,\"x\":-55,\"z\":66,\"f1s\":1,\"f2s\":1,\"rvs\":1,\"ams\":1,\"ab\":1,\"es\":1},\"p\":{\"f1\":2,\"f2\":3,\"fl\":4,\"db\":5,\"cp\":6,\"cs\":7,\"sw\":8,\"rv\":9,\"am\":10,\"tb\":11,\"hl\":12}}")
		return report_failure_string(std::string("unexpected D2 control json line: ") + line);
#else
	if (line != "{\"f\":0,\"n\":2,\"ft\":3276,\"s\":{\"p\":11,\"h\":-22,\"b\":33,\"f\":44,\"x\":-55,\"z\":66,\"f1s\":1,\"f2s\":1,\"rvs\":1,\"ams\":1},\"p\":{\"f1\":2,\"f2\":3,\"fl\":4,\"db\":5,\"cp\":6,\"cs\":7,\"sw\":8,\"rv\":9,\"am\":10}}")
		return report_failure_string(std::string("unexpected D1 control json line: ") + line);
#endif
	if (!input_demo_control_record_parse_json_line(line, input_demo_control_info_game(), &parsed, &error))
		return report_failure_string(std::string("round_trip parse: ") + error);
	input_demo_control_state_clear(&held_after);
	input_demo_control_pulse_clear(&pulse_after);
	input_demo_control_state_apply_update(&held_after, &parsed.held, input_demo_control_info_game());
	input_demo_control_pulse_apply_update(&pulse_after, &parsed.pulse, input_demo_control_info_game());
	input_demo_control_info_from_state(&round_trip, &held_after, &pulse_after);
	if (round_trip.pitch_time != original.pitch_time ||
		round_trip.heading_time != original.heading_time ||
		round_trip.bank_time != original.bank_time ||
		round_trip.forward_thrust_time != original.forward_thrust_time ||
		round_trip.sideways_thrust_time != original.sideways_thrust_time ||
		round_trip.vertical_thrust_time != original.vertical_thrust_time ||
		round_trip.fire_primary_state != original.fire_primary_state ||
		round_trip.fire_secondary_state != original.fire_secondary_state ||
		round_trip.rear_view_state != original.rear_view_state ||
		round_trip.automap_state != original.automap_state ||
		round_trip.fire_primary_count != original.fire_primary_count ||
		round_trip.fire_secondary_count != original.fire_secondary_count ||
		round_trip.fire_flare_count != original.fire_flare_count ||
		round_trip.drop_bomb_count != original.drop_bomb_count ||
		round_trip.cycle_primary_count != original.cycle_primary_count ||
		round_trip.cycle_secondary_count != original.cycle_secondary_count ||
		round_trip.select_weapon_count != original.select_weapon_count ||
		round_trip.rear_view_count != original.rear_view_count ||
		round_trip.automap_count != original.automap_count)
		return report_failure("common control round-trip mismatch");
#if defined(INPUT_DEMO_TEST_D2)
	if (round_trip.afterburner_state != original.afterburner_state ||
		round_trip.energy_to_shield_state != original.energy_to_shield_state ||
		round_trip.toggle_bomb_count != original.toggle_bomb_count ||
		round_trip.headlight_count != original.headlight_count)
		return report_failure("D2 control round-trip mismatch");
#endif
	return 0;
}

static int expect_zero_release_update(void)
{
	input_demo_control_state held;
	input_demo_control_pulse pulse;
	input_demo_control_record record;
	std::string error;
	std::string line = "{\"f\":1,\"s\":{\"f\":0,\"f1s\":0},\"p\":{\"f1\":0}}";

	input_demo_control_state_clear(&held);
	input_demo_control_pulse_clear(&pulse);
	held.forward_thrust_time = 99;
	held.fire_primary_state = 1;
	pulse.fire_primary_count = 5;
	if (!input_demo_control_record_parse_json_line(line, input_demo_control_info_game(), &record, &error))
		return report_failure(error.c_str());
	input_demo_control_state_apply_update(&held, &record.held, input_demo_control_info_game());
	input_demo_control_pulse_apply_update(&pulse, &record.pulse, input_demo_control_info_game());
	if (held.forward_thrust_time || held.fire_primary_state || pulse.fire_primary_count)
		return report_failure("explicit zero update was not preserved");
	return 0;
}

static int expect_held_state_bitmask_update(void)
{
	input_demo_control_state held;
	input_demo_control_record record;
	std::string error;
#if defined(INPUT_DEMO_TEST_D2)
	std::string line = "{\"f\":1,\"s\":{\"f1s\":4,\"f2s\":8,\"rvs\":4,\"ams\":8,\"ab\":4,\"es\":8},\"p\":{\"f1\":1}}";
#else
	std::string line = "{\"f\":1,\"s\":{\"f1s\":4,\"f2s\":8,\"rvs\":4,\"ams\":8},\"p\":{\"f1\":1}}";
#endif

	input_demo_control_state_clear(&held);
	if (!input_demo_control_record_parse_json_line(line, input_demo_control_info_game(), &record, &error))
		return report_failure_string(std::string("held bitmask parse: ") + error);
	input_demo_control_state_apply_update(&held, &record.held, input_demo_control_info_game());
	if (held.fire_primary_state != 4 || held.fire_secondary_state != 8 ||
		held.rear_view_state != 4 || held.automap_state != 8)
		return report_failure("common held-state bitmask update was not preserved");
#if defined(INPUT_DEMO_TEST_D2)
	if (held.afterburner_state != 4 || held.energy_to_shield_state != 8)
		return report_failure("D2 held-state bitmask update was not preserved");
#endif
	return 0;
}

static int expect_file_round_trip(void)
{
	const char *path = "test_input_demo_controls.jsonl";
	const char *invalid_path = "test_input_demo_controls_invalid.jsonl";
	std::vector<input_demo_control_record> records;
	std::vector<input_demo_control_record> parsed_records;
	input_demo_control_record record0;
	input_demo_control_record record1;
	std::string error;
	FILE *f;

	input_demo_control_record_clear(&record0);
	record0.frame = 0;
	record0.has_frame_time = 1;
	record0.frame_time = 3276;
	record0.held.has_forward_thrust_time = 1;
	record0.held.forward_thrust_time = 44;
	input_demo_control_record_clear(&record1);
	record1.frame = 4;
	record1.held.has_forward_thrust_time = 1;
	record1.held.forward_thrust_time = 0;
	record1.pulse.has_fire_primary_count = 1;
	record1.pulse.fire_primary_count = 1;
	record1.run_length = 3;
	records.push_back(record0);
	records.push_back(record1);
	if (!input_demo_control_records_write_jsonl_file(path, input_demo_control_info_game(), records, &error))
		return report_failure_string(std::string("file write: ") + error);
	if (!input_demo_control_records_read_jsonl_file(path, input_demo_control_info_game(), &parsed_records, &error))
		return report_failure_string(std::string("file read: ") + error);
	remove(path);
	if (parsed_records.size() != records.size())
		return report_failure("parsed control record count mismatch");
	if (parsed_records[0].frame != 0 || !parsed_records[0].has_frame_time ||
		parsed_records[1].frame != 4 || parsed_records[1].run_length != 3)
		return report_failure("parsed control records lost stream metadata");
	f = fopen(invalid_path, "wb");
	if (!f)
		return report_failure("could not write invalid control jsonl file");
	fputs("{\"f\":0}\n", f);
	fclose(f);
	if (input_demo_control_records_read_jsonl_file(invalid_path, input_demo_control_info_game(),
		&parsed_records, &error)) {
		remove(invalid_path);
		return report_failure("invalid control jsonl unexpectedly validated");
	}
	remove(invalid_path);
	return 0;
}

static int expect_d2_key_policy(void)
{
	input_demo_control_record record;
	std::string error;

#if defined(INPUT_DEMO_TEST_D2)
	if (!input_demo_control_record_parse_json_line(
		"{\"f\":0,\"ft\":1,\"s\":{\"ab\":1},\"p\":{\"tb\":1,\"hl\":2}}",
		input_demo_control_info_game(), &record, &error))
		return report_failure_string(std::string("d2_key_policy parse: ") + error);
#else
	if (input_demo_control_record_parse_json_line(
		"{\"f\":0,\"ft\":1,\"s\":{\"ab\":1},\"p\":{\"tb\":1}}",
		input_demo_control_info_game(), &record, &error))
		return report_failure("D1 accepted D2-only control keys");
#endif
	if (input_demo_control_record_parse_json_line(
		"{\"f\":0,\"ft\":1,\"q\":1}", input_demo_control_info_game(), &record, &error))
		return report_failure("unknown control record key unexpectedly validated");
	return 0;
}

static int expect_coalesced_constant_run(void)
{
	std::vector<input_demo_control_frame> frames(4);
	std::vector<input_demo_control_record> records;
	std::string line;
	std::string error;
	size_t i;

	for (i = 0; i != frames.size(); ++i) {
		input_demo_control_frame_clear(&frames[i]);
		frames[i].frame = (uint32_t)i;
		frames[i].frame_time = 3276;
		frames[i].state.forward_thrust_time = 44;
	}
	if (!input_demo_control_records_coalesce_frames(frames, input_demo_control_info_game(), &records, &error))
		return report_failure_string(std::string("coalesce constant run: ") + error);
	if (records.size() != 1)
		return report_failure("constant run did not collapse to one record");
	if (records[0].frame != 0 || records[0].run_length != 4 || !records[0].has_frame_time ||
		records[0].frame_time != 3276 || !records[0].held.has_forward_thrust_time ||
		records[0].held.forward_thrust_time != 44)
		return report_failure("constant run record fields are wrong");
	if (!input_demo_control_record_to_json_line(records[0], input_demo_control_info_game(), &line, &error))
		return report_failure_string(std::string("coalesce constant run json: ") + error);
	if (line != "{\"f\":0,\"n\":4,\"ft\":3276,\"s\":{\"f\":44}}")
		return report_failure_string(std::string("unexpected constant run json line: ") + line);
	return 0;
}

static int expect_coalesced_pulse_breaks_run(void)
{
	std::vector<input_demo_control_frame> frames(4);
	std::vector<input_demo_control_record> records;
	std::string line0;
	std::string line1;
	std::string line2;
	std::string error;
	size_t i;

	for (i = 0; i != frames.size(); ++i) {
		input_demo_control_frame_clear(&frames[i]);
		frames[i].frame = (uint32_t)i;
		frames[i].frame_time = 3276;
		frames[i].state.forward_thrust_time = 44;
	}
	frames[1].pulse.fire_primary_count = 1;
	if (!input_demo_control_records_coalesce_frames(frames, input_demo_control_info_game(), &records, &error))
		return report_failure_string(std::string("coalesce pulse run: ") + error);
	if (records.size() != 3)
		return report_failure("pulse run did not split into three records");
	if (!input_demo_control_record_to_json_line(records[0], input_demo_control_info_game(), &line0, &error) ||
		!input_demo_control_record_to_json_line(records[1], input_demo_control_info_game(), &line1, &error) ||
		!input_demo_control_record_to_json_line(records[2], input_demo_control_info_game(), &line2, &error))
		return report_failure_string(std::string("coalesce pulse run json: ") + error);
	if (line0 != "{\"f\":0,\"ft\":3276,\"s\":{\"f\":44}}")
		return report_failure_string(std::string("unexpected pulse run first line: ") + line0);
	if (line1 != "{\"f\":1,\"p\":{\"f1\":1}}")
		return report_failure_string(std::string("unexpected pulse run pulse line: ") + line1);
	if (line2 != "{\"f\":2,\"n\":2}")
		return report_failure_string(std::string("unexpected pulse run tail line: ") + line2);
	return 0;
}

static int expect_coalesced_release_and_ft_change(void)
{
	std::vector<input_demo_control_frame> frames(4);
	std::vector<input_demo_control_record> records;
	std::string line0;
	std::string line1;
	std::string line2;
	std::string error;
	size_t i;

	for (i = 0; i != frames.size(); ++i) {
		input_demo_control_frame_clear(&frames[i]);
		frames[i].frame = (uint32_t)i;
		frames[i].frame_time = 3276;
	}
	frames[0].state.forward_thrust_time = 44;
	frames[1].state.forward_thrust_time = 44;
	frames[3].frame_time = 4000;
	if (!input_demo_control_records_coalesce_frames(frames, input_demo_control_info_game(), &records, &error))
		return report_failure_string(std::string("coalesce release run: ") + error);
	if (records.size() != 3)
		return report_failure("release/frame-time run did not split into three records");
	if (!input_demo_control_record_to_json_line(records[0], input_demo_control_info_game(), &line0, &error) ||
		!input_demo_control_record_to_json_line(records[1], input_demo_control_info_game(), &line1, &error) ||
		!input_demo_control_record_to_json_line(records[2], input_demo_control_info_game(), &line2, &error))
		return report_failure_string(std::string("coalesce release run json: ") + error);
	if (line0 != "{\"f\":0,\"n\":2,\"ft\":3276,\"s\":{\"f\":44}}")
		return report_failure_string(std::string("unexpected release run first line: ") + line0);
	if (line1 != "{\"f\":2,\"s\":{\"f\":0}}")
		return report_failure_string(std::string("unexpected release run second line: ") + line1);
	if (line2 != "{\"f\":3,\"ft\":4000}")
		return report_failure_string(std::string("unexpected release run third line: ") + line2);
	return 0;
}

static int expect_coalescer_game_policy(void)
{
	std::vector<input_demo_control_frame> frames(1);
	std::vector<input_demo_control_record> records;
	std::string error;

	input_demo_control_frame_clear(&frames[0]);
	frames[0].frame = 0;
	frames[0].frame_time = 1;
#if defined(INPUT_DEMO_TEST_D2)
	frames[0].state.afterburner_state = 1;
	frames[0].pulse.toggle_bomb_count = 1;
	if (!input_demo_control_records_coalesce_frames(frames, input_demo_control_info_game(), &records, &error))
		return report_failure_string(std::string("d2 coalescer unexpectedly failed: ") + error);
#else
	frames[0].state.afterburner_state = 1;
	if (input_demo_control_records_coalesce_frames(frames, input_demo_control_info_game(), &records, &error))
		return report_failure("D1 coalescer accepted D2-only state fields");
	frames[0].state.afterburner_state = 0;
	frames[0].pulse.toggle_bomb_count = 1;
	if (input_demo_control_records_coalesce_frames(frames, input_demo_control_info_game(), &records, &error))
		return report_failure("D1 coalescer accepted D2-only pulse fields");
#endif
	return 0;
}

#ifdef main
#undef main
#endif

int main(int argc, char *argv[])
{
	(void)argc;
	(void)argv;
	if (expect_round_trip_json())
		return 1;
	if (expect_zero_release_update())
		return 1;
	if (expect_held_state_bitmask_update())
		return 1;
	if (expect_file_round_trip())
		return 1;
	if (expect_d2_key_policy())
		return 1;
	if (expect_coalesced_constant_run())
		return 1;
	if (expect_coalesced_pulse_breaks_run())
		return 1;
	if (expect_coalesced_release_and_ft_change())
		return 1;
	if (expect_coalescer_game_policy())
		return 1;
	puts("PASS");
	return 0;
}
