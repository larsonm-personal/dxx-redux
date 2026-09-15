#ifndef COOP_PORTABLE_H
#define COOP_PORTABLE_H

#include <stdint.h>

/* Android's supported ABIs are little-endian. This fixed-width packed record
 * follows the recovery/save wire convention, with no pointers or object IDs
 * Keep the 228-byte MULTI_COOP_TRAVEL envelope in both multi.h files in sync */
#define COOP_PORTABLE_BYTES 172
#pragma pack(push, 1)
typedef struct coop_portable_player {
	uint32_t flags;
	int32_t energy;
	int32_t shields;
	uint32_t restore_serial;
	uint32_t life;
	int32_t score;
	int32_t last_score;
	int32_t time_level;
	int32_t time_total;
	uint16_t primary_ammo[10];
	uint16_t secondary_ammo[10];
	uint16_t primary_weapon_flags;
	uint16_t secondary_weapon_flags;
	int16_t net_killed_total;
	int16_t net_kills_total;
	int16_t num_kills_level;
	int16_t num_kills_total;
	int16_t num_robots_level;
	int16_t num_robots_total;
	int16_t KillGoalCount;
	uint16_t hostages_rescued_total;
	uint16_t hostages_total;
	uint8_t laser_level;
	uint8_t lives;
	int8_t starting_level;
	int8_t primary_weapon;
	int8_t secondary_weapon;
	uint8_t hostages_on_board;
	uint8_t hostages_level;
	int8_t hours_level;
	int8_t hours_total;
	int32_t omega;
	int32_t afterburner_charge;
	int32_t fusion;
	int32_t respawning_concussions;
	int32_t vulcan_boxes;
	int32_t vulcan_box_ammo;
	int64_t cloak_age;
	int64_t invulnerable_age;
	int64_t laser_wait;
	int64_t missile_wait;
	int64_t fusion_wait;
	uint8_t fusion_armed;
} coop_portable_player;
#pragma pack(pop)
typedef char coop_portable_size_check[sizeof(coop_portable_player) == COOP_PORTABLE_BYTES ? 1 : -1];

static inline int coop_portable_valid(const coop_portable_player *p)
{
	const int64_t max_timer = INT64_C(18000) * 65536;
	return p && p->shields > 0 && p->energy >= 0 && p->laser_level <= 5 &&
	       !(p->flags & ~UINT32_C(0x7fff)) &&
	       !(p->primary_weapon_flags & ~1023u) && !(p->secondary_weapon_flags & ~1023u) &&
	       p->primary_weapon >= 0 && p->primary_weapon < 10 &&
	       p->secondary_weapon >= 0 && p->secondary_weapon < 10 &&
	       p->omega >= 0 && p->omega <= 65536 &&
	       p->afterburner_charge >= 0 && p->afterburner_charge <= 65536 &&
	       p->fusion >= 0 && p->fusion_armed <= 1 &&
	       p->respawning_concussions >= 0 && p->vulcan_boxes >= 0 && p->vulcan_box_ammo >= 0 &&
	       p->cloak_age >= -max_timer && p->cloak_age <= max_timer &&
	       p->invulnerable_age >= -max_timer && p->invulnerable_age <= max_timer &&
	       p->laser_wait >= 0 && p->laser_wait <= max_timer &&
	       p->missile_wait >= 0 && p->missile_wait <= max_timer &&
	       p->fusion_wait >= 0 && p->fusion_wait <= max_timer;
}

/* Capture only the local frozen player; apply retains local object/slot identity */
int coop_capture_portable(coop_portable_player *record);
int coop_apply_portable(int player_num, const coop_portable_player *record);

#endif
