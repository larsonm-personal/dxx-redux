#define DXX_REWIND_FILE_CORE_ONLY
#define DXX_REWIND_FILE_WRAPPER
#include "guidebot_metadata_snapshot.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(condition)                                                    \
	do {                                                                    \
		if (!(condition)) {                                                 \
			fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #condition); \
			exit(1);                                                        \
		}                                                                   \
	} while (0)

static void populate(guidebot_metadata_snapshot *s)
{
	int i;
	s->canonical_state.route_step_count = 4;
	s->canonical_state.mine_volume = 12345.6789012345;
	s->canonical_state.travel_distance = -0.0;
	s->canonical_state.route_required_key_mask = 5;
	strcpy(s->canonical_state.guidebot_note, "snapshot-string-corruption-target");
	for (i = 0; i < LEVEL_METADATA_MAX_ROUTE_STEPS; ++i) {
		level_metadata_route_step *step = &s->canonical_state.route_steps[i];
		step->seg = i * 3;
		step->wall_num = -1;
		step->distance_from_previous = i / 7.0;
		step->switch_guidance_candidate_count = 1;
		step->switch_guidance_candidate_seg[0] = i + 2;
		step->switch_guidance_candidate_pos[0][1] = -65536 * (i + 1);
		step->opened_link_count = 1;
		step->opened_link_wall[0] = i;
		snprintf(step->label, sizeof(step->label), "step %d", i);
	}
	s->live_route_state = s->canonical_state;
	s->live_route_state.route_steps[1].seg = 71;
	s->live_candidate_state = s->canonical_state;
	s->live_candidate_state.route_steps[2].seg = 93;
	s->canonical_plan_summary.route_step_count = 4;
	s->canonical_plan_summary.first_pending_step = -1;
	s->canonical_plan_summary_valid = 1;
	s->live_route_state_valid = 1;
	s->canonical_snapshot.topology_hash = UINT64_C(0xfedcba9876543210);
	s->live_snapshot.object_hash = UINT64_C(0x8123456789abcdef);
	s->route_revision = 91234;
	s->published_route_decision.version = GUIDEBOT_ROUTE_DECISION_VERSION;
	s->published_route_decision.objective_segment = 37;
	s->published_route_decision.state_hash = UINT64_C(0x76543210fedcba98);
	s->published_route_decision_valid = 1;
	s->route_certifier_workspace.job_active = 1;
	s->route_certifier_workspace.reach_head = 7;
	s->route_certifier_workspace.reach_tail = 19;
	s->route_certifier_workspace.firing_search_best_score = 1.0L / 7.0L;
	s->route_certifier_workspace.firing_search_detailed_scores[2] = -0.0L;
	for (i = 0; i < LEVEL_METADATA_MAX_SEGMENTS; ++i) {
		s->route_certifier_workspace.reachable[i] = (unsigned char) (i % 2);
		s->route_certifier_workspace.queue[i] = i;
		s->route_certifier_workspace.strategic_distance[i] = i * 13;
		s->route_certifier_workspace.physical_distance[i] = -i;
	}
	s->route_frontier_workspace = s->route_certifier_workspace;
	s->route_frontier_workspace.firing_search_best_score = -123456.123456789L;
	s->live_work_summary.route_ticks = 999;
}

int main(void)
{
	guidebot_metadata_snapshot *live = calloc(1, sizeof(*live));
	guidebot_metadata_snapshot *frozen = calloc(1, sizeof(*frozen));
	guidebot_metadata_snapshot *decoded = calloc(1, sizeof(*decoded));
	guidebot_metadata_snapshot *before = calloc(1, sizeof(*before));
	const size_t size = guidebot_metadata_snapshot_encoded_size();
	unsigned char *original = malloc(size + 1), *encoded = malloc(size), *bad = malloc(size);
	size_t i, marker = size;
	const size_t lengths[] = { 0, 1, 8, size / 2, size - 1, size + 1 };
	CHECK(live && frozen && decoded && before && original && encoded && bad && size);
	populate(live);
	memcpy(frozen, live, sizeof(*live));
	memcpy(before, frozen, sizeof(*frozen));
	CHECK(guidebot_metadata_snapshot_encode(frozen, original, size, 1234567));
	CHECK(!memcmp(frozen, before, sizeof(*frozen)));
	/* Simulate the game changing immediately after capture */
	memset(live, 0x5a, sizeof(*live));
	CHECK(guidebot_metadata_snapshot_encode(frozen, encoded, size, 1234567));
	CHECK(!memcmp(original, encoded, size));
	for (i = 0; i < 32; ++i) {
		CHECK(guidebot_metadata_snapshot_decode(decoded, encoded, size, 1234567));
		CHECK(decoded->canonical_state.route_steps[3].seg == 9);
		CHECK(decoded->live_route_state.route_steps[1].seg == 71);
		CHECK(decoded->route_certifier_workspace.strategic_distance[101] == 1313);
		CHECK(decoded->canonical_snapshot.topology_hash == UINT64_C(0xfedcba9876543210));
		CHECK(decoded->route_certifier_workspace.firing_search_best_score == 1.0L / 7.0L);
		memcpy(before, decoded, sizeof(*decoded));
		CHECK(guidebot_metadata_snapshot_encode(decoded, encoded, size, 1234567));
		CHECK(!memcmp(before, decoded, sizeof(*decoded)));
		CHECK(!memcmp(original, encoded, size));
	}
	/* A failed load must not partially apply a valid prefix */
	for (i = 0; i < sizeof(lengths) / sizeof(lengths[0]); ++i) {
		CHECK(!guidebot_metadata_snapshot_decode(decoded, original, lengths[i], 1234567));
		CHECK(!memcmp(before, decoded, sizeof(*decoded)));
	}
	memcpy(bad, original, size);
	for (i = 0; i + 128 <= size; ++i)
		if (!memcmp(bad + i, "snapshot-string-corruption-target", 32)) {
			marker = i;
			break;
		}
	CHECK(marker < size);
	memset(bad + marker, 'x', 128);
	CHECK(!guidebot_metadata_snapshot_decode(decoded, bad, size, 1234567));
	CHECK(!memcmp(before, decoded, sizeof(*decoded)));
	CHECK(!memcmp(original, encoded, size));
	free(live);
	free(frozen);
	free(decoded);
	free(before);
	free(original);
	free(encoded);
	free(bad);
	puts("PASS: detached snapshot ownership, 32 save/load cycles, exact bytes and rejected-load immutability");
	return 0;
}
