#include "route_analysis_cache.h"

#include <stdint.h>
#include <stdio.h>
#include <string.h>

#define ROUTE_ANALYSIS_CACHE_MAGIC 0x52414348u

typedef struct route_analysis_cache_record {
	uint32_t magic;
	uint32_t checksum;
	route_analysis_cache_key key;
	level_metadata_state state;
	route_planner_plan_summary summary;
} route_analysis_cache_record;

static uint32_t route_analysis_cache_checksum(
    const route_analysis_cache_record *record)
{
	route_analysis_cache_record copy = *record;
	const unsigned char *bytes = (const unsigned char *) &copy;
	uint32_t hash = 2166136261u;
	size_t index;

	copy.checksum = 0;
	for (index = 0; index < sizeof(copy); ++index) {
		hash ^= bytes[index];
		hash *= 16777619u;
	}
	return hash;
}

static int route_analysis_cache_string_valid(const char *text, size_t capacity)
{
	return memchr(text, '\0', capacity) != NULL;
}

static int route_analysis_cache_state_valid(
    const level_metadata_state *state,
    const route_planner_plan_summary *summary)
{
	int step;

	if (state->route_status < LEVEL_METADATA_ROUTE_OK ||
	    state->route_status > LEVEL_METADATA_ROUTE_FAILED ||
	    (state->unnecessary_key_mask &
	     ~(LEVEL_METADATA_KEY_MASK_BLUE | LEVEL_METADATA_KEY_MASK_RED |
	       LEVEL_METADATA_KEY_MASK_GOLD)) != 0 ||
	    state->route_step_count < 0 ||
	    state->route_step_count > LEVEL_METADATA_MAX_ROUTE_STEPS ||
	    summary->endpoint_kind != ROUTE_PLANNER_ENDPOINT_END_OF_LEVEL ||
	    summary->route_step_count != state->route_step_count ||
	    summary->first_pending_step < -1 ||
	    summary->first_pending_step >= state->route_step_count ||
	    !route_analysis_cache_string_valid(
	        state->route_problem, sizeof(state->route_problem)) ||
	    !route_analysis_cache_string_valid(
	        state->route_note, sizeof(state->route_note)))
		return 0;
	for (step = 0; step < state->route_step_count; ++step) {
		const level_metadata_route_step *item = &state->route_steps[step];
		if (item->opened_link_count < 0 ||
		    item->opened_link_count > LEVEL_METADATA_MAX_ROUTE_LINKS ||
		    !route_analysis_cache_string_valid(item->label, sizeof(item->label)) ||
		    !route_analysis_cache_string_valid(
		        item->trigger_type_name, sizeof(item->trigger_type_name)))
			return 0;
	}
	return 1;
}

size_t route_analysis_cache_record_size(void)
{
	return sizeof(route_analysis_cache_record);
}

int route_analysis_cache_make_key(
    unsigned int generation,
    unsigned int game,
    unsigned long long analysis_profile_hash,
    const route_snapshot_summary *snapshot,
    route_analysis_cache_key *key)
{
	if (!generation || !analysis_profile_hash ||
	    (game != ROUTE_ANALYSIS_CACHE_GAME_D1 &&
	     game != ROUTE_ANALYSIS_CACHE_GAME_D2) ||
	    !snapshot || !snapshot->topology_hash || !key)
		return 0;
	memset(key, 0, sizeof(*key));
	key->generation = generation;
	key->game = game;
	key->analysis_profile_hash = analysis_profile_hash;
	key->topology_hash = snapshot->topology_hash;
	return 1;
}

int route_analysis_cache_filename(
    const route_analysis_cache_key *key,
    char *filename,
    size_t capacity)
{
	int written;

	if (!key || !filename || !capacity)
		return 0;
	written = snprintf(
	    filename, capacity,
	    "route-cache/g%u/%s-%016llx-%016llx.bin",
	    key->generation,
	    key->game == ROUTE_ANALYSIS_CACHE_GAME_D2 ? "d2" : "d1",
	    key->analysis_profile_hash, key->topology_hash);
	return written > 0 && (size_t) written < capacity;
}

int route_analysis_cache_encode(
    const route_analysis_cache_key *key,
    const level_metadata_state *state,
    const route_planner_plan_summary *summary,
    void *record,
    size_t capacity)
{
	route_analysis_cache_record encoded;

	if (!key || !state || !summary || !record || capacity < sizeof(encoded) ||
	    !route_analysis_cache_state_valid(state, summary))
		return 0;
	memset(&encoded, 0, sizeof(encoded));
	encoded.magic = ROUTE_ANALYSIS_CACHE_MAGIC;
	encoded.key = *key;
	encoded.state = *state;
	encoded.summary = *summary;
	encoded.checksum = route_analysis_cache_checksum(&encoded);
	memcpy(record, &encoded, sizeof(encoded));
	return 1;
}

int route_analysis_cache_decode(
    const route_analysis_cache_key *key,
    const void *record,
    size_t size,
    level_metadata_state *state,
    route_planner_plan_summary *summary)
{
	route_analysis_cache_record decoded;

	if (!key || !record || size != sizeof(decoded) || !state || !summary)
		return 0;
	memcpy(&decoded, record, sizeof(decoded));
	if (decoded.magic != ROUTE_ANALYSIS_CACHE_MAGIC ||
	    memcmp(&decoded.key, key, sizeof(*key)) != 0 ||
	    decoded.checksum != route_analysis_cache_checksum(&decoded) ||
	    !route_analysis_cache_state_valid(&decoded.state, &decoded.summary))
		return 0;
	*state = decoded.state;
	*summary = decoded.summary;
	return 1;
}
