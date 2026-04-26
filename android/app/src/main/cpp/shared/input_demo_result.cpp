#include "input_demo_result.h"

#include <stdio.h>
#include <string.h>

#include <fstream>
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
	if (result->game_time64)
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
}