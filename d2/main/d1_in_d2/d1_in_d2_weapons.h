/* D1 projectile selection and runtime weapon phases */

#ifndef _D1_IN_D2_WEAPONS_H
#define _D1_IN_D2_WEAPONS_H

#include "fix.h"
#include "vecmat.h"

struct object;
struct PHYSFS_File;

enum { D1_IN_D2_PRIMARY_ORDER_COUNT = 7, D1_IN_D2_SECONDARY_ORDER_COUNT = 6 };
typedef struct d1_in_d2_weapon_order_config {
	ubyte primary[D1_IN_D2_PRIMARY_ORDER_COUNT];
	ubyte secondary[D1_IN_D2_SECONDARY_ORDER_COUNT];
} d1_in_d2_weapon_order_config;

void d1_in_d2_reset_weapon_order(void);
const ubyte *d1_in_d2_weapon_order(int secondary);
int d1_in_d2_set_weapon_order(int secondary, const ubyte *order, int count);
void d1_in_d2_read_weapon_order(struct PHYSFS_File *file);
void d1_in_d2_write_weapon_order(struct PHYSFS_File *file);
/* True means the native operation handled the call */
int d1_in_d2_cycle_weapon(int secondary);
int d1_in_d2_auto_select_weapon(int secondary, int classic);
int d1_in_d2_reorder_weapons(int secondary);
/* -1 leaves the D2 inventory/ammo/energy query active */
int d1_in_d2_weapon_availability(ubyte player_num, int weapon_index, int secondary);

/* Native quad selection uses a logical index distinct from the inventory bit */
int d1_in_d2_is_quad_selection(int weapon_index);
int d1_in_d2_primary_selection_index(int weapon_index);
/* Negative results leave ordinary D2 ordering/acquisition active */
int d1_in_d2_primary_order(int weapon_index);
int d1_in_d2_secondary_order(int weapon_index);
int d1_in_d2_pick_up_primary(int weapon_index, int is_quads);

/* Native collision relationship; -1 leaves D2 handling active */
int d1_in_d2_laser_are_related(int first, int second);

/* For player firing only; accounting records and robot weapon IDs stay intact */
int d1_in_d2_primary_projectile(int default_projectile);
int d1_in_d2_initialize_player_weapon(struct object *weapon, fix fusion_charge, int game_mode);
int d1_in_d2_position_weapon(struct object *weapon, const struct object *parent, const vms_vector *direction, fix length);
void d1_in_d2_notify_player_fire(struct object *player, int weapon_type);
int d1_in_d2_limit_weapon_speed(struct object *weapon);
/* Native bouncing projectiles skip the entire wall-impact operation, including
 * monitor/door activation, explosions and awareness/RNG effects */
int d1_in_d2_weapon_wall_impact_allowed(const struct object *weapon);
int d1_in_d2_smart_child(const struct object *parent, int default_child);

/* Configure the D1 acquisition/retention cones; inactive leaves D2 globals alone */
int d1_in_d2_configure_homing(fix turn_time);
/* Target operations return an object index or -1; -2 leaves the D2 path active */
enum { D1_HOMING_NOT_HANDLED = -2 };
int d1_in_d2_acquire_homing_target(vms_vector *position, struct object *tracker);
int d1_in_d2_scan_homing_targets(vms_vector *position, struct object *tracker, int type1, int type2);
int d1_in_d2_track_homing_target(int target, struct object *tracker, fix *dot, unsigned frame, int original_homing);

/* Owns the steering phase after target selection; inactive returns without mutation */
int d1_in_d2_turn_homing_weapon(struct object *weapon, int target, fix *dot, fix turn_time, int original_homing);

#endif /* _D1_IN_D2_WEAPONS_H */
