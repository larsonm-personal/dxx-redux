#ifndef COOP_POWERUP_DUPLICATION_H
#define COOP_POWERUP_DUPLICATION_H

#include <stddef.h>
#include <stdint.h>

#include "object.h"
#include "pstypes.h"

#define COOP_POWERUP_CLIENT_ID_LEN 36
#define COOP_POWERUP_CALLSIGN_LEN  8

typedef struct coop_powerup_collection {
	int16_t object_index;
	int32_t object_signature;
	uint8_t powerup_id;
	char client_id[COOP_POWERUP_CLIENT_ID_LEN + 1];
	char callsign[COOP_POWERUP_CALLSIGN_LEN + 1];
} coop_powerup_collection;

int coop_powerup_duplication_active(void);
int coop_powerup_duplication_eligible(const object *powerup);
int coop_powerup_duplication_collected(const object *powerup, int pnum);
int coop_powerup_duplication_record(const object *powerup, int pnum);
int coop_powerup_duplication_hide(const object *powerup);
void coop_powerup_duplication_reset(void);
size_t coop_powerup_duplication_count(void);
const coop_powerup_collection *coop_powerup_duplication_data(void);
int coop_powerup_duplication_replace(const coop_powerup_collection *items,
                                     size_t count);
int coop_powerup_duplication_set_pending(
    const coop_powerup_collection *items, size_t count);
int coop_powerup_duplication_apply_pending(void);

void coop_powerup_duplication_send(const object *powerup);
void coop_powerup_duplication_receive(const ubyte *buf);
void coop_powerup_duplication_send_snapshot(int pnum);
void coop_powerup_duplication_receive_snapshot_begin(const ubyte *buf);
void coop_powerup_duplication_receive_snapshot_entry(const ubyte *buf);
void coop_powerup_duplication_receive_snapshot_end(const ubyte *buf);

#endif
