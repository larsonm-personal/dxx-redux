#ifndef INPUT_DEMO_RESULT_H
#define INPUT_DEMO_RESULT_H

#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
#pragma pack(push, 4)
#endif

#define INPUT_DEMO_RESULT_MAX_GAME           8
#define INPUT_DEMO_RESULT_MAX_MISSION        64
#define INPUT_DEMO_RESULT_MAX_PRIMARY_AMMO   16
#define INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO 16

typedef struct input_demo_result_player {
	int32_t present;
	int32_t energy;
	int32_t shields;
	int32_t score;
	int16_t lives;
	int16_t laser_level;
	int16_t primary_weapon;
	int16_t secondary_weapon;
	uint32_t flags;
	uint16_t primary_ammo[INPUT_DEMO_RESULT_MAX_PRIMARY_AMMO];
	uint16_t secondary_ammo[INPUT_DEMO_RESULT_MAX_SECONDARY_AMMO];
	uint16_t hostages;
} input_demo_result_player;

typedef struct input_demo_result_position {
	int32_t present;
	int32_t segment;
	int32_t x;
	int32_t y;
	int32_t z;
	int32_t has_forward;
	int32_t fx;
	int32_t fy;
	int32_t fz;
} input_demo_result_position;

typedef struct input_demo_result_level {
	int32_t present;
	int32_t robots_alive;
	int32_t robots_killed;
	int32_t hostages_remaining;
	int32_t powerups_remaining;
	int32_t control_center_destroyed;
	int32_t endlevel_completed;
} input_demo_result_level;

typedef struct input_demo_result {
	int32_t version;
	char game[INPUT_DEMO_RESULT_MAX_GAME];
	char mission[INPUT_DEMO_RESULT_MAX_MISSION];
	int32_t level;
	int32_t difficulty;
	uint32_t frame_count;
	/* Keep the 64-bit field on an explicit boundary shared by C and C++ builds */
	int32_t has_game_time64;
	int32_t reserved0;
	int64_t game_time64;
	input_demo_result_player player0;
	input_demo_result_position position;
	input_demo_result_level level_summary;
} input_demo_result;

#if defined(_MSC_VER) || defined(__GNUC__) || defined(__clang__)
#pragma pack(pop)
#endif

void input_demo_result_player_clear(input_demo_result_player *player);
void input_demo_result_position_clear(input_demo_result_position *position);
void input_demo_result_level_clear(input_demo_result_level *level);
void input_demo_result_clear(input_demo_result *result);
int input_demo_result_read_json_file(const char *path,
                                     input_demo_result *result,
                                     char *error, size_t error_size);
int input_demo_result_write_json_file(const char *path,
                                      const input_demo_result *result,
                                      char *error, size_t error_size);
int input_demo_result_compare(const input_demo_result *expected,
                              const input_demo_result *actual,
                              char *error, size_t error_size);
int input_demo_result_compare_files(const char *expected_path,
                                    const char *actual_path,
                                    char *error, size_t error_size);

#ifdef __cplusplus
}

#include <string>

bool input_demo_result_parse_json_text(const std::string &text,
                                       input_demo_result *result,
                                       std::string *error);
bool input_demo_result_to_json_text(const input_demo_result &result,
                                    std::string *text,
                                    std::string *error);
#endif

#endif