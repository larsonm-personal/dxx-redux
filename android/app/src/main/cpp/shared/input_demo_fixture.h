#ifndef INPUT_DEMO_FIXTURE_H
#define INPUT_DEMO_FIXTURE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define INPUT_DEMO_FIXTURE_PACKED
#elif defined(__GNUC__)
#define INPUT_DEMO_FIXTURE_PACKED __attribute__((packed))
#else
#define INPUT_DEMO_FIXTURE_PACKED
#endif

typedef struct input_demo_rng_frame {
	uint32_t frame;
	uint32_t state;
	uint8_t has_call_count;
	uint32_t call_count;
} INPUT_DEMO_FIXTURE_PACKED input_demo_rng_frame;

typedef struct input_demo_rng_record {
	uint32_t frame;
	uint32_t run_length;
	uint32_t state;
	uint8_t has_call_count;
	uint32_t call_count;
} INPUT_DEMO_FIXTURE_PACKED input_demo_rng_record;

#define INPUT_DEMO_PLAYER_CFG_PRIMARY_ORDER_MAX   11
#define INPUT_DEMO_PLAYER_CFG_SECONDARY_ORDER_MAX 11
#define INPUT_DEMO_DIFFICULTY_LEVELS              5
#define INPUT_DEMO_CHECKPOINT_STOLEN_ITEM_COUNT   10
#define INPUT_DEMO_CHECKPOINT_ESCORT_INT_UNSET    INT32_MIN
#define INPUT_DEMO_CHECKPOINT_ESCORT_I64_UNSET    INT64_MIN

typedef struct input_demo_player_cfg {
	int32_t auto_leveling;
	int32_t persistent_debris;
	uint8_t has_headlight_active_default;
	int32_t headlight_active_default;
	uint8_t no_fire_autoselect;
	uint8_t cycle_autoselect_only;
	uint8_t select_after_fire;
	uint8_t classic_autoselect_weapon;
	uint8_t original_homing;
	uint8_t primary_order_count;
	uint8_t primary_order[INPUT_DEMO_PLAYER_CFG_PRIMARY_ORDER_MAX];
	uint8_t secondary_order_count;
	uint8_t secondary_order[INPUT_DEMO_PLAYER_CFG_SECONDARY_ORDER_MAX];
} input_demo_player_cfg;

typedef struct input_demo_checkpoint_escort_state {
	uint8_t valid;
	int32_t buddy_allowed_to_talk;
	int64_t buddy_last_seen_player;
	int64_t buddy_last_player_path_created;
	int32_t escort_kill_object;
	int64_t escort_last_path_created;
	int32_t escort_goal_object;
	int32_t escort_special_goal;
	int32_t escort_goal_index;
	int32_t buddy_messages_suppressed;
	int64_t buddy_sorry_time;
	int32_t looking_for_marker;
	int32_t last_buddy_key;
	int64_t last_buddy_message_time;
	int64_t last_come_back_message_time;
	int64_t buddy_last_missile_time;
	int32_t escort_owner_player;
} input_demo_checkpoint_escort_state;

typedef struct input_demo_checkpoint_thief_state {
	uint8_t valid;
	int32_t stolen_item_index;
	int64_t re_init_thief_time;
	int64_t last_thief_hit_time;
} input_demo_checkpoint_thief_state;

void input_demo_rng_frame_clear(input_demo_rng_frame *frame);
void input_demo_rng_record_clear(input_demo_rng_record *record);
void input_demo_player_cfg_clear(input_demo_player_cfg *player_cfg);
void input_demo_checkpoint_escort_state_clear(input_demo_checkpoint_escort_state *escort_state);
void input_demo_checkpoint_thief_state_clear(input_demo_checkpoint_thief_state *thief_state);

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#undef INPUT_DEMO_FIXTURE_PACKED

#ifdef __cplusplus
}

#include <string>
#include <vector>

#include "input_demo_controls.h"
#include "input_demo_result.h"

struct input_demo_metadata {
	int version;
	std::string game;
	std::string mission;
	int build_number;
	std::string git_version;
	std::string arch;
	int level;
	int difficulty;
	std::string start_mode;
	std::string rng_mode;
	uint32_t frame_count;
	std::string classic_preview;
	std::string start_save;
	bool has_player_cfg;
	input_demo_player_cfg player_cfg;

	input_demo_metadata()
	    : version(0), build_number(0), level(0), difficulty(0), frame_count(0), has_player_cfg(false)
	{
		input_demo_player_cfg_clear(&player_cfg);
	}
};

struct input_demo_checkpoint {
	std::string format;
	std::string encoding;
	std::string compression;
	uint32_t size;
	std::string sha256;
	std::string save_name;
	uint8_t has_start_gt;
	int64_t start_gt;
	uint8_t has_collision_delay_last_play_time;
	int64_t collision_delay_last_play_time;
	input_demo_checkpoint_escort_state escort_state;
	input_demo_checkpoint_thief_state thief_state;
	std::string data;

	input_demo_checkpoint()
	    : compression("none"), size(0), has_start_gt(0), start_gt(0),
	      has_collision_delay_last_play_time(0), collision_delay_last_play_time(0)
	{
		input_demo_checkpoint_escort_state_clear(&escort_state);
		input_demo_checkpoint_thief_state_clear(&thief_state);
	}
};

struct input_demo_file_frame {
	input_demo_control_record input;
	input_demo_rng_record rng;
	bool has_state;
	input_demo_result state;
	bool has_diag;
	std::string diag_json;
	std::vector<std::string> events;

	input_demo_file_frame()
	    : has_state(false), has_diag(false)
	{
		input_demo_control_record_clear(&input);
		input_demo_rng_record_clear(&rng);
		input_demo_result_clear(&state);
	}
};

struct input_demo_file {
	input_demo_metadata metadata;
	bool has_checkpoint;
	input_demo_checkpoint checkpoint;
	std::vector<input_demo_file_frame> frames;
	bool has_result;
	input_demo_result result;

	input_demo_file()
	    : has_checkpoint(false), has_result(false)
	{
		input_demo_result_clear(&result);
	}
};

bool input_demo_rng_record_to_json_line(const input_demo_rng_record &record,
                                        std::string *line, std::string *error);
bool input_demo_rng_record_parse_json_line(const std::string &line,
                                           input_demo_rng_record *record, std::string *error);
bool input_demo_rng_records_coalesce_frames(const std::vector<input_demo_rng_frame> &frames,
                                            std::vector<input_demo_rng_record> *records, std::string *error);
bool input_demo_rng_records_write_jsonl_file(const char *path,
                                             const std::vector<input_demo_rng_record> &records, std::string *error);
bool input_demo_rng_records_read_jsonl_file(const char *path,
                                            std::vector<input_demo_rng_record> *records, std::string *error);
bool input_demo_metadata_parse_header_line(const std::string &line,
                                           input_demo_metadata *metadata, std::string *error);
bool input_demo_metadata_to_header_line(const input_demo_metadata &metadata,
                                        std::string *line, std::string *error);
bool input_demo_file_parse_text(const std::string &text,
                                input_demo_file *demo, std::string *error);
bool input_demo_file_read(const char *path,
                          input_demo_file *demo, std::string *error);
bool input_demo_file_to_text(const input_demo_file &demo,
                             std::string *text, std::string *error);
bool input_demo_file_write(const char *path,
                           const input_demo_file &demo, std::string *error);

#endif

#endif
