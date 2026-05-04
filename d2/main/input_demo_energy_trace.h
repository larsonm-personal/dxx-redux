#ifndef DXX_D2_MAIN_INPUT_DEMO_ENERGY_TRACE_H
#define DXX_D2_MAIN_INPUT_DEMO_ENERGY_TRACE_H

#include <stdio.h>

#include "input_demo_debug_logging.h"
#include "input_demo_recorder.h"
#include "input_demo_replay.h"

static void input_demo_trace_record_frame_event_json(const char *json_text)
{
	static int input_demo_record_event_append_logged_error = 0;
	char error[256] = "";

	if (!input_demo_recorder_is_active())
		return;
	if (!input_demo_recorder_append_frame_event_json(json_text, error, sizeof(error)) &&
		!input_demo_record_event_append_logged_error) {
		input_demo_record_event_append_logged_error = 1;
		con_printf(CON_NORMAL, "Input demo recorder event append failed: %s\n", error);
	}
}

static void input_demo_trace_energy_change(const char *cause, fix energy_before, fix energy_after, const char *extra_json, const char *extra_log)
{
	char json[512];
	const int before_value = f2i(energy_before);
	const int after_value = f2i(energy_after);
	const int delta_value = after_value - before_value;
	const int delta_raw = energy_after - energy_before;
	const int wrote = snprintf(
		json,
		sizeof(json),
		"{\"kind\":\"energy_change\",\"cause\":\"%s\",\"before\":%d,\"after\":%d,\"delta\":%d,\"before_raw\":%d,\"after_raw\":%d,\"delta_raw\":%d%s}",
		cause,
		before_value,
		after_value,
		delta_value,
		energy_before,
		energy_after,
		delta_raw,
		extra_json ? extra_json : "");

	if (energy_before == energy_after)
		return;

	if (wrote > 0 && wrote < (int)sizeof(json))
		input_demo_trace_record_frame_event_json(json);

	if (input_demo_debug_is_enabled() && input_demo_replay_is_loaded())
		con_printf(CON_NORMAL,
			"Input demo replay energy change: frame=%u cause=%s before=%d after=%d delta=%d before_raw=%d after_raw=%d delta_raw=%d%s\n",
			(unsigned int)input_demo_replay_next_frame_index(),
			cause,
			before_value,
			after_value,
			delta_value,
			energy_before,
			energy_after,
			delta_raw,
			extra_log ? extra_log : "");
}

#endif