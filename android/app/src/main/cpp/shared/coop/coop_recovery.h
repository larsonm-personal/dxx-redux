#ifndef COOP_RECOVERY_H
#define COOP_RECOVERY_H

#include <stddef.h>
#include <stdint.h>
#include "coop_save.h"
#include "object.h"

/* Android coop protocol: packed, fixed-width records on our little-endian ABIs
 * Keep MULTI_COOP_RECOVERY's 160-byte envelope in both multi.h files in sync */
#pragma pack(push, 1)
typedef struct coop_recovery_gear {
	uint16_t primary;
	uint16_t laser;
	uint32_t flags;
	uint32_t vulcan;
	uint16_t missiles[COOP_SAVE_MAX_WEAPONS];
	int32_t omega;
} coop_recovery_gear;

enum { COOP_RECOVERY_LIVE = 1,
	   COOP_RECOVERY_CREDIT,
	   COOP_RECOVERY_TAKEN,
	   COOP_RECOVERY_DEPARTED,
	   COOP_RECOVERY_ALIVE,
	   COOP_RECOVERY_RECLAIMING };
typedef struct coop_recovery_item {
	uint32_t id;
	uint32_t revision;
	uint32_t life;
	char client_id[COOP_CLIENT_ID_LEN + 1];
	char callsign[COOP_CALLSIGN_LEN + 1];
	int32_t signature;
	int16_t object_index;
	int16_t remote_index;
	int8_t network_owner;
	uint8_t powerup;
	uint8_t state;
	coop_recovery_gear gear;
} coop_recovery_item;
#pragma pack(pop)

int coop_recovery_active(void);
void coop_recovery_reset(void);
void coop_recovery_begin_restore(uint32_t generation);
void coop_recovery_end_restore(void);
void coop_recovery_drop(int pnum, uint32_t life);
void coop_recovery_begin_drop(int pnum);
void coop_recovery_consume_mine(int pnum, int secondary, int objnum);
uint32_t coop_recovery_life(int pnum);
void coop_recovery_set_life(int pnum, uint32_t life);
void coop_recovery_remember_record(const coop_player_record *rec);
fix coop_recovery_omega(int pnum);
void coop_recovery_set_omega(int pnum, fix charge);
void coop_recovery_alive(int pnum);
void coop_recovery_departure_record(int pnum, coop_player_record *rec);
int coop_recovery_pickup_blocked(const object *powerup);
int coop_recovery_pickup_count(const object *powerup, int secondary, int amount);
void coop_recovery_note_remove(object *powerup);
void coop_recovery_note_pickup(object *powerup, const coop_player_record *before, int used);
int coop_recovery_rejoin_ready(const char *callsign, const char *client_id);
void coop_recovery_expire(object *powerup);
void coop_recovery_level_leave(void);
void coop_recovery_frame(void);
void coop_recovery_receive(const ubyte *buf, int sender);
void coop_recovery_send_snapshot(int pnum);
void coop_recovery_host_changed(void);
int coop_recovery_save_ready(void);
int coop_recovery_prepare_rejoin(int pnum, coop_player_record *rec);
uint32_t coop_recovery_restore_serial(int pnum);
int coop_recovery_accept_ship_status(int pnum, uint32_t revision, uint32_t life);
void coop_recovery_set_restore_serial(int pnum, uint32_t revision);
uint32_t coop_recovery_epoch(void);
int coop_recovery_accept_restore(uint32_t generation, uint32_t revision);
size_t coop_recovery_count(void);
const coop_recovery_item *coop_recovery_data(void);
int coop_recovery_set_pending(const coop_recovery_item *items, size_t count);
int coop_recovery_apply_pending(void);

#endif
