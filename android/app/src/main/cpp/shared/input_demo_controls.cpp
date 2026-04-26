#include "input_demo_controls.h"

#include <limits.h>
#include <stdio.h>
#include <string.h>

#include <fstream>
#include <sstream>

#include <nlohmann/json.hpp>

using ordered_json = nlohmann::ordered_json;

extern "C" void input_demo_control_state_clear(input_demo_control_state *state)
{
	memset(state, 0, sizeof(*state));
}

extern "C" void input_demo_control_pulse_clear(input_demo_control_pulse *pulse)
{
	memset(pulse, 0, sizeof(*pulse));
}

extern "C" void input_demo_control_state_update_clear(input_demo_control_state_update *update)
{
	memset(update, 0, sizeof(*update));
}

extern "C" void input_demo_control_pulse_update_clear(input_demo_control_pulse_update *update)
{
	memset(update, 0, sizeof(*update));
}

extern "C" void input_demo_control_frame_clear(input_demo_control_frame *frame)
{
	memset(frame, 0, sizeof(*frame));
}

extern "C" void input_demo_control_record_clear(input_demo_control_record *record)
{
	memset(record, 0, sizeof(*record));
	record->run_length = 1;
}

extern "C" void input_demo_control_state_update_from_state(input_demo_control_state_update *update,
                                                           const input_demo_control_state *state, int game)
{
	input_demo_control_state_update_clear(update);
	if (state->pitch_time) {
		update->has_pitch_time = 1;
		update->pitch_time = state->pitch_time;
	}
	if (state->heading_time) {
		update->has_heading_time = 1;
		update->heading_time = state->heading_time;
	}
	if (state->bank_time) {
		update->has_bank_time = 1;
		update->bank_time = state->bank_time;
	}
	if (state->forward_thrust_time) {
		update->has_forward_thrust_time = 1;
		update->forward_thrust_time = state->forward_thrust_time;
	}
	if (state->sideways_thrust_time) {
		update->has_sideways_thrust_time = 1;
		update->sideways_thrust_time = state->sideways_thrust_time;
	}
	if (state->vertical_thrust_time) {
		update->has_vertical_thrust_time = 1;
		update->vertical_thrust_time = state->vertical_thrust_time;
	}
	if (state->fire_primary_state) {
		update->has_fire_primary_state = 1;
		update->fire_primary_state = state->fire_primary_state;
	}
	if (state->fire_secondary_state) {
		update->has_fire_secondary_state = 1;
		update->fire_secondary_state = state->fire_secondary_state;
	}
	if (state->rear_view_state) {
		update->has_rear_view_state = 1;
		update->rear_view_state = state->rear_view_state;
	}
	if (state->automap_state) {
		update->has_automap_state = 1;
		update->automap_state = state->automap_state;
	}
	if (game == INPUT_DEMO_GAME_D2 && state->afterburner_state) {
		update->has_afterburner_state = 1;
		update->afterburner_state = state->afterburner_state;
	}
	if (game == INPUT_DEMO_GAME_D2 && state->energy_to_shield_state) {
		update->has_energy_to_shield_state = 1;
		update->energy_to_shield_state = state->energy_to_shield_state;
	}
}

extern "C" void input_demo_control_state_update_from_transition(input_demo_control_state_update *update,
                                                                const input_demo_control_state *previous,
                                                                const input_demo_control_state *current,
                                                                int game)
{
	input_demo_control_state_update_clear(update);
	if (previous->pitch_time != current->pitch_time) {
		update->has_pitch_time = 1;
		update->pitch_time = current->pitch_time;
	}
	if (previous->heading_time != current->heading_time) {
		update->has_heading_time = 1;
		update->heading_time = current->heading_time;
	}
	if (previous->bank_time != current->bank_time) {
		update->has_bank_time = 1;
		update->bank_time = current->bank_time;
	}
	if (previous->forward_thrust_time != current->forward_thrust_time) {
		update->has_forward_thrust_time = 1;
		update->forward_thrust_time = current->forward_thrust_time;
	}
	if (previous->sideways_thrust_time != current->sideways_thrust_time) {
		update->has_sideways_thrust_time = 1;
		update->sideways_thrust_time = current->sideways_thrust_time;
	}
	if (previous->vertical_thrust_time != current->vertical_thrust_time) {
		update->has_vertical_thrust_time = 1;
		update->vertical_thrust_time = current->vertical_thrust_time;
	}
	if (previous->fire_primary_state != current->fire_primary_state) {
		update->has_fire_primary_state = 1;
		update->fire_primary_state = current->fire_primary_state;
	}
	if (previous->fire_secondary_state != current->fire_secondary_state) {
		update->has_fire_secondary_state = 1;
		update->fire_secondary_state = current->fire_secondary_state;
	}
	if (previous->rear_view_state != current->rear_view_state) {
		update->has_rear_view_state = 1;
		update->rear_view_state = current->rear_view_state;
	}
	if (previous->automap_state != current->automap_state) {
		update->has_automap_state = 1;
		update->automap_state = current->automap_state;
	}
	if (game == INPUT_DEMO_GAME_D2 && previous->afterburner_state != current->afterburner_state) {
		update->has_afterburner_state = 1;
		update->afterburner_state = current->afterburner_state;
	}
	if (game == INPUT_DEMO_GAME_D2 && previous->energy_to_shield_state != current->energy_to_shield_state) {
		update->has_energy_to_shield_state = 1;
		update->energy_to_shield_state = current->energy_to_shield_state;
	}
}

extern "C" void input_demo_control_pulse_update_from_pulse(input_demo_control_pulse_update *update,
                                                           const input_demo_control_pulse *pulse, int game)
{
	input_demo_control_pulse_update_clear(update);
	if (pulse->fire_primary_count) {
		update->has_fire_primary_count = 1;
		update->fire_primary_count = pulse->fire_primary_count;
	}
	if (pulse->fire_secondary_count) {
		update->has_fire_secondary_count = 1;
		update->fire_secondary_count = pulse->fire_secondary_count;
	}
	if (pulse->fire_flare_count) {
		update->has_fire_flare_count = 1;
		update->fire_flare_count = pulse->fire_flare_count;
	}
	if (pulse->drop_bomb_count) {
		update->has_drop_bomb_count = 1;
		update->drop_bomb_count = pulse->drop_bomb_count;
	}
	if (pulse->cycle_primary_count) {
		update->has_cycle_primary_count = 1;
		update->cycle_primary_count = pulse->cycle_primary_count;
	}
	if (pulse->cycle_secondary_count) {
		update->has_cycle_secondary_count = 1;
		update->cycle_secondary_count = pulse->cycle_secondary_count;
	}
	if (pulse->select_weapon_count) {
		update->has_select_weapon_count = 1;
		update->select_weapon_count = pulse->select_weapon_count;
	}
	if (pulse->rear_view_count) {
		update->has_rear_view_count = 1;
		update->rear_view_count = pulse->rear_view_count;
	}
	if (pulse->automap_count) {
		update->has_automap_count = 1;
		update->automap_count = pulse->automap_count;
	}
	if (game == INPUT_DEMO_GAME_D2 && pulse->toggle_bomb_count) {
		update->has_toggle_bomb_count = 1;
		update->toggle_bomb_count = pulse->toggle_bomb_count;
	}
	if (game == INPUT_DEMO_GAME_D2 && pulse->headlight_count) {
		update->has_headlight_count = 1;
		update->headlight_count = pulse->headlight_count;
	}
}

extern "C" void input_demo_control_state_apply_update(input_demo_control_state *state,
                                                      const input_demo_control_state_update *update, int game)
{
	if (update->has_pitch_time)
		state->pitch_time = update->pitch_time;
	if (update->has_heading_time)
		state->heading_time = update->heading_time;
	if (update->has_bank_time)
		state->bank_time = update->bank_time;
	if (update->has_forward_thrust_time)
		state->forward_thrust_time = update->forward_thrust_time;
	if (update->has_sideways_thrust_time)
		state->sideways_thrust_time = update->sideways_thrust_time;
	if (update->has_vertical_thrust_time)
		state->vertical_thrust_time = update->vertical_thrust_time;
	if (update->has_fire_primary_state)
		state->fire_primary_state = update->fire_primary_state;
	if (update->has_fire_secondary_state)
		state->fire_secondary_state = update->fire_secondary_state;
	if (update->has_rear_view_state)
		state->rear_view_state = update->rear_view_state;
	if (update->has_automap_state)
		state->automap_state = update->automap_state;
	if (game == INPUT_DEMO_GAME_D2 && update->has_afterburner_state)
		state->afterburner_state = update->afterburner_state;
	if (game == INPUT_DEMO_GAME_D2 && update->has_energy_to_shield_state)
		state->energy_to_shield_state = update->energy_to_shield_state;
}

extern "C" void input_demo_control_pulse_apply_update(input_demo_control_pulse *pulse,
                                                      const input_demo_control_pulse_update *update, int game)
{
	if (update->has_fire_primary_count)
		pulse->fire_primary_count = update->fire_primary_count;
	if (update->has_fire_secondary_count)
		pulse->fire_secondary_count = update->fire_secondary_count;
	if (update->has_fire_flare_count)
		pulse->fire_flare_count = update->fire_flare_count;
	if (update->has_drop_bomb_count)
		pulse->drop_bomb_count = update->drop_bomb_count;
	if (update->has_cycle_primary_count)
		pulse->cycle_primary_count = update->cycle_primary_count;
	if (update->has_cycle_secondary_count)
		pulse->cycle_secondary_count = update->cycle_secondary_count;
	if (update->has_select_weapon_count)
		pulse->select_weapon_count = update->select_weapon_count;
	if (update->has_rear_view_count)
		pulse->rear_view_count = update->rear_view_count;
	if (update->has_automap_count)
		pulse->automap_count = update->automap_count;
	if (game == INPUT_DEMO_GAME_D2 && update->has_toggle_bomb_count)
		pulse->toggle_bomb_count = update->toggle_bomb_count;
	if (game == INPUT_DEMO_GAME_D2 && update->has_headlight_count)
		pulse->headlight_count = update->headlight_count;
}

extern "C" int input_demo_control_state_update_is_empty(const input_demo_control_state_update *update)
{
	return !(update->has_pitch_time || update->has_heading_time || update->has_bank_time ||
	         update->has_forward_thrust_time || update->has_sideways_thrust_time ||
	         update->has_vertical_thrust_time || update->has_fire_primary_state ||
	         update->has_fire_secondary_state || update->has_rear_view_state ||
	         update->has_automap_state || update->has_afterburner_state ||
	         update->has_energy_to_shield_state);
}

extern "C" int input_demo_control_pulse_update_is_empty(const input_demo_control_pulse_update *update)
{
	return !(update->has_fire_primary_count || update->has_fire_secondary_count ||
	         update->has_fire_flare_count || update->has_drop_bomb_count ||
	         update->has_cycle_primary_count || update->has_cycle_secondary_count ||
	         update->has_select_weapon_count || update->has_rear_view_count ||
	         update->has_automap_count || update->has_toggle_bomb_count ||
	         update->has_headlight_count);
}

extern "C" int input_demo_control_record_has_d2_fields(const input_demo_control_record *record)
{
	return record->held.has_afterburner_state || record->held.has_energy_to_shield_state ||
	       record->pulse.has_toggle_bomb_count || record->pulse.has_headlight_count;
}

static bool fail(std::string *error, const std::string &message)
{
	if (error)
		*error = message;
	return false;
}

static bool parse_int32_field(const ordered_json &value, int32_t *out, std::string *error,
                              const char *field_name)
{
	long long parsed;

	if (!value.is_number_integer())
		return fail(error, std::string(field_name) + " must be an integer");
	parsed = value.get<long long>();
	if (parsed < INT32_MIN || parsed > INT32_MAX)
		return fail(error, std::string(field_name) + " is out of range");
	*out = (int32_t) parsed;
	return true;
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

static bool parse_u8_field(const ordered_json &value, uint8_t *out, std::string *error,
                           const char *field_name, unsigned int max_value)
{
	uint32_t parsed;

	if (!parse_uint32_field(value, &parsed, error, field_name))
		return false;
	if (parsed > max_value)
		return fail(error, std::string(field_name) + " is out of range");
	*out = (uint8_t) parsed;
	return true;
}

static bool parse_state_object(const ordered_json &value, int game,
                               input_demo_control_state_update *update, std::string *error)
{
	ordered_json::const_iterator it;

	if (!value.is_object())
		return fail(error, "s must be an object");
	input_demo_control_state_update_clear(update);
	for (it = value.begin(); it != value.end(); ++it) {
		const std::string &name = it.key();
		if (name == "p") {
			int32_t parsed;
			if (!parse_int32_field(it.value(), &parsed, error, "s.p"))
				return false;
			update->pitch_time = parsed;
			update->has_pitch_time = 1;
		} else if (name == "h") {
			int32_t parsed;
			if (!parse_int32_field(it.value(), &parsed, error, "s.h"))
				return false;
			update->heading_time = parsed;
			update->has_heading_time = 1;
		} else if (name == "b") {
			int32_t parsed;
			if (!parse_int32_field(it.value(), &parsed, error, "s.b"))
				return false;
			update->bank_time = parsed;
			update->has_bank_time = 1;
		} else if (name == "f") {
			int32_t parsed;
			if (!parse_int32_field(it.value(), &parsed, error, "s.f"))
				return false;
			update->forward_thrust_time = parsed;
			update->has_forward_thrust_time = 1;
		} else if (name == "x") {
			int32_t parsed;
			if (!parse_int32_field(it.value(), &parsed, error, "s.x"))
				return false;
			update->sideways_thrust_time = parsed;
			update->has_sideways_thrust_time = 1;
		} else if (name == "z") {
			int32_t parsed;
			if (!parse_int32_field(it.value(), &parsed, error, "s.z"))
				return false;
			update->vertical_thrust_time = parsed;
			update->has_vertical_thrust_time = 1;
		} else if (name == "f1s") {
			if (!parse_u8_field(it.value(), &update->fire_primary_state, error, "s.f1s", UCHAR_MAX))
				return false;
			update->has_fire_primary_state = 1;
		} else if (name == "f2s") {
			if (!parse_u8_field(it.value(), &update->fire_secondary_state, error, "s.f2s", UCHAR_MAX))
				return false;
			update->has_fire_secondary_state = 1;
		} else if (name == "rvs") {
			if (!parse_u8_field(it.value(), &update->rear_view_state, error, "s.rvs", UCHAR_MAX))
				return false;
			update->has_rear_view_state = 1;
		} else if (name == "ams") {
			if (!parse_u8_field(it.value(), &update->automap_state, error, "s.ams", UCHAR_MAX))
				return false;
			update->has_automap_state = 1;
		} else if (name == "ab") {
			if (game != INPUT_DEMO_GAME_D2)
				return fail(error, "D1 fixtures must reject D2-only held-state keys");
			if (!parse_u8_field(it.value(), &update->afterburner_state, error, "s.ab", UCHAR_MAX))
				return false;
			update->has_afterburner_state = 1;
		} else if (name == "es") {
			if (game != INPUT_DEMO_GAME_D2)
				return fail(error, "D1 fixtures must reject D2-only held-state keys");
			if (!parse_u8_field(it.value(), &update->energy_to_shield_state, error, "s.es", UCHAR_MAX))
				return false;
			update->has_energy_to_shield_state = 1;
		} else {
			return fail(error, "unknown held-state key: " + name);
		}
	}
	return true;
}

static bool parse_pulse_object(const ordered_json &value, int game,
                               input_demo_control_pulse_update *update, std::string *error)
{
	ordered_json::const_iterator it;

	if (!value.is_object())
		return fail(error, "p must be an object");
	input_demo_control_pulse_update_clear(update);
	for (it = value.begin(); it != value.end(); ++it) {
		const std::string &name = it.key();
		if (name == "f1") {
			if (!parse_u8_field(it.value(), &update->fire_primary_count, error, "p.f1", UCHAR_MAX))
				return false;
			update->has_fire_primary_count = 1;
		} else if (name == "f2") {
			if (!parse_u8_field(it.value(), &update->fire_secondary_count, error, "p.f2", UCHAR_MAX))
				return false;
			update->has_fire_secondary_count = 1;
		} else if (name == "fl") {
			if (!parse_u8_field(it.value(), &update->fire_flare_count, error, "p.fl", UCHAR_MAX))
				return false;
			update->has_fire_flare_count = 1;
		} else if (name == "db") {
			if (!parse_u8_field(it.value(), &update->drop_bomb_count, error, "p.db", UCHAR_MAX))
				return false;
			update->has_drop_bomb_count = 1;
		} else if (name == "cp") {
			if (!parse_u8_field(it.value(), &update->cycle_primary_count, error, "p.cp", UCHAR_MAX))
				return false;
			update->has_cycle_primary_count = 1;
		} else if (name == "cs") {
			if (!parse_u8_field(it.value(), &update->cycle_secondary_count, error, "p.cs", UCHAR_MAX))
				return false;
			update->has_cycle_secondary_count = 1;
		} else if (name == "sw") {
			if (!parse_u8_field(it.value(), &update->select_weapon_count, error, "p.sw", UCHAR_MAX))
				return false;
			update->has_select_weapon_count = 1;
		} else if (name == "rv") {
			if (!parse_u8_field(it.value(), &update->rear_view_count, error, "p.rv", UCHAR_MAX))
				return false;
			update->has_rear_view_count = 1;
		} else if (name == "am") {
			if (!parse_u8_field(it.value(), &update->automap_count, error, "p.am", UCHAR_MAX))
				return false;
			update->has_automap_count = 1;
		} else if (name == "tb") {
			if (game != INPUT_DEMO_GAME_D2)
				return fail(error, "D1 fixtures must reject D2-only pulse keys");
			if (!parse_u8_field(it.value(), &update->toggle_bomb_count, error, "p.tb", UCHAR_MAX))
				return false;
			update->has_toggle_bomb_count = 1;
		} else if (name == "hl") {
			if (game != INPUT_DEMO_GAME_D2)
				return fail(error, "D1 fixtures must reject D2-only pulse keys");
			if (!parse_u8_field(it.value(), &update->headlight_count, error, "p.hl", UCHAR_MAX))
				return false;
			update->has_headlight_count = 1;
		} else {
			return fail(error, "unknown pulse key: " + name);
		}
	}
	return true;
}

static bool validate_record(const input_demo_control_record &record, int game, std::string *error)
{
	if (!record.run_length)
		return fail(error, "record n must be positive");
	if (game == INPUT_DEMO_GAME_D1 && input_demo_control_record_has_d2_fields(&record))
		return fail(error, "D1 fixtures must reject D2-only control keys");
	return true;
}

static bool validate_stream(const std::vector<input_demo_control_record> &records, int game,
                            std::string *error)
{
	size_t i;
	uint32_t previous_frame = 0;
	int have_frame_time = 0;

	if (records.empty())
		return fail(error, "control stream is empty");
	for (i = 0; i != records.size(); ++i) {
		if (!validate_record(records[i], game, error))
			return false;
		if (i == 0) {
			if (records[i].frame != 0)
				return fail(error, "first control record must include f: 0");
		} else if (records[i].frame <= previous_frame) {
			return fail(error, "control record f values must be strictly increasing");
		}
		if (!records[i].has_frame_time && !have_frame_time)
			return fail(error, "first control record must include ft");
		if (records[i].has_frame_time)
			have_frame_time = 1;
		previous_frame = records[i].frame;
	}
	return true;
}

static bool validate_frame(const input_demo_control_frame &frame, int game, std::string *error)
{
	if (game == INPUT_DEMO_GAME_D1 &&
	    (frame.state.afterburner_state || frame.state.energy_to_shield_state ||
	     frame.pulse.toggle_bomb_count || frame.pulse.headlight_count))
		return fail(error, "D1 coalescer input must reject D2-only control fields");
	if (frame.frame_time < 0)
		return fail(error, "control frame_time must be non-negative");
	return true;
}

bool input_demo_control_record_to_json_line(const input_demo_control_record &record, int game,
                                            std::string *line, std::string *error)
{
	ordered_json root = ordered_json::object();
	ordered_json held = ordered_json::object();
	ordered_json pulse = ordered_json::object();

	if (!validate_record(record, game, error))
		return false;
	if (!line)
		return fail(error, "missing control json line output");
	root["f"] = record.frame;
	if (record.run_length != 1)
		root["n"] = record.run_length;
	if (record.has_frame_time)
		root["ft"] = record.frame_time;
	if (record.held.has_pitch_time)
		held["p"] = record.held.pitch_time;
	if (record.held.has_heading_time)
		held["h"] = record.held.heading_time;
	if (record.held.has_bank_time)
		held["b"] = record.held.bank_time;
	if (record.held.has_forward_thrust_time)
		held["f"] = record.held.forward_thrust_time;
	if (record.held.has_sideways_thrust_time)
		held["x"] = record.held.sideways_thrust_time;
	if (record.held.has_vertical_thrust_time)
		held["z"] = record.held.vertical_thrust_time;
	if (record.held.has_fire_primary_state)
		held["f1s"] = record.held.fire_primary_state;
	if (record.held.has_fire_secondary_state)
		held["f2s"] = record.held.fire_secondary_state;
	if (record.held.has_rear_view_state)
		held["rvs"] = record.held.rear_view_state;
	if (record.held.has_automap_state)
		held["ams"] = record.held.automap_state;
	if (record.held.has_afterburner_state)
		held["ab"] = record.held.afterburner_state;
	if (record.held.has_energy_to_shield_state)
		held["es"] = record.held.energy_to_shield_state;
	if (!held.empty())
		root["s"] = held;
	if (record.pulse.has_fire_primary_count)
		pulse["f1"] = record.pulse.fire_primary_count;
	if (record.pulse.has_fire_secondary_count)
		pulse["f2"] = record.pulse.fire_secondary_count;
	if (record.pulse.has_fire_flare_count)
		pulse["fl"] = record.pulse.fire_flare_count;
	if (record.pulse.has_drop_bomb_count)
		pulse["db"] = record.pulse.drop_bomb_count;
	if (record.pulse.has_cycle_primary_count)
		pulse["cp"] = record.pulse.cycle_primary_count;
	if (record.pulse.has_cycle_secondary_count)
		pulse["cs"] = record.pulse.cycle_secondary_count;
	if (record.pulse.has_select_weapon_count)
		pulse["sw"] = record.pulse.select_weapon_count;
	if (record.pulse.has_rear_view_count)
		pulse["rv"] = record.pulse.rear_view_count;
	if (record.pulse.has_automap_count)
		pulse["am"] = record.pulse.automap_count;
	if (record.pulse.has_toggle_bomb_count)
		pulse["tb"] = record.pulse.toggle_bomb_count;
	if (record.pulse.has_headlight_count)
		pulse["hl"] = record.pulse.headlight_count;
	if (!pulse.empty())
		root["p"] = pulse;
	*line = root.dump();
	return true;
}

bool input_demo_control_record_parse_json_line(const std::string &line, int game,
                                               input_demo_control_record *record, std::string *error)
{
	ordered_json root = ordered_json::parse(line, NULL, false);
	ordered_json::const_iterator it;
	int have_frame = 0;

	if (!record)
		return fail(error, "missing control record output");
	if (root.is_discarded())
		return fail(error, "control json line is not valid JSON");
	if (!root.is_object())
		return fail(error, "control json line must be an object");
	input_demo_control_record_clear(record);
	for (it = root.begin(); it != root.end(); ++it) {
		const std::string &name = it.key();
		if (name == "f") {
			uint32_t parsed;
			if (!parse_uint32_field(it.value(), &parsed, error, "f"))
				return false;
			record->frame = parsed;
			have_frame = 1;
		} else if (name == "n") {
			uint32_t parsed;
			if (!parse_uint32_field(it.value(), &parsed, error, "n"))
				return false;
			record->run_length = parsed;
			if (!record->run_length)
				return fail(error, "record n must be positive");
		} else if (name == "ft") {
			int32_t parsed;
			if (!parse_int32_field(it.value(), &parsed, error, "ft"))
				return false;
			record->frame_time = parsed;
			record->has_frame_time = 1;
		} else if (name == "s") {
			if (!parse_state_object(it.value(), game, &record->held, error))
				return false;
		} else if (name == "p") {
			if (!parse_pulse_object(it.value(), game, &record->pulse, error))
				return false;
		} else {
			return fail(error, "unknown control record key: " + name);
		}
	}
	if (!have_frame)
		return fail(error, "control json line is missing f");
	return validate_record(*record, game, error);
}

bool input_demo_control_records_coalesce_frames(const std::vector<input_demo_control_frame> &frames,
                                                int game, std::vector<input_demo_control_record> *records, std::string *error)
{
	std::vector<input_demo_control_record> out;
	input_demo_control_state previous_state;
	int have_previous_frame_time = 0;
	int32_t previous_frame_time = 0;
	uint32_t expected_frame = 0;
	size_t i;

	if (!records)
		return fail(error, "missing control record list output");
	if (frames.empty())
		return fail(error, "control frame list is empty");
	input_demo_control_state_clear(&previous_state);
	for (i = 0; i != frames.size(); ++i) {
		input_demo_control_record record;
		const input_demo_control_frame &frame = frames[i];

		if (frame.frame != expected_frame) {
			if (!i)
				return fail(error, "first control frame must include f: 0");
			return fail(error, "control frames must be contiguous");
		}
		if (!validate_frame(frame, game, error))
			return false;
		input_demo_control_record_clear(&record);
		record.frame = frame.frame;
		record.has_frame_time = !have_previous_frame_time || frame.frame_time != previous_frame_time;
		record.frame_time = frame.frame_time;
		input_demo_control_state_update_from_transition(&record.held, &previous_state, &frame.state, game);
		input_demo_control_pulse_update_from_pulse(&record.pulse, &frame.pulse, game);
		if (!out.empty() &&
		    !record.has_frame_time &&
		    input_demo_control_state_update_is_empty(&record.held) &&
		    input_demo_control_pulse_update_is_empty(&record.pulse) &&
		    input_demo_control_pulse_update_is_empty(&out.back().pulse)) {
			out.back().run_length++;
		} else {
			out.push_back(record);
		}
		previous_state = frame.state;
		previous_frame_time = frame.frame_time;
		have_previous_frame_time = 1;
		expected_frame = frame.frame + 1;
	}
	if (!validate_stream(out, game, error))
		return false;
	*records = out;
	return true;
}

bool input_demo_control_records_write_jsonl_file(const char *path, int game,
                                                 const std::vector<input_demo_control_record> &records, std::string *error)
{
	std::ofstream out(path, std::ios::out | std::ios::trunc | std::ios::binary);
	size_t i;

	if (!path)
		return fail(error, "missing control jsonl path");
	if (!validate_stream(records, game, error))
		return false;
	if (!out)
		return fail(error, "could not open control jsonl file for writing");
	for (i = 0; i != records.size(); ++i) {
		std::string line;
		if (!input_demo_control_record_to_json_line(records[i], game, &line, error))
			return false;
		out << line << '\n';
		if (!out)
			return fail(error, "could not write control jsonl file");
	}
	return true;
}

bool input_demo_control_records_read_jsonl_file(const char *path, int game,
                                                std::vector<input_demo_control_record> *records, std::string *error)
{
	std::ifstream in(path, std::ios::in | std::ios::binary);
	std::string line;
	std::vector<input_demo_control_record> parsed_records;
	unsigned int line_number = 0;

	if (!path)
		return fail(error, "missing control jsonl path");
	if (!records)
		return fail(error, "missing control record list output");
	if (!in)
		return fail(error, "could not open control jsonl file");
	while (std::getline(in, line)) {
		input_demo_control_record record;
		std::string line_error;

		++line_number;
		if (line.empty())
			continue;
		if (!input_demo_control_record_parse_json_line(line, game, &record, &line_error))
			return fail(error, "control jsonl line " + std::to_string(line_number) + ": " + line_error);
		parsed_records.push_back(record);
	}
	if (in.bad())
		return fail(error, "could not read control jsonl file");
	if (!validate_stream(parsed_records, game, error))
		return false;
	*records = parsed_records;
	return true;
}