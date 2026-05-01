#include "input_demo_result.h"

#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>
#include <string>

#include <nlohmann/json.hpp>

namespace
{

static int input_demo_result_copy_error(const std::string &message,
                                        char *error, size_t error_size)
{
	if (error && error_size)
		snprintf(error, error_size, "%s", message.c_str());
	return 0;
}

static bool input_demo_result_read_text_file(const char *path, std::string *text, std::string *error)
{
	std::ifstream input(path, std::ios::binary);
	std::ostringstream buffer;

	if (!input.is_open())
		return input_demo_result_copy_error(std::string("could not open result file for reading: ") + path,
		                                    NULL, 0),
		       error ? (*error = std::string("could not open result file for reading: ") + path, false) : false;
	buffer << input.rdbuf();
	if (!input.good() && !input.eof()) {
		if (error)
			*error = std::string("could not read result file: ") + path;
		return false;
	}
	if (text)
		*text = buffer.str();
	return true;
}

static bool input_demo_result_key_allowed(const std::string &key,
                                          const char *const *allowed_keys,
                                          size_t allowed_count)
{
	size_t i;

	for (i = 0; i != allowed_count; ++i) {
		if (key == allowed_keys[i])
			return true;
	}
	return false;
}

static bool input_demo_result_parse_ammo_array(const nlohmann::json &array,
                                               uint16_t *ammo, size_t ammo_count,
                                               std::string *error, const char *label)
{
	if (!array.is_array())
		return error ? (*error = std::string(label) + " must be an array", false) : false;
	if (array.size() != ammo_count)
		return error ? (*error = std::string(label) + " must have exactly " + std::to_string(ammo_count) + " entries", false) : false;
	for (size_t i = 0; i != ammo_count; ++i) {
		if (!array[i].is_number_integer() && !array[i].is_number_unsigned())
			return error ? (*error = std::string(label) + " contains a non-integer ammo value", false) : false;
		ammo[i] = (uint16_t) array[i].get<unsigned int>();
	}
	return true;
}

static bool input_demo_result_parse_player(const nlohmann::json &player_json,
                                           input_demo_result_player *player,
                                           std::string *error)
{
	static const char *const allowed_keys[] = {
		"energy", "shields", "score", "lives", "laser_level", "primary_weapon",
		"secondary_weapon", "flags", "hostages", "primary_ammo", "secondary_ammo"
	};

	if (!player_json.is_object())
		return error ? (*error = "player0 must be an object", false) : false;
	input_demo_result_player_clear(player);
	player->present = 1;
	for (nlohmann::json::const_iterator it = player_json.begin(); it != player_json.end(); ++it) {
		if (!input_demo_result_key_allowed(it.key(), allowed_keys, sizeof(allowed_keys) / sizeof(allowed_keys[0])))
			return error ? (*error = std::string("unknown player0 key: ") + it.key(), false) : false;
	}
	if (player_json.contains("energy"))
		player->energy = player_json.at("energy").get<int32_t>();
	if (player_json.contains("shields"))
		player->shields = player_json.at("shields").get<int32_t>();
	if (player_json.contains("score"))
		player->score = player_json.at("score").get<int32_t>();
	if (player_json.contains("lives"))
		player->lives = player_json.at("lives").get<int16_t>();
	if (player_json.contains("laser_level"))
		player->laser_level = player_json.at("laser_level").get<int16_t>();
	if (player_json.contains("primary_weapon"))
		player->primary_weapon = player_json.at("primary_weapon").get<int16_t>();
	if (player_json.contains("secondary_weapon"))
		player->secondary_weapon = player_json.at("secondary_weapon").get<int16_t>();
	if (player_json.contains("flags"))
		player->flags = player_json.at("flags").get<uint32_t>();
	if (player_json.contains("hostages"))
		player->hostages = player_json.at("hostages").get<uint16_t>();
	if (player_json.contains("primary_ammo") && !input_demo_result_parse_ammo_array(
	                                                player_json.at("primary_ammo"), player->primary_ammo, INPUT_DEMO_RESULT_MAX_PRIMARY_AMMO, error, "player0.primary_ammo"))
		return false;
	if (player_json.contains("secondary_ammo") && !input_demo_result_parse_ammo_array(
	                                                  player_json.at("secondary_ammo"), player->secondary_ammo, INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO, error, "player0.secondary_ammo"))
		return false;
	return true;
}

static bool input_demo_result_parse_position(const nlohmann::json &position_json,
                                             input_demo_result_position *position,
                                             std::string *error)
{
	static const char *const allowed_keys[] = { "segment", "x", "y", "z", "forward_x", "forward_y", "forward_z" };

	if (!position_json.is_object())
		return error ? (*error = "position must be an object", false) : false;
	input_demo_result_position_clear(position);
	position->present = 1;
	for (nlohmann::json::const_iterator it = position_json.begin(); it != position_json.end(); ++it) {
		if (!input_demo_result_key_allowed(it.key(), allowed_keys, sizeof(allowed_keys) / sizeof(allowed_keys[0])))
			return error ? (*error = std::string("unknown position key: ") + it.key(), false) : false;
	}
	if (!position_json.contains("segment") || !position_json.contains("x") || !position_json.contains("y") ||
	    !position_json.contains("z"))
		return error ? (*error = "position must include segment, x, y, and z", false) : false;
	position->segment = position_json.at("segment").get<int32_t>();
	position->x = position_json.at("x").get<int32_t>();
	position->y = position_json.at("y").get<int32_t>();
	position->z = position_json.at("z").get<int32_t>();
	if (position_json.contains("forward_x") || position_json.contains("forward_y") || position_json.contains("forward_z")) {
		position->has_forward = 1;
		if (position_json.contains("forward_x"))
			position->fx = position_json.at("forward_x").get<int32_t>();
		if (position_json.contains("forward_y"))
			position->fy = position_json.at("forward_y").get<int32_t>();
		if (position_json.contains("forward_z"))
			position->fz = position_json.at("forward_z").get<int32_t>();
	}
	return true;
}

static bool input_demo_result_parse_level_summary(const nlohmann::json &level_json,
                                                  input_demo_result_level *level,
                                                  std::string *error)
{
	static const char *const allowed_keys[] = {
		"robots_alive", "robots_killed", "hostages_remaining", "powerups_remaining",
		"control_center_destroyed", "endlevel_completed"
	};

	if (!level_json.is_object())
		return error ? (*error = "level_summary must be an object", false) : false;
	input_demo_result_level_clear(level);
	level->present = 1;
	for (nlohmann::json::const_iterator it = level_json.begin(); it != level_json.end(); ++it) {
		if (!input_demo_result_key_allowed(it.key(), allowed_keys, sizeof(allowed_keys) / sizeof(allowed_keys[0])))
			return error ? (*error = std::string("unknown level_summary key: ") + it.key(), false) : false;
	}
	if (level_json.contains("robots_alive"))
		level->robots_alive = level_json.at("robots_alive").get<int32_t>();
	if (level_json.contains("robots_killed"))
		level->robots_killed = level_json.at("robots_killed").get<int32_t>();
	if (level_json.contains("hostages_remaining"))
		level->hostages_remaining = level_json.at("hostages_remaining").get<int32_t>();
	if (level_json.contains("powerups_remaining"))
		level->powerups_remaining = level_json.at("powerups_remaining").get<int32_t>();
	if (level_json.contains("control_center_destroyed"))
		level->control_center_destroyed = level_json.at("control_center_destroyed").get<bool>() ? 1 : 0;
	if (level_json.contains("endlevel_completed"))
		level->endlevel_completed = level_json.at("endlevel_completed").get<bool>() ? 1 : 0;
	return true;
}

static bool input_demo_result_compare_int64(const char *field, const char *label,
                                            int64_t expected, int64_t actual,
                                            std::string *error)
{
	if (expected == actual)
		return true;
	if (error)
		*error = std::string(field) + " (" + label + ") mismatch: expected " + std::to_string(expected) +
		         ", actual " + std::to_string(actual);
	return false;
}

static bool input_demo_result_compare_string(const char *field, const char *label,
                                             const char *expected, const char *actual,
                                             std::string *error)
{
	if (strcmp(expected, actual) == 0)
		return true;
	if (error)
		*error = std::string(field) + " (" + label + ") mismatch: expected '" + expected +
		         "', actual '" + actual + "'";
	return false;
}

static bool input_demo_result_compare_player(const input_demo_result_player *expected,
                                             const input_demo_result_player *actual,
                                             std::string *error)
{
	size_t i;

	if (!actual->present)
		return error ? (*error = "player0 (player summary) missing from actual result", false) : false;
	if (!input_demo_result_compare_int64("player0.energy", "player energy", expected->energy, actual->energy, error) ||
	    !input_demo_result_compare_int64("player0.shields", "player shields", expected->shields, actual->shields, error) ||
	    !input_demo_result_compare_int64("player0.score", "player score", expected->score, actual->score, error) ||
	    !input_demo_result_compare_int64("player0.lives", "player lives", expected->lives, actual->lives, error) ||
	    !input_demo_result_compare_int64("player0.laser_level", "player laser level", expected->laser_level, actual->laser_level, error) ||
	    !input_demo_result_compare_int64("player0.primary_weapon", "player primary weapon", expected->primary_weapon, actual->primary_weapon, error) ||
	    !input_demo_result_compare_int64("player0.secondary_weapon", "player secondary weapon", expected->secondary_weapon, actual->secondary_weapon, error) ||
	    !input_demo_result_compare_int64("player0.flags", "player flags", expected->flags, actual->flags, error) ||
	    !input_demo_result_compare_int64("player0.hostages", "player hostages", expected->hostages, actual->hostages, error))
		return false;
	for (i = 0; i != INPUT_DEMO_RESULT_MAX_PRIMARY_AMMO; ++i) {
		const std::string field = std::string("player0.primary_ammo[") + std::to_string(i) + "]";
		if (!input_demo_result_compare_int64(field.c_str(), "player primary ammo",
		                                     expected->primary_ammo[i], actual->primary_ammo[i], error))
			return false;
	}
	for (i = 0; i != INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO; ++i) {
		const std::string field = std::string("player0.secondary_ammo[") + std::to_string(i) + "]";
		if (!input_demo_result_compare_int64(field.c_str(), "player secondary ammo",
		                                     expected->secondary_ammo[i], actual->secondary_ammo[i], error))
			return false;
	}
	return true;
}

static bool input_demo_result_compare_position(const input_demo_result_position *expected,
                                               const input_demo_result_position *actual,
                                               std::string *error)
{
	if (!actual->present)
		return error ? (*error = "position (player position) missing from actual result", false) : false;
	if (!input_demo_result_compare_int64("position.segment", "player segment", expected->segment, actual->segment, error) ||
	    !input_demo_result_compare_int64("position.x", "player x", expected->x, actual->x, error) ||
	    !input_demo_result_compare_int64("position.y", "player y", expected->y, actual->y, error) ||
	    !input_demo_result_compare_int64("position.z", "player z", expected->z, actual->z, error) ||
	    !input_demo_result_compare_int64("position.forward_x", "player forward x", expected->fx, actual->fx, error) ||
	    !input_demo_result_compare_int64("position.forward_y", "player forward y", expected->fy, actual->fy, error) ||
	    !input_demo_result_compare_int64("position.forward_z", "player forward z", expected->fz, actual->fz, error))
		return false;
	return true;
}

static bool input_demo_result_compare_level_summary(const input_demo_result_level *expected,
                                                    const input_demo_result_level *actual,
                                                    std::string *error)
{
	if (!actual->present)
		return error ? (*error = "level_summary (level summary) missing from actual result", false) : false;
	if (!input_demo_result_compare_int64("level_summary.robots_alive", "robots alive", expected->robots_alive, actual->robots_alive, error) ||
	    !input_demo_result_compare_int64("level_summary.robots_killed", "robots killed", expected->robots_killed, actual->robots_killed, error) ||
	    !input_demo_result_compare_int64("level_summary.hostages_remaining", "hostages remaining", expected->hostages_remaining, actual->hostages_remaining, error) ||
	    !input_demo_result_compare_int64("level_summary.powerups_remaining", "powerups remaining", expected->powerups_remaining, actual->powerups_remaining, error) ||
	    !input_demo_result_compare_int64("level_summary.control_center_destroyed", "control center destroyed", expected->control_center_destroyed,
	                                     actual->control_center_destroyed, error) ||
	    !input_demo_result_compare_int64("level_summary.endlevel_completed", "endlevel completed", expected->endlevel_completed,
	                                     actual->endlevel_completed, error))
		return false;
	return true;
}

static nlohmann::ordered_json input_demo_result_ammo_array(const uint16_t *ammo, size_t count)
{
	nlohmann::ordered_json array = nlohmann::ordered_json::array();
	size_t i;

	for (i = 0; i != count; ++i) {
		array.push_back(ammo[i]);
	}
	return array;
}

static bool input_demo_result_parse_json_object(const nlohmann::json &root,
                                                input_demo_result *result,
                                                std::string *error,
                                                bool require_metadata)

{
	static const char *const result_allowed_keys[] = {
		"version", "game", "mission", "level", "difficulty", "frame_count",
		"game_time64", "player0", "position", "level_summary"
	};
	static const char *const snapshot_allowed_keys[] = {
		"game_time64", "player0", "position", "level_summary"
	};
	const char *const *allowed_keys = require_metadata ? result_allowed_keys : snapshot_allowed_keys;
	const size_t allowed_key_count = require_metadata ? sizeof(result_allowed_keys) / sizeof(result_allowed_keys[0])
	                                                  : sizeof(snapshot_allowed_keys) / sizeof(snapshot_allowed_keys[0]);

	if (!root.is_object())
		return error ? (*error = require_metadata ? "result root must be an object" : "snapshot root must be an object", false) : false;
	for (nlohmann::json::const_iterator it = root.begin(); it != root.end(); ++it) {
		if (!input_demo_result_key_allowed(it.key(), allowed_keys, allowed_key_count))
			return error ? (*error = std::string(require_metadata ? "unknown result key: " : "unknown snapshot key: ") + it.key(), false) : false;
	}
	if (require_metadata) {
		if (!root.contains("version") || !root.contains("game") || !root.contains("mission") || !root.contains("level") ||
		    !root.contains("difficulty") || !root.contains("frame_count"))
			return error ? (*error = "result is missing one or more required keys: version, game, mission, level, difficulty, frame_count", false) : false;
	}
	input_demo_result_clear(result);
	if (require_metadata) {
		result->version = root.at("version").get<int32_t>();
		snprintf(result->game, sizeof(result->game), "%s", root.at("game").get_ref<const std::string &>().c_str());
		snprintf(result->mission, sizeof(result->mission), "%s", root.at("mission").get_ref<const std::string &>().c_str());
		result->level = root.at("level").get<int32_t>();
		result->difficulty = root.at("difficulty").get<int32_t>();
		result->frame_count = root.at("frame_count").get<uint32_t>();
	}
	if (root.contains("game_time64")) {
		result->has_game_time64 = 1;
		result->game_time64 = root.at("game_time64").get<int64_t>();
	}
	if (root.contains("player0") && !input_demo_result_parse_player(root.at("player0"), &result->player0, error))
		return false;
	if (root.contains("position") && !input_demo_result_parse_position(root.at("position"), &result->position, error))
		return false;
	if (root.contains("level_summary") && !input_demo_result_parse_level_summary(root.at("level_summary"), &result->level_summary, error))
		return false;
	return true;
}

static nlohmann::ordered_json input_demo_result_to_json_object(const input_demo_result *result,
                                                               bool include_metadata)
{
	nlohmann::ordered_json root = nlohmann::ordered_json::object();

	if (include_metadata) {
		root["version"] = result->version ? result->version : 2;
		root["game"] = result->game;
		root["mission"] = result->mission;
		root["level"] = result->level;
		root["difficulty"] = result->difficulty;
		root["frame_count"] = result->frame_count;
	}
	if (result->has_game_time64)
		root["game_time64"] = result->game_time64;

	if (result->player0.present) {
		nlohmann::ordered_json player = nlohmann::ordered_json::object();
		nlohmann::ordered_json primary_ammo = input_demo_result_ammo_array(
		    result->player0.primary_ammo, INPUT_DEMO_RESULT_MAX_PRIMARY_AMMO);
		nlohmann::ordered_json secondary_ammo = input_demo_result_ammo_array(
		    result->player0.secondary_ammo, INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO);

		player["energy"] = result->player0.energy;
		player["shields"] = result->player0.shields;
		player["score"] = result->player0.score;
		player["lives"] = result->player0.lives;
		player["laser_level"] = result->player0.laser_level;
		player["primary_weapon"] = result->player0.primary_weapon;
		player["secondary_weapon"] = result->player0.secondary_weapon;
		player["flags"] = result->player0.flags;
		player["hostages"] = result->player0.hostages;
		player["primary_ammo"] = std::move(primary_ammo);
		player["secondary_ammo"] = std::move(secondary_ammo);
		root["player0"] = std::move(player);
	}

	if (result->position.present) {
		nlohmann::ordered_json position = nlohmann::ordered_json::object();

		position["segment"] = result->position.segment;
		position["x"] = result->position.x;
		position["y"] = result->position.y;
		position["z"] = result->position.z;
		if (result->position.has_forward) {
			position["forward_x"] = result->position.fx;
			position["forward_y"] = result->position.fy;
			position["forward_z"] = result->position.fz;
		}
		root["position"] = std::move(position);
	}

	if (result->level_summary.present) {
		nlohmann::ordered_json level = nlohmann::ordered_json::object();

		level["robots_alive"] = result->level_summary.robots_alive;
		level["robots_killed"] = result->level_summary.robots_killed;
		level["hostages_remaining"] = result->level_summary.hostages_remaining;
		level["powerups_remaining"] = result->level_summary.powerups_remaining;
		level["control_center_destroyed"] = result->level_summary.control_center_destroyed ? true : false;
		level["endlevel_completed"] = result->level_summary.endlevel_completed ? true : false;
		root["level_summary"] = std::move(level);
	}

	return root;
}

static int input_demo_result_compare_internal(const input_demo_result *expected,
                                              const input_demo_result *actual,
                                              bool compare_metadata,
                                              char *error,
                                              size_t error_size)
{
	std::string compare_error;

	if (!expected || !actual)
		return input_demo_result_copy_error("missing result comparison input", error, error_size);
	if (compare_metadata &&
	    (!input_demo_result_compare_int64("version", "schema version", expected->version, actual->version, &compare_error) ||
	     !input_demo_result_compare_string("game", "game id", expected->game, actual->game, &compare_error) ||
	     !input_demo_result_compare_string("mission", "mission id", expected->mission, actual->mission, &compare_error) ||
	     !input_demo_result_compare_int64("level", "level", expected->level, actual->level, &compare_error) ||
	     !input_demo_result_compare_int64("difficulty", "difficulty", expected->difficulty, actual->difficulty, &compare_error) ||
	     !input_demo_result_compare_int64("frame_count", "frame count", expected->frame_count, actual->frame_count, &compare_error)))
		return input_demo_result_copy_error(compare_error, error, error_size);
	if (expected->has_game_time64 &&
	    !input_demo_result_compare_int64("game_time64", compare_metadata ? "final GameTime64" : "frame GameTime64",
	                                     expected->game_time64, actual->game_time64, &compare_error))
		return input_demo_result_copy_error(compare_error, error, error_size);
	if (expected->player0.present && !input_demo_result_compare_player(&expected->player0, &actual->player0, &compare_error))
		return input_demo_result_copy_error(compare_error, error, error_size);
	if (expected->position.present && !input_demo_result_compare_position(&expected->position, &actual->position, &compare_error))
		return input_demo_result_copy_error(compare_error, error, error_size);
	if (expected->level_summary.present &&
	    !input_demo_result_compare_level_summary(&expected->level_summary, &actual->level_summary, &compare_error))
		return input_demo_result_copy_error(compare_error, error, error_size);
	return 1;
}

} // namespace

bool input_demo_result_parse_json_text(const std::string &text,
                                       input_demo_result *result,
                                       std::string *error)
{
	nlohmann::json root;

	if (!result)
		return error ? (*error = "missing input demo result output", false) : false;
	try {
		root = nlohmann::json::parse(text);
	} catch (const std::exception &ex) {
		return error ? (*error = std::string("could not parse result json: ") + ex.what(), false) : false;
	}
	return input_demo_result_parse_json_object(root, result, error, true);
}

bool input_demo_result_parse_snapshot_json_text(const std::string &text,
                                                input_demo_result *result,
                                                std::string *error)
{
	nlohmann::json root;

	if (!result)
		return error ? (*error = "missing input demo snapshot output", false) : false;
	try {
		root = nlohmann::json::parse(text);
	} catch (const std::exception &ex) {
		return error ? (*error = std::string("could not parse snapshot json: ") + ex.what(), false) : false;
	}
	return input_demo_result_parse_json_object(root, result, error, false);
}

bool input_demo_result_to_json_text(const input_demo_result &result,
                                    std::string *text,
                                    std::string *error)
{
	if (!text)
		return error ? (*error = "missing result text output", false) : false;
	*text = input_demo_result_to_json_object(&result, true).dump(2);
	text->push_back('\n');
	return true;
}

bool input_demo_result_snapshot_to_json_text(const input_demo_result &result,
                                             std::string *text,
                                             std::string *error)
{
	if (!text)
		return error ? (*error = "missing snapshot text output", false) : false;
	*text = input_demo_result_to_json_object(&result, false).dump();
	return true;
}

extern "C" {

void input_demo_result_player_clear(input_demo_result_player *player)
{
	if (!player)
		return;
	memset(player, 0, sizeof(*player));
}

void input_demo_result_position_clear(input_demo_result_position *position)
{
	if (!position)
		return;
	memset(position, 0, sizeof(*position));
}

void input_demo_result_level_clear(input_demo_result_level *level)
{
	if (!level)
		return;
	memset(level, 0, sizeof(*level));
}

void input_demo_result_clear(input_demo_result *result)
{
	if (!result)
		return;
	memset(result, 0, sizeof(*result));
	result->version = 2;
	input_demo_result_player_clear(&result->player0);
	input_demo_result_position_clear(&result->position);
	input_demo_result_level_clear(&result->level_summary);
}

int input_demo_result_read_json_file(const char *path,
                                     input_demo_result *result,
                                     char *error, size_t error_size)
{
	std::string text;
	std::string parse_error;

	if (!path || !path[0])
		return input_demo_result_copy_error("missing result input path", error, error_size);
	if (!result)
		return input_demo_result_copy_error("missing input demo result output", error, error_size);
	if (!input_demo_result_read_text_file(path, &text, &parse_error))
		return input_demo_result_copy_error(parse_error, error, error_size);
	if (!input_demo_result_parse_json_text(text, result, &parse_error))
		return input_demo_result_copy_error(parse_error, error, error_size);
	return 1;
}

int input_demo_result_write_json_file(const char *path,
                                      const input_demo_result *result,
                                      char *error, size_t error_size)
{
	std::ofstream output;
	std::string text;
	std::string text_error;

	if (!path || !path[0])
		return input_demo_result_copy_error("missing result output path", error, error_size);
	if (!result)
		return input_demo_result_copy_error("missing input demo result", error, error_size);
	if (!input_demo_result_to_json_text(*result, &text, &text_error))
		return input_demo_result_copy_error(text_error, error, error_size);
	output.open(path, std::ios::binary | std::ios::trunc);
	if (!output.is_open())
		return input_demo_result_copy_error(std::string("could not open result file for writing: ") + path,
		                                    error, error_size);
	output << text;
	if (!output.good())
		return input_demo_result_copy_error(std::string("could not write result file: ") + path,
		                                    error, error_size);
	return 1;
}

int input_demo_result_compare(const input_demo_result *expected,
                              const input_demo_result *actual,
                              char *error, size_t error_size)
{
	return input_demo_result_compare_internal(expected, actual, true, error, error_size);
}

int input_demo_result_compare_snapshot(const input_demo_result *expected,
                                       const input_demo_result *actual,
                                       char *error,
                                       size_t error_size)
{
	return input_demo_result_compare_internal(expected, actual, false, error, error_size);
}

int input_demo_result_snapshot_to_json_buffer(const input_demo_result *result,
                                              char *text,
                                              size_t text_size)
{
	if (!result || !text || !text_size)
		return 0;
	const std::string snapshot_text = input_demo_result_to_json_object(result, false).dump();
	if (snapshot_text.size() + 1 > text_size)
		return 0;
	snprintf(text, text_size, "%s", snapshot_text.c_str());
	return 1;
}

int input_demo_result_compare_files(const char *expected_path,
                                    const char *actual_path,
                                    char *error, size_t error_size)
{
	input_demo_result expected;
	input_demo_result actual;

	if (!input_demo_result_read_json_file(expected_path, &expected, error, error_size))
		return 0;
	if (!input_demo_result_read_json_file(actual_path, &actual, error, error_size))
		return 0;
	return input_demo_result_compare(&expected, &actual, error, error_size);
}
}