/*
 *
 * Small D1-in-D2 gameplay policy helpers.
 *
 */

#ifndef _D1_IN_D2_SEMANTICS_H
#define _D1_IN_D2_SEMANTICS_H

#include "vecmat.h"

struct object;
struct segment;
struct active_door;

/* Small difficulty rules at the existing pickup/damage calculation phase */
fix d1_in_d2_pickup_boost(fix base_boost);
fix d1_in_d2_contact_damage(fix damage);
fix d1_in_d2_blast_damage(fix damage);
/* Native D1 still bumps an exploding robot; engine actors retain D2 filtering */
int d1_in_d2_robot_contact_allowed(const struct object *robot);
int d1_in_d2_robot_pair_collides(const struct object *first, const struct object *second);
/* Native bounce keeps projectile orientation and derives ship/robot/debris
 * velocity from actual travel; D2 retains the reflected velocity instead */
int d1_in_d2_bounce_preserves_velocity(const struct object *obj);

/* Door phase queries: -1 leaves D2 handling intact, otherwise a boolean result
 * Closing checks native obstruction without reopening or advancing animation */
int d1_in_d2_door_close_blocked(const struct active_door *door);
int d1_in_d2_door_wait_elapsed(const struct active_door *door);

/* Own the complete native replacement/drop decision and its SIM RNG draws
 * Returns handled for D1; inactive leaves the object and RNG untouched */
int d1_in_d2_replace_powerup(struct object *container);

int d1_in_d2_reactor_countdown(int engine_seconds);

vms_vector *d1_in_d2_badass_explosion_pos(struct object *weapon, vms_vector *collision_point);
vms_vector *d1_in_d2_prepare_player_explosion_pos(struct object *weapon, vms_vector *collision_point);

int d1_in_d2_use_d2_resource_drop_suppression(const struct object *objp, int game_mode);

/* Handled native physics operations; zero leaves D2/companion state and RNG
 * untouched so their engine phase can run at the original call site */
int d1_in_d2_integrate_rotation(struct object *obj, int count, fix partial_step, fix drag);
int d1_in_d2_robot_rotational_hit(struct object *obj, vms_vector *force_vec, fix rate, fix magnitude);
void d1_in_d2_note_physics_result(const struct object *obj, const vms_vector *start,
	int fate, int stopped, int bounced);

/* D1 crossing policy only; the caller owns intersection search and traversal
 * May populate bitmap/merge/RLE caches, but never changes world state or RNG */
int d1_in_d2_transparent_wall_crossable(int wid_flag, struct segment *seg,
	int side, int flags, vms_vector *hit_point, int face, short objnum);

/* Replay observation at the shared traversal boundary; no simulation effects */
void d1_in_d2_note_wall_boundary(short objnum, int startseg, int side,
	int face, int face_hit_type, int wid_flag, int startmask, int endmask,
	int centermask, int flags, const vms_vector *p0, const vms_vector *p1,
	const vms_vector *hit_point, fix rad);

#endif /* _D1_IN_D2_SEMANTICS_H */
