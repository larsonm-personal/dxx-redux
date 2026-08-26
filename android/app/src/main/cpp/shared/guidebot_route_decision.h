#ifndef DXX_GUIDEBOT_ROUTE_DECISION_H
#define DXX_GUIDEBOT_ROUTE_DECISION_H

#include "route_planner_c.h"
#include "route_snapshot_c.h"

#ifdef __cplusplus
extern "C" {
#endif

#define GUIDEBOT_ROUTE_DECISION_VERSION       2
#define GUIDEBOT_ROUTE_SHADOW_FIXTURE_VERSION 1

enum guidebot_route_decision_status {
	GUIDEBOT_ROUTE_DECISION_UNAVAILABLE = 0,
	GUIDEBOT_ROUTE_DECISION_CALCULATING = 1,
	GUIDEBOT_ROUTE_DECISION_READY = 2,
	GUIDEBOT_ROUTE_DECISION_COMPLETE = 3,
	GUIDEBOT_ROUTE_DECISION_PARTIAL = 4,
	GUIDEBOT_ROUTE_DECISION_FAILED = 5
};

enum guidebot_route_decision_dependency {
	GUIDEBOT_ROUTE_DEPENDENCY_TOPOLOGY = 1u << 0,
	GUIDEBOT_ROUTE_DEPENDENCY_START = 1u << 1,
	GUIDEBOT_ROUTE_DEPENDENCY_PROGRESSION = 1u << 2,
	GUIDEBOT_ROUTE_DEPENDENCY_NAVIGATION = 1u << 3,
	GUIDEBOT_ROUTE_DEPENDENCY_TRIGGERS = 1u << 4,
	GUIDEBOT_ROUTE_DEPENDENCY_OBJECTS = 1u << 5,
	GUIDEBOT_ROUTE_DEPENDENCY_AUTOMAP = 1u << 6,
	GUIDEBOT_ROUTE_DEPENDENCY_TARGET_POLICY = 1u << 7,
	GUIDEBOT_ROUTE_DEPENDENCY_ACTOR = 1u << 8
};

enum guidebot_route_certificate_status {
	GUIDEBOT_ROUTE_CERTIFICATE_UNCHECKED = 0,
	GUIDEBOT_ROUTE_CERTIFICATE_VALID = 1,
	GUIDEBOT_ROUTE_CERTIFICATE_INVALID = 2
};

enum guidebot_route_shadow_mismatch_kind {
	GUIDEBOT_ROUTE_SHADOW_MATCH = 0,
	GUIDEBOT_ROUTE_SHADOW_AVAILABILITY_MISMATCH = 1,
	GUIDEBOT_ROUTE_SHADOW_SEMANTIC_MISMATCH = 2,
	GUIDEBOT_ROUTE_SHADOW_GUIDANCE_MISMATCH = 3,
	GUIDEBOT_ROUTE_SHADOW_PROOF_MISMATCH = 4
};

enum guidebot_route_shadow_reason_kind {
	GUIDEBOT_ROUTE_SHADOW_REASON_NONE = 0,
	GUIDEBOT_ROUTE_SHADOW_REASON_STALE_COMPLETION_HISTORY = 1,
	GUIDEBOT_ROUTE_SHADOW_REASON_REVERSIBLE_WALL_STATE = 2,
	GUIDEBOT_ROUTE_SHADOW_REASON_OUT_OF_ORDER_ACTION = 3,
	GUIDEBOT_ROUTE_SHADOW_REASON_ACTOR_ACCESSIBILITY = 4,
	GUIDEBOT_ROUTE_SHADOW_REASON_TARGET_RANKING = 5,
	GUIDEBOT_ROUTE_SHADOW_REASON_ACTIVATION_VISIBILITY = 6,
	GUIDEBOT_ROUTE_SHADOW_REASON_CACHE_READINESS = 7,
	GUIDEBOT_ROUTE_SHADOW_REASON_PLANNER_DEFECT = 8
};

enum guidebot_route_shadow_hint {
	GUIDEBOT_ROUTE_SHADOW_HINT_STALE_COMPLETION_HISTORY = 1u << 0,
	GUIDEBOT_ROUTE_SHADOW_HINT_REVERSIBLE_WALL_STATE = 1u << 1,
	GUIDEBOT_ROUTE_SHADOW_HINT_CACHE_READINESS = 1u << 2
};

enum guidebot_route_adoption_action {
	GUIDEBOT_ROUTE_ADOPTION_STOP = 0,
	GUIDEBOT_ROUTE_ADOPTION_RETAIN_PATH = 1,
	GUIDEBOT_ROUTE_ADOPTION_REPLACE_PATH = 2
};

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif

typedef struct guidebot_route_validity_certificate {
	int status;
	int source_trigger;
	int source_wall;
	int source_object;
	int frontier_segment;
} guidebot_route_validity_certificate;

typedef struct guidebot_route_decision {
	unsigned int version;
	int status;
	int target_policy;
	int requested_target_segment;
	int objective_kind;
	int activation_kind;
	int objective_trigger;
	int objective_wall;
	int objective_key;
	int objective_object;
	int objective_segment;
	int objective_side;
	int guidance_position_valid;
	int guidance_position[3];
	int path_terminal_segment;
	int partial_frontier_segment;
	unsigned int dependency_mask;
	unsigned long long topology_hash;
	unsigned long long state_hash;
	unsigned long long start_hash;
	unsigned long long progression_hash;
	unsigned long long navigation_hash;
	unsigned long long trigger_hash;
	unsigned long long object_hash;
	unsigned long long automap_hash;
	unsigned long long actor_hash;
	unsigned long long input_hash;
	unsigned long long semantic_hash;
	unsigned long long guidance_hash;
	unsigned long long decision_hash;
	guidebot_route_validity_certificate certificate;
} guidebot_route_decision;

typedef struct guidebot_route_objective_identity {
	int kind;
	int trigger;
	int wall;
	int object;
	int segment;
	int side;
} guidebot_route_objective_identity;

typedef struct guidebot_route_shadow_fixture {
	unsigned int version;
	unsigned int hints;
	int game;
	int level;
	int mismatch_kind;
	int reason_kind;
	route_snapshot_summary snapshot;
} guidebot_route_shadow_fixture;

typedef struct guidebot_route_shadow_summary {
	int enabled;
	int attempted;
	int primary_valid;
	int shadow_valid;
	int mismatch_kind;
	unsigned int attempts;
	unsigned int semantic_matches;
	unsigned int guidance_matches;
	unsigned int availability_mismatches;
	unsigned int semantic_mismatches;
	unsigned int guidance_mismatches;
	unsigned int proof_mismatches;
	unsigned int last_fvi_count;
	unsigned long long total_us;
	unsigned long long max_us;
	guidebot_route_decision primary;
	guidebot_route_decision shadow;
	guidebot_route_shadow_fixture fixture;
} guidebot_route_shadow_summary;

#ifdef _MSC_VER
#pragma pack(pop)
#endif

void guidebot_route_decision_clear(guidebot_route_decision *decision);
int guidebot_route_objective_identity_compare(
    const guidebot_route_objective_identity *left,
    const guidebot_route_objective_identity *right);
int guidebot_route_decision_project(
    const level_metadata_state *state,
    const route_planner_plan_summary *plan,
    const route_snapshot_summary *snapshot,
    int readiness,
    int requested_target_segment,
    guidebot_route_decision *decision);
int guidebot_route_decision_semantic_equal(
    const guidebot_route_decision *left,
    const guidebot_route_decision *right);
int guidebot_route_decision_guidance_equal(
    const guidebot_route_decision *left,
    const guidebot_route_decision *right);
int guidebot_route_decision_supports_exit_command(
    const guidebot_route_decision *decision);
int guidebot_route_decision_adoption_action(
    int previous_valid,
    const guidebot_route_decision *previous,
    int previous_certificate_valid,
    int next_valid,
    const guidebot_route_decision *next);
const char *guidebot_route_decision_status_name(int status);
const char *guidebot_route_shadow_mismatch_name(int kind);
const char *guidebot_route_shadow_reason_name(int kind);
void guidebot_route_shadow_record(
    guidebot_route_shadow_summary *summary,
    int primary_valid,
    const guidebot_route_decision *primary,
    int shadow_valid,
    const guidebot_route_decision *shadow,
    unsigned int fvi_count,
    unsigned long long elapsed_us,
    const route_snapshot_summary *snapshot,
    int game,
    int level,
    unsigned int hints);

#ifdef __cplusplus
}
#endif

#endif
