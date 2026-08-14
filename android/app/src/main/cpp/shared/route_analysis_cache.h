#ifndef DXX_ROUTE_ANALYSIS_CACHE_H
#define DXX_ROUTE_ANALYSIS_CACHE_H

#include <stddef.h>

#include "route_planner_c.h"
#include "route_snapshot_c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ROUTE_ANALYSIS_CACHE_GAME_D1    1
#define ROUTE_ANALYSIS_CACHE_GAME_D2    2
#define ROUTE_ANALYSIS_CACHE_GENERATION 1

typedef struct route_analysis_cache_key {
	unsigned int generation;
	unsigned int game;
	unsigned long long analysis_profile_hash;
	unsigned long long topology_hash;
	unsigned long long progression_hash;
	unsigned long long trigger_hash;
	unsigned long long object_hash;
} route_analysis_cache_key;

typedef struct route_analysis_cache_summary {
	unsigned int generation;
	unsigned int hits;
	unsigned int misses;
	unsigned int writes;
	unsigned int rejections;
	unsigned int io_errors;
	unsigned int live_reuses;
	unsigned int live_fallbacks;
	unsigned long long topology_hash;
	char filename[192];
} route_analysis_cache_summary;

size_t route_analysis_cache_record_size(void);
int route_analysis_cache_make_key(
    unsigned int generation,
    unsigned int game,
    unsigned long long analysis_profile_hash,
    const route_snapshot_summary *snapshot,
    route_analysis_cache_key *key);
int route_analysis_cache_filename(
    const route_analysis_cache_key *key,
    char *filename,
    size_t capacity);
int route_analysis_cache_encode(
    const route_analysis_cache_key *key,
    const level_metadata_state *state,
    const route_planner_plan_summary *summary,
    void *record,
    size_t capacity);
int route_analysis_cache_decode(
    const route_analysis_cache_key *key,
    const void *record,
    size_t size,
    level_metadata_state *state,
    route_planner_plan_summary *summary);

#ifdef __cplusplus
}
#endif

#endif
