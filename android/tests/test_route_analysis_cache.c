#include "route_analysis_cache.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int expect(int condition, const char *message)
{
	if (condition)
		return 0;
	fprintf(stderr, "FAIL: %s\n", message);
	return 1;
}

int main(void)
{
	route_snapshot_summary snapshot = { 0 };
	route_analysis_cache_key key;
	route_analysis_cache_key wrong_key;
	level_metadata_state input = { 0 };
	level_metadata_state output = { 0 };
	route_planner_plan_summary input_summary = { 0 };
	route_planner_plan_summary output_summary = { 0 };
	unsigned char *record;
	size_t size = route_analysis_cache_record_size();
	char filename[192];
	int failures = 0;

	snapshot.topology_hash = 0x1234;
	snapshot.progression_hash = 0x2345;
	snapshot.trigger_hash = 0x3456;
	snapshot.object_hash = 0x4567;
	failures += expect(
	    route_analysis_cache_make_key(
	        7010, ROUTE_ANALYSIS_CACHE_GAME_D2, &snapshot, &key),
	    "make key");
	wrong_key = key;
	wrong_key.build_number++;
	failures += expect(
	    route_analysis_cache_filename(&key, filename, sizeof(filename)) &&
	        strstr(filename, "route-cache/7010/d2-") == filename,
	    "versioned filename");

	input.route_status = LEVEL_METADATA_ROUTE_OK;
	input.route_step_count = 2;
	input.route_steps[0].kind = LEVEL_METADATA_ROUTE_START;
	strcpy(input.route_steps[0].label, "Start");
	input.route_steps[1].kind = LEVEL_METADATA_ROUTE_TRIGGER;
	input.route_steps[1].activation_kind =
	    LEVEL_METADATA_ROUTE_ACTIVATION_SHOOT_SWITCH;
	input.route_steps[1].activation_pos_valid = 1;
	input.route_steps[1].activation_pos[0] = 10;
	input.route_steps[1].aim_pos_valid = 1;
	input.route_steps[1].aim_pos[2] = 30;
	strcpy(input.route_steps[1].label, "Shoot switch 6");
	strcpy(input.route_steps[1].trigger_type_name, "open_door");
	input_summary.endpoint_kind = ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL;
	input_summary.route_step_count = 2;
	input_summary.first_pending_step = 1;
	input_summary.first_pending_path_terminal_segment = 4;
	record = (unsigned char *) malloc(size);
	failures += expect(record != NULL, "allocate record");
	if (!record)
		return 1;
	failures += expect(
	    route_analysis_cache_encode(
	        &key, &input, &input_summary, record, size),
	    "encode");
	failures += expect(
	    route_analysis_cache_decode(
	        &key, record, size, &output, &output_summary),
	    "decode");
	failures += expect(
	    output.route_steps[1].activation_pos[0] == 10 &&
	        output.route_steps[1].aim_pos[2] == 30 &&
	        output_summary.first_pending_path_terminal_segment == 4,
	    "preserve firing coordinates and summary");
	failures += expect(
	    !route_analysis_cache_decode(
	        &wrong_key, record, size, &output, &output_summary),
	    "reject wrong build");
	failures += expect(
	    !route_analysis_cache_decode(
	        &key, record, size - 1, &output, &output_summary),
	    "reject truncation");
	record[size / 2] ^= 0x80;
	failures += expect(
	    !route_analysis_cache_decode(
	        &key, record, size, &output, &output_summary),
	    "reject corruption");
	free(record);
	if (failures)
		return 1;
	puts("PASS: route analysis cache");
	return 0;
}
