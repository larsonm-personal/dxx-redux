#ifndef DXX_ROUTE_ANALYSIS_CACHE_H
#define DXX_ROUTE_ANALYSIS_CACHE_H

#include <stddef.h>

#include "route_planner_c.h"
#include "route_snapshot_c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define ROUTE_ANALYSIS_CACHE_GAME_D1 1
#define ROUTE_ANALYSIS_CACHE_GAME_D2 2
/* Keep in sync with ROUTE_METADATA_CACHE_GENERATION in RouteMetadataScheduling.kt. */
#define ROUTE_ANALYSIS_CACHE_GENERATION       10
#define ROUTE_ANALYSIS_TIMING_SAMPLE_CAPACITY 32

typedef struct route_analysis_cache_key {
	unsigned int generation;
	unsigned int game;
	unsigned long long analysis_profile_hash;
	unsigned long long topology_hash;
} route_analysis_cache_key;

typedef struct route_analysis_cache_summary {
	unsigned int generation;
	unsigned int hits;
	unsigned int misses;
	unsigned int writes;
	unsigned int rejections;
	unsigned int io_errors;
	unsigned int publication_adoption_attempts;
	unsigned int publication_adoption_failures;
	unsigned int live_reuse_attempts;
	unsigned int live_reuses;
	unsigned int live_fallbacks;
	unsigned int live_certifier_attempts;
	unsigned int live_certifier_successes;
	unsigned int live_certifier_prepared_fallbacks;
	unsigned int live_certifier_failures;
	unsigned int live_certifier_max_visited_segments;
	unsigned int live_certifier_max_evaluated_edges;
	unsigned int live_certifier_max_evaluated_actions;
	unsigned long long live_reuse_total_us;
	unsigned long long live_reuse_max_us;
	unsigned long long live_reuse_median_us;
	unsigned long long live_reuse_p95_us;
	unsigned long long live_reuse_samples[ROUTE_ANALYSIS_TIMING_SAMPLE_CAPACITY];
	unsigned int live_reuse_sample_count;
	unsigned int live_reuse_sample_next;
	unsigned long long live_fallback_total_us;
	unsigned long long live_fallback_max_us;
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
