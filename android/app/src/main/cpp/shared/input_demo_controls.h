#ifndef INPUT_DEMO_CONTROLS_H
#define INPUT_DEMO_CONTROLS_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER)
#pragma pack(push, 1)
#define INPUT_DEMO_PACKED
#elif defined(__GNUC__)
#define INPUT_DEMO_PACKED __attribute__((packed))
#else
#define INPUT_DEMO_PACKED
#endif

#define INPUT_DEMO_GAME_D1 1
#define INPUT_DEMO_GAME_D2 2

typedef struct input_demo_control_state {
	int32_t pitch_time;
	int32_t heading_time;
	int32_t bank_time;
	int32_t forward_thrust_time;
	int32_t sideways_thrust_time;
	int32_t vertical_thrust_time;
	uint8_t fire_primary_state;
	uint8_t fire_secondary_state;
	uint8_t rear_view_state;
	uint8_t automap_state;
	uint8_t afterburner_state;
	uint8_t energy_to_shield_state;
} INPUT_DEMO_PACKED input_demo_control_state;

typedef struct input_demo_control_pulse {
	uint8_t fire_primary_count;
	uint8_t fire_secondary_count;
	uint8_t fire_flare_count;
	uint8_t drop_bomb_count;
	uint8_t cycle_primary_count;
	uint8_t cycle_secondary_count;
	uint8_t select_weapon_count;
	uint8_t rear_view_count;
	uint8_t automap_count;
	uint8_t toggle_bomb_count;
	uint8_t headlight_count;
} INPUT_DEMO_PACKED input_demo_control_pulse;

typedef struct input_demo_control_state_update {
	uint8_t has_pitch_time;
	int32_t pitch_time;
	uint8_t has_heading_time;
	int32_t heading_time;
	uint8_t has_bank_time;
	int32_t bank_time;
	uint8_t has_forward_thrust_time;
	int32_t forward_thrust_time;
	uint8_t has_sideways_thrust_time;
	int32_t sideways_thrust_time;
	uint8_t has_vertical_thrust_time;
	int32_t vertical_thrust_time;
	uint8_t has_fire_primary_state;
	uint8_t fire_primary_state;
	uint8_t has_fire_secondary_state;
	uint8_t fire_secondary_state;
	uint8_t has_rear_view_state;
	uint8_t rear_view_state;
	uint8_t has_automap_state;
	uint8_t automap_state;
	uint8_t has_afterburner_state;
	uint8_t afterburner_state;
	uint8_t has_energy_to_shield_state;
	uint8_t energy_to_shield_state;
} INPUT_DEMO_PACKED input_demo_control_state_update;

typedef struct input_demo_control_pulse_update {
	uint8_t has_fire_primary_count;
	uint8_t fire_primary_count;
	uint8_t has_fire_secondary_count;
	uint8_t fire_secondary_count;
	uint8_t has_fire_flare_count;
	uint8_t fire_flare_count;
	uint8_t has_drop_bomb_count;
	uint8_t drop_bomb_count;
	uint8_t has_cycle_primary_count;
	uint8_t cycle_primary_count;
	uint8_t has_cycle_secondary_count;
	uint8_t cycle_secondary_count;
	uint8_t has_select_weapon_count;
	uint8_t select_weapon_count;
	uint8_t has_rear_view_count;
	uint8_t rear_view_count;
	uint8_t has_automap_count;
	uint8_t automap_count;
	uint8_t has_toggle_bomb_count;
	uint8_t toggle_bomb_count;
	uint8_t has_headlight_count;
	uint8_t headlight_count;
} INPUT_DEMO_PACKED input_demo_control_pulse_update;

typedef struct input_demo_control_frame {
	uint32_t frame;
	int32_t frame_time;
	input_demo_control_state state;
	input_demo_control_pulse pulse;
} INPUT_DEMO_PACKED input_demo_control_frame;

typedef struct input_demo_control_record {
	uint32_t frame;
	uint32_t run_length;
	uint8_t has_frame_time;
	int32_t frame_time;
	input_demo_control_state_update held;
	input_demo_control_pulse_update pulse;
} INPUT_DEMO_PACKED input_demo_control_record;

void input_demo_control_state_clear(input_demo_control_state *state);
void input_demo_control_pulse_clear(input_demo_control_pulse *pulse);
void input_demo_control_state_update_clear(input_demo_control_state_update *update);
void input_demo_control_pulse_update_clear(input_demo_control_pulse_update *update);
void input_demo_control_frame_clear(input_demo_control_frame *frame);
void input_demo_control_record_clear(input_demo_control_record *record);
void input_demo_control_state_update_from_state(input_demo_control_state_update *update,
                                                const input_demo_control_state *state, int game);
void input_demo_control_pulse_update_from_pulse(input_demo_control_pulse_update *update,
                                                const input_demo_control_pulse *pulse, int game);
void input_demo_control_state_apply_update(input_demo_control_state *state,
                                           const input_demo_control_state_update *update, int game);
void input_demo_control_pulse_apply_update(input_demo_control_pulse *pulse,
                                           const input_demo_control_pulse_update *update, int game);
int input_demo_control_state_update_is_empty(const input_demo_control_state_update *update);
int input_demo_control_pulse_update_is_empty(const input_demo_control_pulse_update *update);
int input_demo_control_record_has_d2_fields(const input_demo_control_record *record);

#ifdef __cplusplus
}

#include <string>
#include <vector>

bool input_demo_control_record_to_json_line(const input_demo_control_record &record, int game,
                                            std::string *line, std::string *error);
bool input_demo_control_record_parse_json_line(const std::string &line, int game,
                                               input_demo_control_record *record, std::string *error);
bool input_demo_control_records_coalesce_frames(const std::vector<input_demo_control_frame> &frames,
                                                int game, std::vector<input_demo_control_record> *records, std::string *error);
bool input_demo_control_records_write_jsonl_file(const char *path, int game,
                                                 const std::vector<input_demo_control_record> &records, std::string *error);
bool input_demo_control_records_read_jsonl_file(const char *path, int game,
                                                std::vector<input_demo_control_record> *records, std::string *error);

#endif

#if defined(_MSC_VER)
#pragma pack(pop)
#endif

#undef INPUT_DEMO_PACKED

#endif