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

static bool input_demo_result_parse_sparse_ammo_object(const nlohmann::json &object,
                                                       uint16_t *ammo, size_t ammo_count,
                                                       std::string *error, const char *label)
{
	if (!object.is_object())
		return error ? (*error = std::string(label) + " must be an object", false) : false;
	for (nlohmann::json::const_iterator it = object.begin(); it != object.end(); ++it) {
		char *end = NULL;
		long index = strtol(it.key().c_str(), &end, 10);

		if (!end || *end != '\0' || index < 0 || (size_t) index >= ammo_count)
			return error ? (*error = std::string(label) + " contains an invalid ammo index: " + it.key(), false) : false;
		if (!it.value().is_number_integer() && !it.value().is_number_unsigned())
			return error ? (*error = std::string(label) + " contains a non-integer ammo value", false) : false;
		ammo[index] = (uint16_t) it.value().get<unsigned int>();
	}
	return true;
}

static bool input_demo_result_parse_player(const nlohmann::json &player_json,
                                           input_demo_result_player *player,
                                           std::string *error)
{
	static const char *const allowed_keys[] = { "e", "s", "sc", "li", "ll", "pw", "sw", "fl", "pa", "sa", "hk" };

	if (!player_json.is_object())
		return error ? (*error = "p0 must be an object", false) : false;
	input_demo_result_player_clear(player);
	player->present = 1;
	for (nlohmann::json::const_iterator it = player_json.begin(); it != player_json.end(); ++it) {
		if (!input_demo_result_key_allowed(it.key(), allowed_keys, sizeof(allowed_keys) / sizeof(allowed_keys[0])))
			return error ? (*error = std::string("unknown p0 key: ") + it.key(), false) : false;
	}
	if (player_json.contains("e"))
		player->energy = player_json.at("e").get<int32_t>();
	if (player_json.contains("s"))
		player->shields = player_json.at("s").get<int32_t>();
	if (player_json.contains("sc"))
		player->score = player_json.at("sc").get<int32_t>();
	if (player_json.contains("li"))
		player->lives = player_json.at("li").get<int16_t>();
	if (player_json.contains("ll"))
		player->laser_level = player_json.at("ll").get<int16_t>();
	if (player_json.contains("pw"))
		player->primary_weapon = player_json.at("pw").get<int16_t>();
	if (player_json.contains("sw"))
		player->secondary_weapon = player_json.at("sw").get<int16_t>();
	if (player_json.contains("fl"))
		player->flags = player_json.at("fl").get<uint32_t>();
	if (player_json.contains("hk"))
		player->hostages = player_json.at("hk").get<uint16_t>();
	if (player_json.contains("pa") && !input_demo_result_parse_sparse_ammo_object(
	                                      player_json.at("pa"), player->primary_ammo, INPUT_DEMO_RESULT_MAX_PRIMARY_AMMO, error, "p0.pa"))
		return false;
	if (player_json.contains("sa") && !input_demo_result_parse_sparse_ammo_object(
	                                      player_json.at("sa"), player->secondary_ammo, INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO, error, "p0.sa"))
		return false;
	return true;
}

static bool input_demo_result_parse_position(const nlohmann::json &position_json,
                                             input_demo_result_position *position,
                                             std::string *error)
{
	static const char *const allowed_keys[] = { "sg", "x", "y", "z", "fx", "fy", "fz" };

	if (!position_json.is_object())
		return error ? (*error = "pos must be an object", false) : false;
	input_demo_result_position_clear(position);
	position->present = 1;
	for (nlohmann::json::const_iterator it = position_json.begin(); it != position_json.end(); ++it) {
		if (!input_demo_result_key_allowed(it.key(), allowed_keys, sizeof(allowed_keys) / sizeof(allowed_keys[0])))
			return error ? (*error = std::string("unknown pos key: ") + it.key(), false) : false;
	}
	if (!position_json.contains("sg") || !position_json.contains("x") || !position_json.contains("y") ||
	    !position_json.contains("z"))
		return error ? (*error = "pos must include sg, x, y, and z", false) : false;
	position->segment = position_json.at("sg").get<int32_t>();
	position->x = position_json.at("x").get<int32_t>();
	position->y = position_json.at("y").get<int32_t>();
	position->z = position_json.at("z").get<int32_t>();
	if (position_json.contains("fx") || position_json.contains("fy") || position_json.contains("fz")) {
		position->has_forward = 1;
		if (position_json.contains("fx"))
			position->fx = position_json.at("fx").get<int32_t>();
		if (position_json.contains("fy"))
			position->fy = position_json.at("fy").get<int32_t>();
		if (position_json.contains("fz"))
			position->fz = position_json.at("fz").get<int32_t>();
	}
	return true;
}

static bool input_demo_result_parse_level_summary(const nlohmann::json &level_json,
                                                  input_demo_result_level *level,
                                                  std::string *error)
{
	static const char *const allowed_keys[] = { "ra", "rk", "hr", "pr", "cc", "el" };

	if (!level_json.is_object())
		return error ? (*error = "lv must be an object", false) : false;
	input_demo_result_level_clear(level);
	level->present = 1;
	for (nlohmann::json::const_iterator it = level_json.begin(); it != level_json.end(); ++it) {
		if (!input_demo_result_key_allowed(it.key(), allowed_keys, sizeof(allowed_keys) / sizeof(allowed_keys[0])))
			return error ? (*error = std::string("unknown lv key: ") + it.key(), false) : false;
	}
	if (level_json.contains("ra"))
		level->robots_alive = level_json.at("ra").get<int32_t>();
	if (level_json.contains("rk"))
		level->robots_killed = level_json.at("rk").get<int32_t>();
	if (level_json.contains("hr"))
		level->hostages_remaining = level_json.at("hr").get<int32_t>();
	if (level_json.contains("pr"))
		level->powerups_remaining = level_json.at("pr").get<int32_t>();
	if (level_json.contains("cc"))
		level->control_center_destroyed = level_json.at("cc").get<bool>() ? 1 : 0;
	if (level_json.contains("el"))
		level->endlevel_completed = level_json.at("el").get<bool>() ? 1 : 0;
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
		return error ? (*error = "p0 (player summary) missing from actual result", false) : false;
	if (!input_demo_result_compare_int64("p0.e", "player energy", expected->energy, actual->energy, error) ||
	    !input_demo_result_compare_int64("p0.s", "player shields", expected->shields, actual->shields, error) ||
	    !input_demo_result_compare_int64("p0.sc", "player score", expected->score, actual->score, error) ||
	    !input_demo_result_compare_int64("p0.li", "player lives", expected->lives, actual->lives, error) ||
	    !input_demo_result_compare_int64("p0.ll", "player laser level", expected->laser_level, actual->laser_level, error) ||
	    !input_demo_result_compare_int64("p0.pw", "player primary weapon", expected->primary_weapon, actual->primary_weapon, error) ||
	    !input_demo_result_compare_int64("p0.sw", "player secondary weapon", expected->secondary_weapon, actual->secondary_weapon, error) ||
	    !input_demo_result_compare_int64("p0.fl", "player flags", expected->flags, actual->flags, error) ||
	    !input_demo_result_compare_int64("p0.hk", "player hostages", expected->hostages, actual->hostages, error))
		return false;
	for (i = 0; i != INPUT_DEMO_RESULT_MAX_PRIMARY_AMMO; ++i) {
		if (!input_demo_result_compare_int64("p0.pa", "player primary ammo",
		                                     expected->primary_ammo[i], actual->primary_ammo[i], error))
			return false;
	}
	for (i = 0; i != INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO; ++i) {
		if (!input_demo_result_compare_int64("p0.sa", "player secondary ammo",
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
		return error ? (*error = "pos (player position) missing from actual result", false) : false;
	if (!input_demo_result_compare_int64("pos.sg", "player segment", expected->segment, actual->segment, error) ||
	    !input_demo_result_compare_int64("pos.x", "player x", expected->x, actual->x, error) ||
	    !input_demo_result_compare_int64("pos.y", "player y", expected->y, actual->y, error) ||
	    !input_demo_result_compare_int64("pos.z", "player z", expected->z, actual->z, error) ||
	    !input_demo_result_compare_int64("pos.fx", "player forward x", expected->fx, actual->fx, error) ||
	    !input_demo_result_compare_int64("pos.fy", "player forward y", expected->fy, actual->fy, error) ||
	    !input_demo_result_compare_int64("pos.fz", "player forward z", expected->fz, actual->fz, error))
		return false;
	return true;
}

static bool input_demo_result_compare_level_summary(const input_demo_result_level *expected,
                                                    const input_demo_result_level *actual,
                                                    std::string *error)
{
	if (!actual->present)
		return error ? (*error = "lv (level summary) missing from actual result", false) : false;
	if (!input_demo_result_compare_int64("lv.ra", "robots alive", expected->robots_alive, actual->robots_alive, error) ||
	    !input_demo_result_compare_int64("lv.rk", "robots killed", expected->robots_killed, actual->robots_killed, error) ||
	    !input_demo_result_compare_int64("lv.hr", "hostages remaining", expected->hostages_remaining, actual->hostages_remaining, error) ||
	    !input_demo_result_compare_int64("lv.pr", "powerups remaining", expected->powerups_remaining, actual->powerups_remaining, error) ||
	    !input_demo_result_compare_int64("lv.cc", "control center destroyed", expected->control_center_destroyed,
	                                     actual->control_center_destroyed, error) ||
	    !input_demo_result_compare_int64("lv.el", "endlevel completed", expected->endlevel_completed,
	                                     actual->endlevel_completed, error))
		return false;
	return true;
}

static nlohmann::ordered_json input_demo_result_sparse_ammo_object(const uint16_t *ammo, size_t count)
{
	nlohmann::ordered_json object = nlohmann::ordered_json::object();
	size_t i;

	for (i = 0; i != count; ++i) {
		if (!ammo[i])
			continue;
		object[std::to_string(i)] = ammo[i];
	}
	return object;
}

} // namespace

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
	result->version = 1;
	input_demo_result_player_clear(&result->player0);
	input_demo_result_position_clear(&result->position);
	input_demo_result_level_clear(&result->level_summary);
}

int input_demo_result_read_json_file(const char *path,
                                     input_demo_result *result,
                                     char *error, size_t error_size)
{
	static const char *const allowed_keys[] = { "v", "g", "m", "l", "d", "fr", "gt", "p0", "pos", "lv" };
	std::string text;
	std::string parse_error;
	nlohmann::json root;

	if (!path || !path[0])
		return input_demo_result_copy_error("missing result input path", error, error_size);
	if (!result)
		return input_demo_result_copy_error("missing input demo result output", error, error_size);
	if (!input_demo_result_read_text_file(path, &text, &parse_error))
		return input_demo_result_copy_error(parse_error, error, error_size);
	try {
		root = nlohmann::json::parse(text);
	} catch (const std::exception &ex) {
		return input_demo_result_copy_error(std::string("could not parse result json: ") + ex.what(), error, error_size);
	}
	if (!root.is_object())
		return input_demo_result_copy_error("result root must be an object", error, error_size);
	for (nlohmann::json::const_iterator it = root.begin(); it != root.end(); ++it) {
		if (!input_demo_result_key_allowed(it.key(), allowed_keys, sizeof(allowed_keys) / sizeof(allowed_keys[0])))
			return input_demo_result_copy_error(std::string("unknown result key: ") + it.key(), error, error_size);
	}
	if (!root.contains("v") || !root.contains("g") || !root.contains("m") || !root.contains("l") ||
	    !root.contains("d") || !root.contains("fr"))
		return input_demo_result_copy_error("result is missing one or more required keys: v, g, m, l, d, fr",
		                                    error, error_size);
	input_demo_result_clear(result);
	result->version = root.at("v").get<int32_t>();
	snprintf(result->game, sizeof(result->game), "%s", root.at("g").get_ref<const std::string &>().c_str());
	snprintf(result->mission, sizeof(result->mission), "%s", root.at("m").get_ref<const std::string &>().c_str());
	result->level = root.at("l").get<int32_t>();
	result->difficulty = root.at("d").get<int32_t>();
	result->frame_count = root.at("fr").get<uint32_t>();
	if (root.contains("gt")) {
		result->has_game_time64 = 1;
		result->game_time64 = root.at("gt").get<int64_t>();
	}
	if (root.contains("p0") && !input_demo_result_parse_player(root.at("p0"), &result->player0, &parse_error))
		return input_demo_result_copy_error(parse_error, error, error_size);
	if (root.contains("pos") && !input_demo_result_parse_position(root.at("pos"), &result->position, &parse_error))
		return input_demo_result_copy_error(parse_error, error, error_size);
	if (root.contains("lv") && !input_demo_result_parse_level_summary(root.at("lv"), &result->level_summary, &parse_error))
		return input_demo_result_copy_error(parse_error, error, error_size);
	return 1;
}

int input_demo_result_write_json_file(const char *path,
                                      const input_demo_result *result,
                                      char *error, size_t error_size)
{
	nlohmann::ordered_json root = nlohmann::ordered_json::object();
	std::ofstream output;

	if (!path || !path[0])
		return input_demo_result_copy_error("missing result output path", error, error_size);
	if (!result)
		return input_demo_result_copy_error("missing input demo result", error, error_size);
	output.open(path, std::ios::binary | std::ios::trunc);
	if (!output.is_open())
		return input_demo_result_copy_error(std::string("could not open result file for writing: ") + path,
		                                    error, error_size);

	root["v"] = result->version ? result->version : 1;
	if (result->game[0])
		root["g"] = result->game;
	if (result->mission[0])
		root["m"] = result->mission;
	root["l"] = result->level;
	root["d"] = result->difficulty;
	root["fr"] = result->frame_count;
	if (result->has_game_time64)
		root["gt"] = result->game_time64;

	if (result->player0.present) {
		nlohmann::ordered_json player = nlohmann::ordered_json::object();
		nlohmann::ordered_json primary_ammo = input_demo_result_sparse_ammo_object(
		    result->player0.primary_ammo, INPUT_DEMO_RESULT_MAX_PRIMARY_AMMO);
		nlohmann::ordered_json secondary_ammo = input_demo_result_sparse_ammo_object(
		    result->player0.secondary_ammo, INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO);

		if (result->player0.energy)
			player["e"] = result->player0.energy;
		if (result->player0.shields)
			player["s"] = result->player0.shields;
		if (result->player0.score)
			player["sc"] = result->player0.score;
		if (result->player0.lives)
			player["li"] = result->player0.lives;
		if (result->player0.laser_level)
			player["ll"] = result->player0.laser_level;
		if (result->player0.primary_weapon)
			player["pw"] = result->player0.primary_weapon;
		if (result->player0.secondary_weapon)
			player["sw"] = result->player0.secondary_weapon;
		if (result->player0.flags)
			player["fl"] = result->player0.flags;
		if (result->player0.hostages)
			player["hk"] = result->player0.hostages;
		if (!primary_ammo.empty())
			player["pa"] = std::move(primary_ammo);
		if (!secondary_ammo.empty())
			player["sa"] = std::move(secondary_ammo);
		if (!player.empty())
			root["p0"] = std::move(player);
	}

	if (result->position.present) {
		nlohmann::ordered_json position = nlohmann::ordered_json::object();

		position["sg"] = result->position.segment;
		position["x"] = result->position.x;
		position["y"] = result->position.y;
		position["z"] = result->position.z;
		if (result->position.has_forward) {
			position["fx"] = result->position.fx;
			position["fy"] = result->position.fy;
			position["fz"] = result->position.fz;
		}
		root["pos"] = std::move(position);
	}

	if (result->level_summary.present) {
		nlohmann::ordered_json level = nlohmann::ordered_json::object();

		if (result->level_summary.robots_alive)
			level["ra"] = result->level_summary.robots_alive;
		if (result->level_summary.robots_killed)
			level["rk"] = result->level_summary.robots_killed;
		if (result->level_summary.hostages_remaining)
			level["hr"] = result->level_summary.hostages_remaining;
		if (result->level_summary.powerups_remaining)
			level["pr"] = result->level_summary.powerups_remaining;
		if (result->level_summary.control_center_destroyed)
			level["cc"] = true;
		if (result->level_summary.endlevel_completed)
			level["el"] = true;
		if (!level.empty())
			root["lv"] = std::move(level);
	}

	output << root.dump(2) << '\n';
	if (!output.good())
		return input_demo_result_copy_error(std::string("could not write result file: ") + path,
		                                    error, error_size);
	return 1;
}

int input_demo_result_compare(const input_demo_result *expected,
                              const input_demo_result *actual,
                              char *error, size_t error_size)
{
	std::string compare_error;

	if (!expected || !actual)
		return input_demo_result_copy_error("missing result comparison input", error, error_size);
	if (!input_demo_result_compare_int64("v", "schema version", expected->version, actual->version, &compare_error) ||
	    !input_demo_result_compare_string("g", "game id", expected->game, actual->game, &compare_error) ||
	    !input_demo_result_compare_string("m", "mission id", expected->mission, actual->mission, &compare_error) ||
	    !input_demo_result_compare_int64("l", "level", expected->level, actual->level, &compare_error) ||
	    !input_demo_result_compare_int64("d", "difficulty", expected->difficulty, actual->difficulty, &compare_error) ||
	    !input_demo_result_compare_int64("fr", "frame count", expected->frame_count, actual->frame_count, &compare_error))
		return input_demo_result_copy_error(compare_error, error, error_size);
	if (expected->has_game_time64 &&
	    !input_demo_result_compare_int64("gt", "final GameTime64", expected->game_time64, actual->game_time64, &compare_error))
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