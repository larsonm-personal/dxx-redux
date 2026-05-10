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

extern "C" void input_demo_player_cfg_clear(input_demo_player_cfg *player_cfg)
{
	if (!player_cfg)
		return;
	memset(player_cfg, 0, sizeof(*player_cfg));
}

extern "C" void input_demo_checkpoint_escort_state_clear(input_demo_checkpoint_escort_state *escort_state)
{
	if (!escort_state)
		return;
	memset(escort_state, 0, sizeof(*escort_state));
	escort_state->escort_kill_object = INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET;
	escort_state->escort_goal_object = INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET;
	escort_state->escort_special_goal = INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET;
	escort_state->escort_goal_index = INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET;
	escort_state->buddy_messages_suppressed = INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET;
	escort_state->buddy_sorry_time = INPUT_DEMO_CHECKPOINT_ESCORT_I64_UNSET;
	escort_state->looking_for_marker = INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET;
	escort_state->last_buddy_key = INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET;
	escort_state->last_buddy_message_time = INPUT_DEMO_CHECKPOINT_ESCORT_I64_UNSET;
	escort_state->escort_owner_player = INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET;
}

extern "C" void input_demo_checkpoint_thief_state_clear(input_demo_checkpoint_thief_state *thief_state)
{
	if (!thief_state)
		return;
	memset(thief_state, 0, sizeof(*thief_state));
}

static bool fail(std::string *error, const std::string &message)
{
	if (error)
		*error = message;
	return false;
}

static bool is_blank_or_comment_line(const std::string &line)
{
	const size_t first = line.find_first_not_of(" \t\r\n");
	return first == std::string::npos || (line[first] == '/' && first + 1 < line.size() && line[first + 1] == '/');
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

static bool parse_int_field(const ordered_json &value, int *out, std::string *error,
                            const char *field_name)
{
	long long parsed;

	if (!value.is_number_integer())
		return fail(error, std::string(field_name) + " must be an integer");
	parsed = value.get<long long>();
	if (parsed < INT_MIN || parsed > INT_MAX)
		return fail(error, std::string(field_name) + " is out of range");
	*out = (int) parsed;
	return true;
}

static bool parse_int64_field(const ordered_json &value, int64_t *out, std::string *error,
                              const char *field_name)
{
	long long parsed;

	if (!value.is_number_integer())
		return fail(error, std::string(field_name) + " must be an integer");
	parsed = value.get<long long>();
	*out = (int64_t) parsed;
	return true;
}

static bool parse_uint8_field(const ordered_json &value, uint8_t *out, std::string *error,
                              const char *field_name)
{
	uint32_t parsed;

	if (!parse_uint32_field(value, &parsed, error, field_name))
		return false;
	if (parsed > UINT8_MAX)
		return fail(error, std::string(field_name) + " is out of range");
	*out = (uint8_t) parsed;
	return true;
}

static uint8_t player_cfg_primary_order_count_for_game(const std::string &game)
{
	if (game == "d1")
		return 7;
	if (game == "d2")
		return 11;
	return 0;
}

static uint8_t player_cfg_secondary_order_count_for_game(const std::string &game)
{
	if (game == "d1")
		return 6;
	if (game == "d2")
		return 11;
	return 0;
}

static bool parse_player_cfg_order(const ordered_json &value,
                                   uint8_t *items,
                                   uint8_t max_count,
                                   uint8_t *count,
                                   std::string *error,
                                   const char *field_name)
{
	size_t i;

	if (!value.is_array())
		return fail(error, std::string(field_name) + " must be an array");
	if (value.size() > max_count)
		return fail(error, std::string(field_name) + " has too many entries");
	for (i = 0; i != value.size(); ++i) {
		uint8_t parsed;

		if (!parse_uint8_field(value[i], &parsed, error, field_name))
			return false;
		items[i] = parsed;
	}
	*count = (uint8_t) value.size();
	return true;
}

static bool parse_player_cfg(const ordered_json &value,
                             input_demo_player_cfg *player_cfg,
                             std::string *error)
{
	ordered_json::const_iterator it;
	input_demo_player_cfg parsed;

	if (!player_cfg)
		return fail(error, "missing player_cfg output");
	if (!value.is_object())
		return fail(error, "metadata player_cfg must be an object");
	input_demo_player_cfg_clear(&parsed);
	for (it = value.begin(); it != value.end(); ++it) {
		const std::string &name = it.key();

		if (name == "auto_leveling") {
			if (!parse_int_field(it.value(), &parsed.auto_leveling, error, "player_cfg auto_leveling"))
				return false;
		} else if (name == "persistent_debris") {
			if (!parse_int_field(it.value(), &parsed.persistent_debris, error, "player_cfg persistent_debris"))
				return false;
		} else if (name == "headlight_active_default") {
			if (!parse_int_field(it.value(), &parsed.headlight_active_default, error, "player_cfg headlight_active_default"))
				return false;
			parsed.has_headlight_active_default = 1;
		} else if (name == "no_fire_autoselect") {
			if (!parse_uint8_field(it.value(), &parsed.no_fire_autoselect, error, "player_cfg no_fire_autoselect"))
				return false;
		} else if (name == "cycle_autoselect_only") {
			if (!parse_uint8_field(it.value(), &parsed.cycle_autoselect_only, error, "player_cfg cycle_autoselect_only"))
				return false;
		} else if (name == "select_after_fire") {
			if (!parse_uint8_field(it.value(), &parsed.select_after_fire, error, "player_cfg select_after_fire"))
				return false;
		} else if (name == "classic_autoselect_weapon") {
			if (!parse_uint8_field(it.value(), &parsed.classic_autoselect_weapon, error, "player_cfg classic_autoselect_weapon"))
				return false;
		} else if (name == "primary_order") {
			if (!parse_player_cfg_order(it.value(), parsed.primary_order,
			                            INPUT_DEMO_PLAYER_CFG_PRIMARY_ORDER_MAX,
			                            &parsed.primary_order_count, error,
			                            "player_cfg primary_order"))
				return false;
		} else if (name == "secondary_order") {
			if (!parse_player_cfg_order(it.value(), parsed.secondary_order,
			                            INPUT_DEMO_PLAYER_CFG_SECONDARY_ORDER_MAX,
			                            &parsed.secondary_order_count, error,
			                            "player_cfg secondary_order"))
				return false;
		} else {
			return fail(error, "unknown player_cfg key: " + name);
		}
	}
	*player_cfg = parsed;
	return true;
}

static bool validate_player_cfg(const input_demo_player_cfg &player_cfg,
                                const std::string &game,
                                std::string *error)
{
	const uint8_t expected_primary = player_cfg_primary_order_count_for_game(game);
	const uint8_t expected_secondary = player_cfg_secondary_order_count_for_game(game);

	if (!expected_primary || !expected_secondary)
		return fail(error, "metadata player_cfg requires a valid game");
	if (player_cfg.primary_order_count != expected_primary)
		return fail(error, "metadata player_cfg primary_order has the wrong length");
	if (player_cfg.secondary_order_count != expected_secondary)
		return fail(error, "metadata player_cfg secondary_order has the wrong length");
	return true;
}

static void player_cfg_to_json(const input_demo_player_cfg &player_cfg,
                               ordered_json *json)
{
	ordered_json primary = ordered_json::array();
	ordered_json secondary = ordered_json::array();
	uint8_t i;

	(*json)["auto_leveling"] = player_cfg.auto_leveling;
	(*json)["persistent_debris"] = player_cfg.persistent_debris;
	if (player_cfg.has_headlight_active_default)
		(*json)["headlight_active_default"] = player_cfg.headlight_active_default;
	(*json)["no_fire_autoselect"] = player_cfg.no_fire_autoselect;
	(*json)["cycle_autoselect_only"] = player_cfg.cycle_autoselect_only;
	(*json)["select_after_fire"] = player_cfg.select_after_fire;
	(*json)["classic_autoselect_weapon"] = player_cfg.classic_autoselect_weapon;
	for (i = 0; i != player_cfg.primary_order_count; ++i)
		primary.push_back(player_cfg.primary_order[i]);
	for (i = 0; i != player_cfg.secondary_order_count; ++i)
		secondary.push_back(player_cfg.secondary_order[i]);
	(*json)["primary_order"] = primary;
	(*json)["secondary_order"] = secondary;
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
		if (is_blank_or_comment_line(line))
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

static int game_id_from_name(const std::string &game_name)
{
	if (game_name == "d1")
		return INPUT_DEMO_GAME_D1;
	if (game_name == "d2")
		return INPUT_DEMO_GAME_D2;
	return 0;
}

static bool validate_checkpoint(const input_demo_checkpoint &checkpoint, std::string *error)
{
	if (checkpoint.format != "dgss")
		return fail(error, "checkpoint format must be dgss");
	if (checkpoint.encoding != "base64")
		return fail(error, "checkpoint encoding must be base64");
	if (checkpoint.compression != "none" && checkpoint.compression != "zlib")
		return fail(error, "checkpoint compression must be none or zlib");
	if (!checkpoint.size)
		return fail(error, "checkpoint size must be positive");
	if (checkpoint.sha256.empty())
		return fail(error, "checkpoint sha256 is required");
	if (checkpoint.save_name.empty())
		return fail(error, "checkpoint save_name is required");
	if (!checkpoint.has_start_gt)
		return fail(error, "checkpoint start_gt is required");
	if (checkpoint.escort_state.valid &&
	    checkpoint.escort_state.buddy_allowed_to_talk != 0 &&
	    checkpoint.escort_state.buddy_allowed_to_talk != 1)
		return fail(error, "checkpoint buddy_allowed_to_talk must be 0 or 1");
	if (checkpoint.thief_state.valid && checkpoint.thief_state.stolen_item_index < 0)
		return fail(error, "checkpoint thief_stolen_item_index must be non-negative");
	if (checkpoint.data.empty())
		return fail(error, "checkpoint data is required");
	return true;
}

static bool validate_metadata(const input_demo_metadata &metadata, std::string *error)
{
	if (metadata.version != 2 && metadata.version != 3 && metadata.version != 4)
		return fail(error, "metadata version must be 2, 3, or 4");
	if (metadata.game != "d1" && metadata.game != "d2")
		return fail(error, "metadata game must be d1 or d2");
	if (metadata.mission.empty())
		return fail(error, "metadata mission is required");
	if (metadata.build_number < 0)
		return fail(error, "metadata build_number must be non-negative");
	if (metadata.git_version.empty())
		return fail(error, "metadata git_version is required");
	if (metadata.arch.empty())
		return fail(error, "metadata arch is required");
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
	if (metadata.has_player_cfg && !validate_player_cfg(metadata.player_cfg, metadata.game, error))
		return false;
	return true;
}

static bool parse_type(const ordered_json &root, const char *expected_type, std::string *error)
{
	if (!root.contains("type"))
		return fail(error, std::string(expected_type) + " record is missing type");
	if (!root.at("type").is_string())
		return fail(error, std::string(expected_type) + " record type must be a string");
	if (root.at("type").get<std::string>() != expected_type)
		return fail(error, std::string("expected ") + expected_type + " record");
	return true;
}

static bool parse_json_line(const std::string &line, const char *label,
                            ordered_json *json, std::string *error)
{
	try {
		*json = ordered_json::parse(line);
	} catch (const std::exception &e) {
		return fail(error, std::string("invalid ") + label + " json: " + e.what());
	}
	if (!json->is_object())
		return fail(error, std::string(label) + " json must be an object");
	return true;
}

static bool parse_checkpoint_record(const ordered_json &root,
                                    input_demo_checkpoint *checkpoint,
                                    std::string *error)
{
	ordered_json::const_iterator it;
	input_demo_checkpoint parsed;
	bool have_buddy_allowed_to_talk = false;
	bool have_buddy_last_seen_player = false;
	bool have_buddy_last_player_path_created = false;
	bool have_escort_last_path_created = false;
	bool have_last_come_back_message_time = false;
	bool have_buddy_last_missile_time = false;
	bool have_thief_stolen_item_index = false;
	bool have_re_init_thief_time = false;
	bool have_last_thief_hit_time = false;

	if (!checkpoint)
		return fail(error, "missing checkpoint output");
	if (!parse_type(root, "checkpoint", error))
		return false;
	for (it = root.begin(); it != root.end(); ++it) {
		const std::string &name = it.key();

		if (name == "type") {
			continue;
		} else if (name == "format") {
			if (!it.value().is_string())
				return fail(error, "checkpoint format must be a string");
			parsed.format = it.value().get<std::string>();
		} else if (name == "encoding") {
			if (!it.value().is_string())
				return fail(error, "checkpoint encoding must be a string");
			parsed.encoding = it.value().get<std::string>();
		} else if (name == "compression") {
			if (!it.value().is_string())
				return fail(error, "checkpoint compression must be a string");
			parsed.compression = it.value().get<std::string>();
		} else if (name == "size") {
			if (!parse_uint32_field(it.value(), &parsed.size, error, "checkpoint size"))
				return false;
		} else if (name == "sha256") {
			if (!it.value().is_string())
				return fail(error, "checkpoint sha256 must be a string");
			parsed.sha256 = it.value().get<std::string>();
		} else if (name == "save_name") {
			if (!it.value().is_string())
				return fail(error, "checkpoint save_name must be a string");
			parsed.save_name = it.value().get<std::string>();
		} else if (name == "start_gt") {
			if (!parse_int64_field(it.value(), &parsed.start_gt, error, "checkpoint start_gt"))
				return false;
			parsed.has_start_gt = 1;
		} else if (name == "buddy_allowed_to_talk") {
			if (!parse_int_field(it.value(), &parsed.escort_state.buddy_allowed_to_talk, error, "checkpoint buddy_allowed_to_talk"))
				return false;
			have_buddy_allowed_to_talk = true;
		} else if (name == "buddy_last_seen_player") {
			if (!parse_int64_field(it.value(), &parsed.escort_state.buddy_last_seen_player, error, "checkpoint buddy_last_seen_player"))
				return false;
			have_buddy_last_seen_player = true;
		} else if (name == "buddy_last_player_path_created") {
			if (!parse_int64_field(it.value(), &parsed.escort_state.buddy_last_player_path_created, error, "checkpoint buddy_last_player_path_created"))
				return false;
			have_buddy_last_player_path_created = true;
		} else if (name == "escort_kill_object") {
			if (!parse_int_field(it.value(), &parsed.escort_state.escort_kill_object, error, "checkpoint escort_kill_object"))
				return false;
		} else if (name == "escort_last_path_created") {
			if (!parse_int64_field(it.value(), &parsed.escort_state.escort_last_path_created, error, "checkpoint escort_last_path_created"))
				return false;
			have_escort_last_path_created = true;
		} else if (name == "escort_goal_object") {
			if (!parse_int_field(it.value(), &parsed.escort_state.escort_goal_object, error, "checkpoint escort_goal_object"))
				return false;
		} else if (name == "escort_special_goal") {
			if (!parse_int_field(it.value(), &parsed.escort_state.escort_special_goal, error, "checkpoint escort_special_goal"))
				return false;
		} else if (name == "escort_goal_index") {
			if (!parse_int_field(it.value(), &parsed.escort_state.escort_goal_index, error, "checkpoint escort_goal_index"))
				return false;
		} else if (name == "buddy_messages_suppressed") {
			if (!parse_int_field(it.value(), &parsed.escort_state.buddy_messages_suppressed, error, "checkpoint buddy_messages_suppressed"))
				return false;
		} else if (name == "buddy_sorry_time") {
			if (!parse_int64_field(it.value(), &parsed.escort_state.buddy_sorry_time, error, "checkpoint buddy_sorry_time"))
				return false;
		} else if (name == "looking_for_marker") {
			if (!parse_int_field(it.value(), &parsed.escort_state.looking_for_marker, error, "checkpoint looking_for_marker"))
				return false;
		} else if (name == "last_buddy_key") {
			if (!parse_int_field(it.value(), &parsed.escort_state.last_buddy_key, error, "checkpoint last_buddy_key"))
				return false;
		} else if (name == "last_buddy_message_time") {
			if (!parse_int64_field(it.value(), &parsed.escort_state.last_buddy_message_time, error, "checkpoint last_buddy_message_time"))
				return false;
		} else if (name == "last_come_back_message_time") {
			if (!parse_int64_field(it.value(), &parsed.escort_state.last_come_back_message_time, error, "checkpoint last_come_back_message_time"))
				return false;
			have_last_come_back_message_time = true;
		} else if (name == "buddy_last_missile_time") {
			if (!parse_int64_field(it.value(), &parsed.escort_state.buddy_last_missile_time, error, "checkpoint buddy_last_missile_time"))
				return false;
			have_buddy_last_missile_time = true;
		} else if (name == "escort_owner_player") {
			if (!parse_int_field(it.value(), &parsed.escort_state.escort_owner_player, error, "checkpoint escort_owner_player"))
				return false;
		} else if (name == "thief_stolen_item_index") {
			if (!parse_int_field(it.value(), &parsed.thief_state.stolen_item_index, error, "checkpoint thief_stolen_item_index"))
				return false;
			have_thief_stolen_item_index = true;
		} else if (name == "re_init_thief_time") {
			if (!parse_int64_field(it.value(), &parsed.thief_state.re_init_thief_time, error, "checkpoint re_init_thief_time"))
				return false;
			have_re_init_thief_time = true;
		} else if (name == "last_thief_hit_time") {
			if (!parse_int64_field(it.value(), &parsed.thief_state.last_thief_hit_time, error, "checkpoint last_thief_hit_time"))
				return false;
			have_last_thief_hit_time = true;
		} else if (name == "data") {
			if (!it.value().is_string())
				return fail(error, "checkpoint data must be a string");
			parsed.data = it.value().get<std::string>();
		} else {
			return fail(error, "unknown checkpoint key: " + name);
		}
	}
	if (have_buddy_allowed_to_talk || have_buddy_last_seen_player ||
	    have_buddy_last_player_path_created || have_escort_last_path_created ||
	    have_last_come_back_message_time || have_buddy_last_missile_time) {
		if (!(have_buddy_allowed_to_talk && have_buddy_last_seen_player &&
		      have_buddy_last_player_path_created && have_escort_last_path_created &&
		      have_last_come_back_message_time && have_buddy_last_missile_time))
			return fail(error, "checkpoint escort state requires all guidebot fields");
		parsed.escort_state.valid = 1;
	}
	if (have_thief_stolen_item_index || have_re_init_thief_time || have_last_thief_hit_time) {
		if (!(have_thief_stolen_item_index && have_re_init_thief_time && have_last_thief_hit_time))
			return fail(error, "checkpoint thief state requires all thief fields");
		parsed.thief_state.valid = 1;
	}
	if (!validate_checkpoint(parsed, error))
		return false;
	*checkpoint = parsed;
	return true;
}

static bool checkpoint_record_to_json_line(const input_demo_checkpoint &checkpoint,
                                           std::string *line,
                                           std::string *error)
{
	ordered_json root = ordered_json::object();

	if (!line)
		return fail(error, "missing checkpoint text output");
	if (!validate_checkpoint(checkpoint, error))
		return false;
	root["type"] = "checkpoint";
	root["format"] = checkpoint.format;
	root["encoding"] = checkpoint.encoding;
	root["compression"] = checkpoint.compression;
	root["size"] = checkpoint.size;
	root["sha256"] = checkpoint.sha256;
	root["save_name"] = checkpoint.save_name;
	root["start_gt"] = checkpoint.start_gt;
	if (checkpoint.escort_state.valid) {
		root["buddy_allowed_to_talk"] = checkpoint.escort_state.buddy_allowed_to_talk;
		root["buddy_last_seen_player"] = checkpoint.escort_state.buddy_last_seen_player;
		root["buddy_last_player_path_created"] = checkpoint.escort_state.buddy_last_player_path_created;
		if (checkpoint.escort_state.escort_kill_object != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			root["escort_kill_object"] = checkpoint.escort_state.escort_kill_object;
		root["escort_last_path_created"] = checkpoint.escort_state.escort_last_path_created;
		if (checkpoint.escort_state.escort_goal_object != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			root["escort_goal_object"] = checkpoint.escort_state.escort_goal_object;
		if (checkpoint.escort_state.escort_special_goal != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			root["escort_special_goal"] = checkpoint.escort_state.escort_special_goal;
		if (checkpoint.escort_state.escort_goal_index != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			root["escort_goal_index"] = checkpoint.escort_state.escort_goal_index;
		if (checkpoint.escort_state.buddy_messages_suppressed != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			root["buddy_messages_suppressed"] = checkpoint.escort_state.buddy_messages_suppressed;
		if (checkpoint.escort_state.buddy_sorry_time != INPUT_DEMO_CHECKPOINT_ESCORT_I64_UNSET)
			root["buddy_sorry_time"] = checkpoint.escort_state.buddy_sorry_time;
		if (checkpoint.escort_state.looking_for_marker != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			root["looking_for_marker"] = checkpoint.escort_state.looking_for_marker;
		if (checkpoint.escort_state.last_buddy_key != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			root["last_buddy_key"] = checkpoint.escort_state.last_buddy_key;
		if (checkpoint.escort_state.last_buddy_message_time != INPUT_DEMO_CHECKPOINT_ESCORT_I64_UNSET)
			root["last_buddy_message_time"] = checkpoint.escort_state.last_buddy_message_time;
		root["last_come_back_message_time"] = checkpoint.escort_state.last_come_back_message_time;
		root["buddy_last_missile_time"] = checkpoint.escort_state.buddy_last_missile_time;
		if (checkpoint.escort_state.escort_owner_player != INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET)
			root["escort_owner_player"] = checkpoint.escort_state.escort_owner_player;
	}
	if (checkpoint.thief_state.valid) {
		root["thief_stolen_item_index"] = checkpoint.thief_state.stolen_item_index;
		root["re_init_thief_time"] = checkpoint.thief_state.re_init_thief_time;
		root["last_thief_hit_time"] = checkpoint.thief_state.last_thief_hit_time;
	}
	root["data"] = checkpoint.data;
	*line = root.dump();
	return true;
}

bool input_demo_metadata_parse_header_line(const std::string &line,
                                           input_demo_metadata *metadata, std::string *error)
{
	ordered_json root;
	ordered_json::const_iterator it;
	input_demo_metadata parsed;

	if (!metadata)
		return fail(error, "missing metadata output");
	if (!parse_json_line(line, "header", &root, error))
		return false;
	if (!parse_type(root, "header", error))
		return false;
	for (it = root.begin(); it != root.end(); ++it) {
		const std::string &name = it.key();

		if (name == "type") {
			continue;
		} else if (name == "version") {
			if (!parse_int_field(it.value(), &parsed.version, error, "version"))
				return false;
		} else if (name == "game") {
			if (!it.value().is_string())
				return fail(error, "metadata game must be a string");
			parsed.game = it.value().get<std::string>();
		} else if (name == "mission") {
			if (!it.value().is_string())
				return fail(error, "metadata mission must be a string");
			parsed.mission = it.value().get<std::string>();
		} else if (name == "build_number") {
			if (!parse_int_field(it.value(), &parsed.build_number, error, "build_number"))
				return false;
		} else if (name == "git_version") {
			if (!it.value().is_string())
				return fail(error, "metadata git_version must be a string");
			parsed.git_version = it.value().get<std::string>();
		} else if (name == "arch") {
			if (!it.value().is_string())
				return fail(error, "metadata arch must be a string");
			parsed.arch = it.value().get<std::string>();
		} else if (name == "level") {
			if (!parse_int_field(it.value(), &parsed.level, error, "level"))
				return false;
		} else if (name == "difficulty") {
			if (!parse_int_field(it.value(), &parsed.difficulty, error, "difficulty"))
				return false;
		} else if (name == "start_mode") {
			if (!it.value().is_string())
				return fail(error, "metadata start_mode must be a string");
			parsed.start_mode = it.value().get<std::string>();
		} else if (name == "rng_mode") {
			if (!it.value().is_string())
				return fail(error, "metadata rng_mode must be a string");
			parsed.rng_mode = it.value().get<std::string>();
		} else if (name == "frame_count") {
			if (!parse_uint32_field(it.value(), &parsed.frame_count, error, "frame_count"))
				return false;
		} else if (name == "classic_preview") {
			if (!it.value().is_string())
				return fail(error, "metadata classic_preview must be a string");
			parsed.classic_preview = it.value().get<std::string>();
		} else if (name == "start_save") {
			if (!it.value().is_string())
				return fail(error, "metadata start_save must be a string");
			parsed.start_save = it.value().get<std::string>();
		} else if (name == "player_cfg") {
			if (!parse_player_cfg(it.value(), &parsed.player_cfg, error))
				return false;
			parsed.has_player_cfg = true;
		} else {
			return fail(error, "unknown metadata key: " + name);
		}
	}
	if (!validate_metadata(parsed, error))
		return false;
	*metadata = parsed;
	return true;
}

bool input_demo_metadata_to_header_line(const input_demo_metadata &metadata,
                                        std::string *line, std::string *error)
{
	ordered_json root = ordered_json::object();

	if (!line)
		return fail(error, "missing metadata text output");
	if (!validate_metadata(metadata, error))
		return false;
	root["type"] = "header";
	root["version"] = metadata.version;
	root["game"] = metadata.game;
	root["mission"] = metadata.mission;
	root["build_number"] = metadata.build_number;
	root["git_version"] = metadata.git_version;
	root["arch"] = metadata.arch;
	root["level"] = metadata.level;
	root["difficulty"] = metadata.difficulty;
	root["start_mode"] = metadata.start_mode;
	root["rng_mode"] = metadata.rng_mode;
	root["frame_count"] = metadata.frame_count;
	if (!metadata.classic_preview.empty())
		root["classic_preview"] = metadata.classic_preview;
	if (!metadata.start_save.empty())
		root["start_save"] = metadata.start_save;
	if (metadata.has_player_cfg) {
		ordered_json player_cfg = ordered_json::object();

		player_cfg_to_json(metadata.player_cfg, &player_cfg);
		root["player_cfg"] = player_cfg;
	}
	*line = root.dump();
	return true;
}

static bool validate_frame_record(const input_demo_file_frame &frame, uint32_t expected_frame,
                                  int game, std::string *error)
{
	if (frame.input.frame != expected_frame || frame.rng.frame != expected_frame)
		return fail(error, "demo frame indexes must be contiguous");
	if (frame.input.run_length != 1)
		return fail(error, "demo frame input records must not use n");
	if (frame.rng.run_length != 1)
		return fail(error, "demo frame rng records must not use n");
	if (!expected_frame && !frame.input.has_frame_time)
		return fail(error, "first demo frame must include ft");
	if (game == INPUT_DEMO_GAME_D1 && input_demo_control_record_has_d2_fields(&frame.input))
		return fail(error, "D1 demo frames must reject D2-only control keys");
	return validate_rng_record(frame.rng, error);
}

static bool validate_demo_file(const input_demo_file &demo, std::string *error)
{
	int game;
	size_t i;

	if (!validate_metadata(demo.metadata, error))
		return false;
	if (demo.metadata.start_mode == "save_checkpoint") {
		if (!demo.has_checkpoint)
			return fail(error, "save_checkpoint demos must include a checkpoint record");
		if (!validate_checkpoint(demo.checkpoint, error))
			return false;
	} else if (demo.has_checkpoint)
		return fail(error, "new_level demos must not include a checkpoint record");
	game = game_id_from_name(demo.metadata.game);
	if (!game)
		return fail(error, "demo game must be d1 or d2");
	if (demo.frames.size() != demo.metadata.frame_count)
		return fail(error, "demo frame count does not match header");
	if (!demo.has_result)
		return fail(error, "demo file is missing result trailer");
	if (demo.result.frame_count != demo.metadata.frame_count)
		return fail(error, "demo result frame count does not match header");
	for (i = 0; i != demo.frames.size(); ++i) {
		if (!validate_frame_record(demo.frames[i], static_cast<uint32_t>(i), game, error))
			return false;
	}
	return true;
}

static bool parse_frame_record(const ordered_json &root, int game, uint32_t expected_frame,
                               input_demo_file_frame *frame, std::string *error)
{
	ordered_json control = ordered_json::object();
	ordered_json rng = ordered_json::object();
	ordered_json state = ordered_json::object();
	ordered_json diag = ordered_json::object();
	ordered_json events = ordered_json::array();
	ordered_json::const_iterator it;
	uint32_t parsed_frame = 0;
	bool have_frame = false;
	bool have_input = false;
	bool have_rng = false;
	bool have_state = false;
	bool have_diag = false;
	bool have_events = false;
	std::string line_error;

	if (!frame)
		return fail(error, "missing demo frame output");
	if (!parse_type(root, "frame", error))
		return false;
	for (it = root.begin(); it != root.end(); ++it) {
		const std::string &name = it.key();

		if (name == "type") {
			continue;
		} else if (name == "f") {
			if (!parse_uint32_field(it.value(), &parsed_frame, error, "f"))
				return false;
			have_frame = true;
		} else if (name == "ft") {
			control["ft"] = it.value();
		} else if (name == "input") {
			if (!it.value().is_object())
				return fail(error, "frame input must be an object");
			for (ordered_json::const_iterator input_it = it.value().begin(); input_it != it.value().end(); ++input_it) {
				if (input_it.key() != "s" && input_it.key() != "p")
					return fail(error, "unknown frame input key: " + input_it.key());
				control[input_it.key()] = input_it.value();
			}
			have_input = true;
		} else if (name == "rng") {
			if (!it.value().is_object())
				return fail(error, "frame rng must be an object");
			for (ordered_json::const_iterator rng_it = it.value().begin(); rng_it != it.value().end(); ++rng_it) {
				if (rng_it.key() != "s" && rng_it.key() != "c")
					return fail(error, "unknown frame rng key: " + rng_it.key());
				rng[rng_it.key()] = rng_it.value();
			}
			have_rng = true;
		} else if (name == "state") {
			if (!it.value().is_object())
				return fail(error, "frame state must be an object");
			state = it.value();
			have_state = true;
		} else if (name == "diag") {
			if (!it.value().is_object())
				return fail(error, "frame diag must be an object");
			diag = it.value();
			have_diag = true;
		} else if (name == "events") {
			size_t event_index;

			if (!it.value().is_array())
				return fail(error, "frame events must be an array");
			for (event_index = 0; event_index != it.value().size(); ++event_index)
				if (!it.value()[event_index].is_object())
					return fail(error, "frame events entries must be objects");
			events = it.value();
			have_events = true;
		} else {
			return fail(error, "unknown frame key: " + name);
		}
	}
	if (!have_frame)
		return fail(error, "frame record is missing f");
	if (parsed_frame != expected_frame)
		return fail(error, "demo frame indexes must be contiguous");
	if (!have_input)
		return fail(error, "frame record is missing input");
	if (!have_rng)
		return fail(error, "frame record is missing rng");
	control["f"] = parsed_frame;
	rng["f"] = parsed_frame;
	if (!input_demo_control_record_parse_json_line(control.dump(), game, &frame->input, &line_error))
		return fail(error, "frame input: " + line_error);
	if (!input_demo_rng_record_parse_json_line(rng.dump(), &frame->rng, &line_error))
		return fail(error, "frame rng: " + line_error);
	if (have_state) {
		if (!input_demo_result_parse_snapshot_json_text(state.dump(), &frame->state, &line_error))
			return fail(error, "frame state: " + line_error);
		frame->has_state = true;
	}
	if (have_diag) {
		frame->has_diag = true;
		frame->diag_json = diag.dump();
	}
	if (have_events) {
		size_t event_index;

		for (event_index = 0; event_index != events.size(); ++event_index)
			frame->events.push_back(events[event_index].dump());
	}
	return validate_frame_record(*frame, expected_frame, game, error);
}

static bool frame_record_to_json_line(const input_demo_file_frame &frame, int game,
                                      std::string *line, std::string *error)
{
	ordered_json root = ordered_json::object();
	ordered_json input = ordered_json::object();
	ordered_json rng = ordered_json::object();
	ordered_json state = ordered_json::object();
	ordered_json diag = ordered_json::object();
	ordered_json events = ordered_json::array();
	ordered_json control_record;
	ordered_json rng_record;
	ordered_json state_record;
	ordered_json diag_record;
	std::string control_line;
	std::string rng_line;
	std::string state_line;
	size_t event_index;

	if (!input_demo_control_record_to_json_line(frame.input, game, &control_line, error))
		return false;
	if (!input_demo_rng_record_to_json_line(frame.rng, &rng_line, error))
		return false;
	if (!parse_json_line(control_line, "frame input", &control_record, error))
		return false;
	if (!parse_json_line(rng_line, "frame rng", &rng_record, error))
		return false;
	if (frame.has_state) {
		if (!input_demo_result_snapshot_to_json_text(frame.state, &state_line, error))
			return false;
		if (!parse_json_line(state_line, "frame state", &state_record, error))
			return false;
	}
	root["type"] = "frame";
	root["f"] = frame.input.frame;
	if (control_record.contains("ft"))
		root["ft"] = control_record.at("ft");
	if (control_record.contains("s"))
		input["s"] = control_record.at("s");
	if (control_record.contains("p"))
		input["p"] = control_record.at("p");
	root["input"] = std::move(input);
	if (rng_record.contains("s"))
		rng["s"] = rng_record.at("s");
	if (rng_record.contains("c"))
		rng["c"] = rng_record.at("c");
	root["rng"] = std::move(rng);
	if (frame.has_state) {
		state = std::move(state_record);
		root["state"] = std::move(state);
	}
	if (frame.has_diag) {
		if (!parse_json_line(frame.diag_json, "frame diag", &diag_record, error))
			return false;
		diag = std::move(diag_record);
		root["diag"] = std::move(diag);
	}
	for (event_index = 0; event_index != frame.events.size(); ++event_index) {
		ordered_json event_record;

		if (!parse_json_line(frame.events[event_index], "frame event", &event_record, error))
			return false;
		events.push_back(std::move(event_record));
	}
	if (!events.empty())
		root["events"] = std::move(events);
	*line = root.dump();
	return true;
}

bool input_demo_file_parse_text(const std::string &text,
                                input_demo_file *demo, std::string *error)
{
	std::istringstream input(text);
	input_demo_file parsed;
	std::string line;
	uint32_t expected_frame = 0;
	unsigned int line_number = 0;
	bool have_header = false;
	bool have_checkpoint = false;
	bool have_result = false;

	if (!demo)
		return fail(error, "missing demo file output");
	while (std::getline(input, line)) {
		ordered_json root;
		std::string record_type;
		std::string line_error;

		line_number++;
		if (is_blank_or_comment_line(line))
			continue;
		if (!parse_json_line(line, "demo record", &root, &line_error))
			return fail(error, "demo line " + std::to_string(line_number) + ": " + line_error);
		if (!root.contains("type") || !root.at("type").is_string())
			return fail(error, "demo line " + std::to_string(line_number) + ": record type is required");
		record_type = root.at("type").get<std::string>();
		if (!have_header) {
			if (record_type != "header")
				return fail(error, "first demo record must be header");
			if (!input_demo_metadata_parse_header_line(line, &parsed.metadata, &line_error))
				return fail(error, "demo line " + std::to_string(line_number) + ": " + line_error);
			have_header = true;
			continue;
		}
		if (have_result)
			return fail(error, "demo result trailer must be the final record");
		if (record_type == "frame") {
			input_demo_file_frame frame;

			if (!parse_frame_record(root, game_id_from_name(parsed.metadata.game), expected_frame, &frame, &line_error))
				return fail(error, "demo line " + std::to_string(line_number) + ": " + line_error);
			parsed.frames.push_back(frame);
			expected_frame++;
		} else if (record_type == "checkpoint") {
			if (have_checkpoint)
				return fail(error, "demo line " + std::to_string(line_number) + ": duplicate checkpoint record");
			if (expected_frame)
				return fail(error, "demo line " + std::to_string(line_number) + ": checkpoint record must appear before frames");
			if (!parse_checkpoint_record(root, &parsed.checkpoint, &line_error))
				return fail(error, "demo line " + std::to_string(line_number) + ": " + line_error);
			parsed.has_checkpoint = true;
			have_checkpoint = true;
		} else if (record_type == "result") {
			if (!root.contains("result"))
				return fail(error, "demo line " + std::to_string(line_number) + ": result record is missing result");
			if (!input_demo_result_parse_json_text(root.at("result").dump(), &parsed.result, &line_error))
				return fail(error, "demo line " + std::to_string(line_number) + ": " + line_error);
			parsed.has_result = true;
			have_result = true;
		} else {
			return fail(error, "demo line " + std::to_string(line_number) + ": unknown record type: " + record_type);
		}
	}
	if (!have_header)
		return fail(error, "demo file is missing header");
	if (!validate_demo_file(parsed, error))
		return false;
	*demo = parsed;
	return true;
}

bool input_demo_file_read(const char *path,
                          input_demo_file *demo, std::string *error)
{
	std::ifstream in(path, std::ios::in | std::ios::binary);
	std::ostringstream text;

	if (!demo)
		return fail(error, "missing demo file output");
	if (!path || !path[0])
		return fail(error, "missing demo file path");
	if (!in)
		return fail(error, std::string("could not open demo file: ") + path);
	text << in.rdbuf();
	if (in.bad())
		return fail(error, std::string("could not read demo file: ") + path);
	return input_demo_file_parse_text(text.str(), demo, error);
}

bool input_demo_file_to_text(const input_demo_file &demo,
                             std::string *text, std::string *error)
{
	std::string line;
	std::string result_text;
	ordered_json result_record = ordered_json::object();
	ordered_json result_json;
	int game;
	size_t i;

	if (!text)
		return fail(error, "missing demo file text output");
	if (!validate_demo_file(demo, error))
		return false;
	game = game_id_from_name(demo.metadata.game);
	text->clear();
	if (!input_demo_metadata_to_header_line(demo.metadata, &line, error))
		return false;
	*text += line;
	text->push_back('\n');
	if (demo.has_checkpoint) {
		if (!checkpoint_record_to_json_line(demo.checkpoint, &line, error))
			return false;
		*text += line;
		text->push_back('\n');
	}
	for (i = 0; i != demo.frames.size(); ++i) {
		if (!frame_record_to_json_line(demo.frames[i], game, &line, error))
			return false;
		*text += line;
		text->push_back('\n');
	}
	if (!input_demo_result_to_json_text(demo.result, &result_text, error))
		return false;
	try {
		result_json = ordered_json::parse(result_text);
	} catch (const std::exception &e) {
		return fail(error, std::string("could not build result trailer: ") + e.what());
	}
	result_record["type"] = "result";
	result_record["result"] = std::move(result_json);
	*text += result_record.dump();
	text->push_back('\n');
	return true;
}

bool input_demo_file_write(const char *path,
                           const input_demo_file &demo, std::string *error)
{
	std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
	std::string text;

	if (!path || !path[0])
		return fail(error, "missing demo file path");
	if (!input_demo_file_to_text(demo, &text, error))
		return false;
	if (!out)
		return fail(error, std::string("could not open demo file: ") + path);
	out << text;
	if (!out.good())
		return fail(error, std::string("could not write demo file: ") + path);
	return true;
}