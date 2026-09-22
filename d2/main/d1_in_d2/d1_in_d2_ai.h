/* D1 robot operations; callers dispatch at the existing simulation phase */
#ifndef D1_IN_D2_AI_H
#define D1_IN_D2_AI_H

#include "vecmat.h"
#include "aistruct.h"
struct object;
struct segment;
struct ai_local;
struct robot_info;

enum d1_ai_actor_role { D1_AI_ENGINE_ACTOR, D1_AI_NATIVE_ENEMY };
/* Select at an AI phase boundary; optional companions retain engine policy */
enum d1_ai_actor_role d1_in_d2_ai_actor_role(const struct object *obj);
/* HUD cameras may wake engine actors, never native enemies. The native hide
 * submode shares storage with D2 camera flags and must remain untouched */
int d1_in_d2_ai_camera_can_wake(const struct object *obj);
/* Native enemies have neither flash stun nor D2 boss blast resistance */
int d1_in_d2_ai_flash_can_stun(const struct object *obj);
fix d1_in_d2_ai_robot_blast_damage(const struct object *obj, fix damage);

/* Level preparation owns native destination eligibility and base boss intervals
 * Uses the shared boss segment arrays; inactive leaves them and clocks untouched */
int d1_in_d2_ai_initialize_boss(void);
fix d1_in_d2_ai_gate_interval(fix engine_interval);
/* Gate creation has independent network callers as well as the boss frame
 * Returns zero when inactive without touching result/state/RNG; handled writes
 * the created object index, or -1 on failure, using the engine gate API contract */
int d1_in_d2_ai_gate_robot(int type, int segment, int *result);
/* Boss state belongs to the native phase; reset at level/session retirement
 * The save adapter tags its flag in D1 content's otherwise unused hit-time slot
 * Ordinary D2 values pass through unchanged; older untagged D1 saves have no hit */
void d1_in_d2_ai_reset_boss_state(void);
void d1_in_d2_ai_restore_boss_hit(int pending);
fix d1_in_d2_ai_save_boss_hit(fix engine_delta);
fix64 d1_in_d2_ai_restore_boss_hit_time(fix saved);
int d1_in_d2_ai_boss_weapon_hit(const struct object *obj);
/* After shared shield subtraction/diagnostics: -1 inactive, otherwise the
 * native damage result, including kill accounting and death publication */
int d1_in_d2_ai_finish_boss_damage(struct object *obj, int killer);
/* Complete native frame, including early returns and completion
 * Route/replica guards run in the engine before this dispatch
 * Zero leaves engine actors untouched, including state, diagnostics and RNG */
int d1_in_d2_ai_run_frame(struct object *obj);
/* Complete world event propagation and actor delivery, before frame diagnostics
 * Native enemies receive D1 depth; companions retain engine depth/availability
 * Consumes the shared queue once, using temporary maps and no RNG
 * Zero leaves D2 events and actor state untouched */
int d1_in_d2_ai_deliver_awareness(void);
/* World completion after diagnostics: retire companion camera state and run
 * engine-actor boss completion only. Native boss death stays in its own frame
 * Zero leaves ordinary D2 completion active without state or RNG changes */
int d1_in_d2_ai_finish_world_frame(void);
/* Complete source hit response; inactive companions retain engine policy */
int d1_in_d2_ai_robot_hit(struct object *obj, int type);
/* Complete event production: D1 has no multiplayer-with-robots admission gate
 * Reuses the proven common producer, including observer rejection/diagnostics
 * Zero leaves D2 state, RNG and pending diagnostic source untouched */
int d1_in_d2_ai_create_awareness_event(struct object *obj, int type);

enum { D1_AI_NOT_APPLICABLE = -1 };
/* Door queries also accept ConsoleObject; null/companion use engine policy */
int d1_in_d2_ai_door_is_openable(struct object *obj, struct segment *seg, int side);
/* Existing path API: 0 success (including reachable partial paths), -1 failure
 * -2 leaves the engine path active, with no state/output/RNG changes
 * Shared-pool output is bounded; external buffers retain the engine API's
 * caller-provided capacity contract. May reset shared paths on exhaustion */
enum { D1_AI_PATH_NOT_APPLICABLE = -2 };
int d1_in_d2_ai_create_path_points(struct object *obj, int start, int end, point_seg *points,
	short *count, int max_depth, int random, int safety, int avoid);
/* Complete native requests; handled includes failure. Own destination, path
 * publication, mode/submode/awareness and SIM draws. Companion is untouched */
int d1_in_d2_ai_create_path_to_player(struct object *obj, int max_length, int safety);
int d1_in_d2_ai_create_path_to_station(struct object *obj, int max_length);
int d1_in_d2_ai_create_random_path(struct object *obj, int length, int avoid);
/* Complete native follow/replan and motion operation, including hide submode
 * Uses shared point storage and SIM RNG; inactive leaves all state untouched */
int d1_in_d2_ai_follow_path(struct object *obj, int visibility);
/* Configure source behavior and path goals after the caller clears ai_local
 * Shared clocks, physics, cloak and network fields remain in init_ai_object
 * Inactive or companion actors return zero without mutation or RNG draws */
int d1_in_d2_ai_initialize_behavior(struct object *obj, struct ai_local *local, int behavior, int hide_segment);
/* Melee collision and native shots share the primary burst clock
 * Zero leaves ordinary D2/companion firing timing active */
int d1_in_d2_ai_next_fire_time(const struct object *obj, struct ai_local *ailp, struct robot_info *robptr);

#endif
