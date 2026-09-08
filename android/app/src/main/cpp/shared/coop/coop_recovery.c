#include <stdlib.h>
#include <stdio.h>
#include "android_log.h"
#include <string.h>

#include "coop_recovery.h"
#include "byteswap.h"
#include "console.h"
#include "dxxerror.h"
#ifdef DXX_BUILD_DESCENT_II
#include "laser.h"
#endif
#include "game.h"
#include "multi.h"
#include "powerup.h"
#include "strutil.h"
#include "timer.h"
#include "weapon.h"

enum { REC_ITEM = 1,
	   REC_COLLECT,
	   REC_FREEZE,
	   REC_FROZEN,
	   REC_BEGIN,
	   REC_END,
	   REC_LIFE };
static coop_recovery_item *items, *pending;
static size_t count, capacity, pending_count;
static uint32_t next_id = 1, epoch = 1, restore_epoch;
/* Restore serials fence repeated inventory restores, not ordinary pickups */
static uint32_t restore_serial[MAX_PLAYERS], applied_restore_serial[MAX_PLAYERS];
static uint32_t player_life[MAX_PLAYERS];
static fix omega_charge[MAX_PLAYERS];
static uint16_t armed_mines[MAX_PLAYERS][COOP_SAVE_MAX_WEAPONS];
static uint8_t *freeze_waiting;
static fix64 freeze_sent_at;

static uint32_t equipment_flags(void)
{
	uint32_t flags = PLAYER_FLAGS_QUAD_LASERS;
#ifdef DXX_BUILD_DESCENT_II
	flags |= PLAYER_FLAGS_MAP_ALL | PLAYER_FLAGS_AFTERBURNER |
	         PLAYER_FLAGS_AMMO_RACK | PLAYER_FLAGS_CONVERTER | PLAYER_FLAGS_HEADLIGHT;
#endif
	return flags;
}

int coop_recovery_active(void)
{
	return (Game_mode & GM_MULTI_COOP) && (Netgame.game_flags & NETGAME_FLAG_COOP_QOL);
}

static int same_owner(const coop_recovery_item *item, int pnum)
{
	const char *id = Netgame.players[pnum].client_id;
	if (id[0] && item->client_id[0])
		return !strcmp(id, item->client_id);
	return !d_stricmp(Players[pnum].callsign, item->callsign);
}

static void set_owner(coop_recovery_item *item, int pnum)
{
	strncpy(item->client_id, Netgame.players[pnum].client_id, COOP_CLIENT_ID_LEN);
	strncpy(item->callsign, Players[pnum].callsign, COOP_CALLSIGN_LEN);
}

static int reserve(size_t needed)
{
	coop_recovery_item *grown;
	unsigned char *flags;
	size_t size = capacity ? capacity : 32;
	if (needed <= capacity)
		return 1;
	while (size < needed) {
		if (size > SIZE_MAX / 2 / sizeof(*items))
			return 0;
		size *= 2;
	}
	grown = (coop_recovery_item *) calloc(size, sizeof(*items));
	flags = (unsigned char *) calloc(size, 1);
	if (!grown || !flags) {
		free(grown);
		free(flags);
		return 0;
	}
	if (count) {
		memcpy(grown, items, count * sizeof(*items));
		memcpy(flags, freeze_waiting, count);
	}
	free(items);
	free(freeze_waiting);
	items = grown;
	freeze_waiting = flags;
	capacity = size;
	return 1;
}

static int empty(const coop_recovery_gear *gear)
{
	coop_recovery_gear zero = { 0 };
	return !memcmp(gear, &zero, sizeof(zero));
}

static coop_recovery_item *append(int pnum, int state)
{
	coop_recovery_item *item;
	if (!next_id || !reserve(count + 1))
		return NULL;
	item = &items[count++];
	memset(item, 0, sizeof(*item));
	item->id = next_id++;
	item->revision = 1;
	item->state = (uint8_t) state;
	item->object_index = item->remote_index = -1;
	set_owner(item, pnum);
	return item;
}

static void send_item(const coop_recovery_item *item, int operation, int target)
{
	ubyte buf[160] = { 0 };
	_Static_assert(sizeof(coop_recovery_item) <= 144, "Recovery wire envelope too small");
	buf[0] = MULTI_COOP_RECOVERY;
	buf[1] = (ubyte) operation;
	PUT_INTEL_INT(buf + 4, epoch);
	if (item)
		memcpy(buf + 16, item, sizeof(*item));
	if (target >= 0)
		multi_send_data_direct(buf, sizeof(buf), target, 2);
	else
		multi_send_data(buf, sizeof(buf), 2);
}

static int bound_object(const coop_recovery_item *item)
{
	int objnum = item->object_index;
	if (objnum >= 0 && objnum <= Highest_object_index &&
	    Objects[objnum].signature == item->signature &&
	    Objects[objnum].type == OBJ_POWERUP && Objects[objnum].id == item->powerup)
		return objnum;
	return -1;
}

static void remove_world(coop_recovery_item *item)
{
	int objnum = bound_object(item);
	if (objnum >= 0) {
		Objects[objnum].flags |= OF_SHOULD_BE_DEAD;
		if (multi_i_am_master())
			multi_send_remobj(objnum);
	}
	item->object_index = -1;
}

static coop_recovery_gear from_record(const coop_player_record *rec)
{
	coop_recovery_gear gear = { 0 };
	gear.primary = rec->primary_weapon_flags & ~1u;
	gear.laser = rec->laser_level;
	gear.flags = rec->flags & equipment_flags();
	gear.vulcan = rec->primary_ammo[VULCAN_INDEX];
	gear.omega = rec->omega_charge;
	memcpy(gear.missiles, rec->secondary_ammo, sizeof(gear.missiles));
	return gear;
}

static unsigned int take_amount(unsigned int wanted, unsigned int available)
{
	return wanted < available ? wanted : available;
}

/* Allocate real inventory to an egg, never the minimum ammo/bonus synthesized
 * by legacy death drops. Anything not representable by an egg stays as credit */
static coop_recovery_gear allocate_egg(const object *obj, coop_recovery_gear *pool)
{
	coop_recovery_gear gear = { 0 };
	int primary = -1, secondary = -1, amount = 1;
	uint32_t flags = 0;
	switch (obj->id) {
		case POW_LASER: gear.laser = pool->laser ? 1 : 0; break;
		case POW_QUAD_FIRE: flags = PLAYER_FLAGS_QUAD_LASERS; break;
		case POW_VULCAN_WEAPON: primary = VULCAN_INDEX; break;
		case POW_SPREADFIRE_WEAPON: primary = SPREADFIRE_INDEX; break;
		case POW_PLASMA_WEAPON: primary = PLASMA_INDEX; break;
		case POW_FUSION_WEAPON: primary = FUSION_INDEX; break;
		case POW_VULCAN_AMMO:
			gear.vulcan = take_amount(VULCAN_AMMO_AMOUNT, pool->vulcan);
			break;
		case POW_MISSILE_4: amount = 4; /* fall through */
		case POW_MISSILE_1: secondary = CONCUSSION_INDEX; break;
		case POW_HOMING_AMMO_4: amount = 4; /* fall through */
		case POW_HOMING_AMMO_1: secondary = HOMING_INDEX; break;
		case POW_PROXIMITY_WEAPON:
			secondary = PROXIMITY_INDEX;
			amount = 4;
			break;
		case POW_SMARTBOMB_WEAPON: secondary = SMART_INDEX; break;
		case POW_MEGA_WEAPON: secondary = MEGA_INDEX; break;
#ifdef DXX_BUILD_DESCENT_II
		case POW_SUPER_LASER: gear.laser = (uint16_t) take_amount(pool->laser, 4); break;
		case POW_GAUSS_WEAPON: primary = GAUSS_INDEX; break;
		case POW_HELIX_WEAPON: primary = HELIX_INDEX; break;
		case POW_PHOENIX_WEAPON: primary = PHOENIX_INDEX; break;
		case POW_OMEGA_WEAPON: primary = OMEGA_INDEX; break;
		case POW_FULL_MAP: flags = PLAYER_FLAGS_MAP_ALL; break;
		case POW_CONVERTER: flags = PLAYER_FLAGS_CONVERTER; break;
		case POW_AMMO_RACK: flags = PLAYER_FLAGS_AMMO_RACK; break;
		case POW_AFTERBURNER: flags = PLAYER_FLAGS_AFTERBURNER; break;
		case POW_HEADLIGHT: flags = PLAYER_FLAGS_HEADLIGHT; break;
		case POW_SMISSILE1_4: amount = 4; /* fall through */
		case POW_SMISSILE1_1: secondary = SMISSILE1_INDEX; break;
		case POW_GUIDED_MISSILE_4: amount = 4; /* fall through */
		case POW_GUIDED_MISSILE_1: secondary = GUIDED_INDEX; break;
		case POW_MERCURY_MISSILE_4: amount = 4; /* fall through */
		case POW_MERCURY_MISSILE_1: secondary = SMISSILE4_INDEX; break;
		case POW_SMART_MINE:
			secondary = SMART_MINE_INDEX;
			amount = 4;
			break;
		case POW_EARTHSHAKER_MISSILE: secondary = SMISSILE5_INDEX; break;
#endif
		default: break;
	}
	if (primary >= 0) {
		gear.primary = pool->primary & (1u << primary);
		pool->primary &= ~gear.primary;
		if (primary == VULCAN_INDEX
#ifdef DXX_BUILD_DESCENT_II
		    || primary == GAUSS_INDEX
#endif
		)
			gear.vulcan = take_amount(obj->ctype.powerup_info.count > 0 ? (unsigned int) obj->ctype.powerup_info.count : 0, pool->vulcan);
#ifdef DXX_BUILD_DESCENT_II
		if (primary == OMEGA_INDEX) {
			gear.omega = pool->omega;
			pool->omega = 0;
		}
#endif
	}
	gear.flags = pool->flags & flags;
	pool->flags &= ~gear.flags;
	pool->laser -= gear.laser;
	pool->vulcan -= gear.vulcan;
	if (secondary >= 0) {
		gear.missiles[secondary] = (uint16_t) take_amount(amount, pool->missiles[secondary]);
		pool->missiles[secondary] -= gear.missiles[secondary];
	}
	return gear;
}

static coop_recovery_gear transfer(coop_player_record *rec, coop_recovery_gear *source);

void coop_recovery_drop(int pnum, uint32_t life)
{
	coop_player_record rec;
	coop_recovery_gear pool;
	int i;
	if (!coop_recovery_active() || pnum < 0 || pnum >= MAX_PLAYERS) return;
	if (life < player_life[pnum]) return;
	player_life[pnum] = life + 1;
	/* Mark death spew on every peer before the host ledger can arrive
	 * Deliberate weapon gifts remain ordinary, ownerless pickups */
	for (i = 0; i < Net_create_loc; i++) {
		int objnum = Net_create_objnums[i];
		if (objnum >= 0 && objnum <= Highest_object_index && Objects[objnum].type == OBJ_POWERUP &&
		    Objects[objnum].id != POW_ENERGY && Objects[objnum].id != POW_SHIELD_BOOST &&
		    Objects[objnum].id != POW_CLOAK && Objects[objnum].id != POW_INVULNERABILITY)
			Objects[objnum].flags |= OF_COOP_RECOVERY;
	}
	if (!multi_i_am_master()) return;
	for (size_t n = 0; n < count; n++)
		if (items[n].state == COOP_RECOVERY_DEPARTED && same_owner(&items[n], pnum)) {
			/* A quit after death must not produce a second set of eggs */
			for (i = 0; i < Net_create_loc; i++) {
				int objnum = Net_create_objnums[i];
				if (objnum >= 0 && objnum <= Highest_object_index) {
					Objects[objnum].flags |= OF_SHOULD_BE_DEAD;
					multi_send_remobj(objnum);
				}
			}
			return;
		}
	/* Reserve the complete batch before changing any ownership */
	if (!reserve(count + Net_create_loc + 2))
		Error("Cannot retain coop dropped inventory");
	coop_snapshot_player(pnum, &rec);
	pool = from_record(&rec);
	/* Only successfully created armed mines consumed inventory */
	for (i = 0; i < MAX_SECONDARY_WEAPONS; i++)
		pool.missiles[i] -= (uint16_t) take_amount(armed_mines[pnum][i], pool.missiles[i]);
	for (i = 0; i < Net_create_loc; i++) {
		int objnum = Net_create_objnums[i];
		coop_recovery_item *item;
		coop_recovery_gear gear;
		if (objnum < 0 || objnum > Highest_object_index || Objects[objnum].type != OBJ_POWERUP ||
		    (Objects[objnum].flags & OF_SHOULD_BE_DEAD))
			continue;
		gear = allocate_egg(&Objects[objnum], &pool);
		if (empty(&gear)) {
			if (Objects[objnum].flags & OF_COOP_RECOVERY) {
				Objects[objnum].flags |= OF_SHOULD_BE_DEAD;
				multi_send_remobj(objnum);
			}
			continue;
		}
		item = append(pnum, COOP_RECOVERY_LIVE);
		if (!item) Error("Cannot retain coop dropped inventory");
		item->gear = gear;
		item->object_index = (int16_t) objnum;
		item->signature = Objects[objnum].signature;
		item->powerup = Objects[objnum].id;
		item->remote_index = (int16_t) objnum_local_to_remote(objnum, &item->network_owner);
		send_item(item, REC_ITEM, -1);
	}
	if (!empty(&pool)) {
		coop_recovery_item *item = append(pnum, COOP_RECOVERY_CREDIT);
		if (!item) Error("Cannot retain coop undropped inventory");
		item->gear = pool;
		send_item(item, REC_ITEM, -1);
	}
	{
		coop_recovery_item *marker = append(pnum, COOP_RECOVERY_DEPARTED);
		if (!marker) Error("Cannot retain coop departure state");
		marker->life = player_life[pnum];
		send_item(marker, REC_ITEM, -1);
		coop_recovery_item status = { 0 };
		status.id = (uint32_t) pnum + 1;
		status.life = player_life[pnum];
		send_item(&status, REC_LIFE, -1);
	}
}

void coop_recovery_alive(int pnum)
{
	size_t i;
	if (!coop_recovery_active() || !multi_i_am_master()) return;
	for (i = 0; i < count; i++)
		if (items[i].state == COOP_RECOVERY_DEPARTED && same_owner(&items[i], pnum)) {
			/* A respawn inventory replaces the dead ship; late status from the
			 * old life is rejected separately */
			Players[pnum].primary_weapon_flags = 1;
			Players[pnum].secondary_weapon_flags = 1;
			Players[pnum].laser_level = 0;
			Players[pnum].flags &= ~equipment_flags();
			memset(Players[pnum].primary_ammo, 0, sizeof(Players[pnum].primary_ammo));
			memset(Players[pnum].secondary_ammo, 0, sizeof(Players[pnum].secondary_ammo));
			Players[pnum].secondary_ammo[CONCUSSION_INDEX] = 2 + NDL - Difficulty_level;
			coop_recovery_set_omega(pnum, 0);
			items[i].state = COOP_RECOVERY_ALIVE;
			items[i].revision++;
			send_item(&items[i], REC_ITEM, -1);
		}
}

void coop_recovery_departure_record(int pnum, coop_player_record *rec)
{
	size_t i;
	if (!coop_recovery_active()) return;
	for (i = 0; i < count; i++)
		if (items[i].state == COOP_RECOVERY_DEPARTED && same_owner(&items[i], pnum)) {
			rec->primary_weapon_flags = 1;
			rec->secondary_weapon_flags = 0;
			rec->laser_level = 0;
			rec->omega_charge = 0;
			rec->flags &= ~equipment_flags();
			memset(rec->primary_ammo, 0, sizeof(rec->primary_ammo));
			memset(rec->secondary_ammo, 0, sizeof(rec->secondary_ammo));
			return;
		}
}

/* The unapplied remainder stays in its ledger entry, including capacity overflow */
static coop_recovery_gear transfer(coop_player_record *rec, coop_recovery_gear *source)
{
	coop_recovery_gear taken = { 0 };
	int i;
	unsigned int multiplier = 1;
#ifdef DXX_BUILD_DESCENT_II
	if ((rec->flags | source->flags) & PLAYER_FLAGS_AMMO_RACK) multiplier = 2;
#endif
	taken.primary = source->primary & ~rec->primary_weapon_flags;
	taken.flags = source->flags & ~rec->flags;
	taken.laser = (uint16_t) take_amount(source->laser,
#ifdef DXX_BUILD_DESCENT_II
	                                     5u - take_amount(rec->laser_level, 5));
#else
	                                     3u - take_amount(rec->laser_level, 3));
#endif
	taken.vulcan = take_amount(source->vulcan, multiplier * Primary_ammo_max[VULCAN_INDEX] - take_amount(rec->primary_ammo[VULCAN_INDEX], multiplier * Primary_ammo_max[VULCAN_INDEX]));
	taken.omega = (int32_t) take_amount(source->omega > 0 ? source->omega : 0, F1_0 - take_amount(rec->omega_charge > 0 ? rec->omega_charge : 0, F1_0));
	rec->omega_charge += taken.omega;
	rec->primary_weapon_flags |= taken.primary;
	rec->flags |= taken.flags;
	rec->laser_level += (uint8_t) taken.laser;
	rec->primary_ammo[VULCAN_INDEX] += (uint16_t) taken.vulcan;
	source->primary &= ~taken.primary;
	source->flags &= ~taken.flags;
	source->laser -= taken.laser;
	source->vulcan -= taken.vulcan;
	source->omega -= taken.omega;
	for (i = 0; i < MAX_SECONDARY_WEAPONS; i++) {
		taken.missiles[i] = (uint16_t) take_amount(source->missiles[i], multiplier * Secondary_ammo_max[i] - take_amount(rec->secondary_ammo[i], multiplier * Secondary_ammo_max[i]));
		rec->secondary_ammo[i] += taken.missiles[i];
		source->missiles[i] -= taken.missiles[i];
		if (rec->secondary_ammo[i]) rec->secondary_weapon_flags |= 1u << i;
	}
	return taken;
}

static coop_recovery_item *find_object(const object *obj)
{
	for (size_t i = 0; i < count; i++)
		if (bound_object(&items[i]) == obj - Objects) return &items[i];
	return NULL;
}

/* Contents can only decrease within a world generation. This makes duplicate
 * and reordered collection reports harmless without inventory replay */
static void intersect_gear(coop_recovery_gear *a, const coop_recovery_gear *b)
{
	a->primary &= b->primary;
	a->flags &= b->flags;
	a->laser = (uint16_t) take_amount(a->laser, b->laser);
	a->vulcan = take_amount(a->vulcan, b->vulcan);
	a->omega = (int32_t) take_amount(a->omega, b->omega);
	for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++)
		a->missiles[i] = (uint16_t) take_amount(a->missiles[i], b->missiles[i]);
}

int coop_recovery_pickup_blocked(const object *powerup)
{
	if (!coop_recovery_active() || !(powerup->flags & OF_COOP_RECOVERY)) return 0;
	const coop_recovery_item *item = find_object(powerup);
	/* Wait for provenance before allowing a newly synchronized dropped object */
	return !item || item->state != COOP_RECOVERY_LIVE;
}

int coop_recovery_pickup_count(const object *powerup, int secondary, int amount)
{
	if (!coop_recovery_active() || !(powerup->flags & OF_COOP_RECOVERY)) return amount;
	const coop_recovery_item *item = find_object(powerup);
	if (!item) return 0;
	return (int) take_amount(amount, secondary >= 0 ? item->gear.missiles[secondary] : item->gear.vulcan);
}

void coop_recovery_note_pickup(object *powerup, const coop_player_record *before, int used)
{
	if (!coop_recovery_active() || !(powerup->flags & OF_COOP_RECOVERY)) return;
	coop_recovery_item *item = find_object(powerup);
	if (!item || item->state != COOP_RECOVERY_LIVE) return;
	coop_player_record after;
	coop_snapshot_player(Player_num, &after);
	coop_recovery_gear remaining = item->gear;
	remaining.primary &= ~(after.primary_weapon_flags & ~before->primary_weapon_flags);
	remaining.flags &= ~(after.flags & ~before->flags);
	remaining.laser -= (uint16_t) take_amount(after.laser_level > before->laser_level ? after.laser_level - before->laser_level : 0, remaining.laser);
	remaining.vulcan -= take_amount(after.primary_ammo[VULCAN_INDEX] > before->primary_ammo[VULCAN_INDEX] ? after.primary_ammo[VULCAN_INDEX] - before->primary_ammo[VULCAN_INDEX] : 0, remaining.vulcan);
	remaining.omega -= (int32_t) take_amount(after.omega_charge > before->omega_charge ? after.omega_charge - before->omega_charge : 0, remaining.omega);
	for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++)
		remaining.missiles[i] -= (uint16_t) take_amount(after.secondary_ammo[i] > before->secondary_ammo[i] ? after.secondary_ammo[i] - before->secondary_ammo[i] : 0, remaining.missiles[i]);
	if (used) {
		/* A normal duplicate-weapon conversion consumes the weapon too.
		 * Ammo that would overflow the ship remains recoverable as credit */
		remaining.primary = remaining.flags = remaining.laser = 0;
	}
	if (!used && !memcmp(&remaining, &item->gear, sizeof(remaining))) return;
	item->gear = remaining;
	if (used || empty(&remaining)) item->state = empty(&remaining) ? COOP_RECOVERY_TAKEN : COOP_RECOVERY_CREDIT;
	send_item(item, REC_COLLECT, -1);
	COOPLOG("spew collected id=%u used=%d remaining=%d", item->id, used, !empty(&remaining));
}

/* A normal removal is consumption, never expiry. Keep the old contents only
 * until the matching collection report supplies any capacity remainder */
void coop_recovery_note_remove(object *powerup)
{
	if (!coop_recovery_active() || !(powerup->flags & OF_COOP_RECOVERY)) return;
	coop_recovery_item *item = find_object(powerup);
	if (item && item->state == COOP_RECOVERY_LIVE) {
		item->state = COOP_RECOVERY_TAKEN;
		item->object_index = -1;
	}
}

/* Before sending a rejoin's world snapshot, freeze its recoverable drops.
 * Each active peer replies with its remaining contents, including a local
 * pickup whose ordinary remove packet has not reached the host yet */
int coop_recovery_rejoin_ready(const char *callsign, const char *client_id)
{
	int ready = 1;
	if (!coop_recovery_active() || !multi_i_am_master()) return 1;
	int resend = timer_query() >= freeze_sent_at;
	if (resend) freeze_sent_at = timer_query() + F1_0;
	for (size_t i = 0; i < count; i++) {
		coop_recovery_item *item = &items[i];
		if (client_id[0] && item->client_id[0] ? strcmp(client_id, item->client_id) : d_stricmp(callsign, item->callsign)) continue;
		if (item->state == COOP_RECOVERY_LIVE || item->state == COOP_RECOVERY_CREDIT) {
			item->state = COOP_RECOVERY_RECLAIMING;
			item->revision++;
			freeze_waiting[i] = 0;
			for (int peer = 0; peer < MAX_PLAYERS; peer++)
				if (peer != Player_num && Players[peer].connected == CONNECT_PLAYING)
					freeze_waiting[i] |= 1u << peer;
			send_item(item, REC_FREEZE, -1);
		} else if (item->state != COOP_RECOVERY_RECLAIMING) continue;
		if (freeze_waiting[i]) {
			ready = 0;
			if (resend) send_item(item, REC_FREEZE, -1);
		}
	}
	return ready;
}

void coop_recovery_expire(object *powerup)
{
	size_t i;
	if (!coop_recovery_active() || !multi_i_am_master()) return;
	for (i = 0; i < count; i++)
		if (items[i].state == COOP_RECOVERY_LIVE && bound_object(&items[i]) == powerup - Objects) {
			items[i].state = COOP_RECOVERY_CREDIT;
			items[i].revision++;
			remove_world(&items[i]);
			send_item(&items[i], REC_ITEM, -1);
		}
}

void coop_recovery_level_leave(void)
{
	size_t i;
	if (!coop_recovery_active()) return;
	for (i = 0; i < count; i++)
		if (items[i].state == COOP_RECOVERY_LIVE) {
			items[i].state = COOP_RECOVERY_CREDIT;
			items[i].object_index = -1;
			items[i].revision++;
		}
}

int coop_recovery_save_ready(void)
{
	for (size_t i = 0; i < count; i++)
		if (items[i].state == COOP_RECOVERY_RECLAIMING && freeze_waiting[i]) return 0;
	return 1;
}

int coop_recovery_prepare_rejoin(int pnum, coop_player_record *rec)
{
	size_t i;
	int recovered = 0;
	if (rec->shields <= 0) {
		rec->shields = i2f(100);
		rec->energy = i2f(100);
	}
	if (!coop_recovery_active() || !multi_i_am_master()) return 0;
	/* A delayed death packet may have arrived after the disconnect snapshot */
	coop_recovery_departure_record(pnum, rec);
	/* Fence packets from the departed connection, even if no drop arrived */
	player_life[pnum]++;
	for (i = 0; i < count; i++) {
		coop_recovery_gear taken;
		if ((items[i].state != COOP_RECOVERY_LIVE && items[i].state != COOP_RECOVERY_CREDIT && items[i].state != COOP_RECOVERY_RECLAIMING) ||
		    !same_owner(&items[i], pnum)) continue;
		if (items[i].state == COOP_RECOVERY_RECLAIMING && freeze_waiting[i]) continue;
		taken = transfer(rec, &items[i].gear);
		if (empty(&taken) && items[i].state == COOP_RECOVERY_CREDIT) continue;
		if (!empty(&taken)) recovered++;
		remove_world(&items[i]);
		items[i].state = empty(&items[i].gear) ? COOP_RECOVERY_TAKEN : COOP_RECOVERY_CREDIT;
		items[i].revision++;
		send_item(&items[i], REC_ITEM, -1);
	}
	if (!next_id) Error("Coop inventory revision exhausted");
	restore_serial[pnum] = next_id++;
	if (pnum == Player_num) applied_restore_serial[pnum] = restore_serial[pnum];
	{
		coop_recovery_item status = { 0 };
		status.id = (uint32_t) pnum + 1;
		status.life = player_life[pnum];
		send_item(&status, REC_LIFE, -1);
	}
	coop_recovery_alive(pnum);
	return recovered;
}

uint32_t coop_recovery_restore_serial(int pnum)
{
	return pnum == Player_num ? applied_restore_serial[Player_num] : (pnum >= 0 && pnum < MAX_PLAYERS ? restore_serial[pnum] : 0);
}

int coop_recovery_accept_ship_status(int pnum, uint32_t revision, uint32_t life)
{
	if (!coop_recovery_active()) return 1;
	if (pnum < 0 || pnum >= MAX_PLAYERS || life != player_life[pnum]) return 0;
	if (multi_i_am_master()) return revision == restore_serial[pnum];
	return revision >= applied_restore_serial[pnum];
}

uint32_t coop_recovery_life(int pnum)
{
	return pnum >= 0 && pnum < MAX_PLAYERS ? player_life[pnum] : 0;
}

void coop_recovery_set_restore_serial(int pnum, uint32_t revision)
{
	if (pnum >= 0 && pnum < MAX_PLAYERS) {
		restore_serial[pnum] = revision;
		applied_restore_serial[pnum] = revision;
		if (next_id <= revision) next_id = revision + 1;
	}
}

uint32_t coop_recovery_epoch(void)
{
	return epoch;
}
int coop_recovery_accept_restore(uint32_t generation, uint32_t revision)
{
	if (!revision || (generation == epoch && revision <= applied_restore_serial[Player_num])) return 0;
	epoch = generation;
	return 1;
}

void coop_recovery_receive(const ubyte *buf, int sender)
{
	coop_recovery_item incoming;
	uint32_t incoming_epoch = (uint32_t) GET_INTEL_INT(buf + 4);
	if (!coop_recovery_active() || sender < 0 || sender >= MAX_PLAYERS) return;
	memcpy(&incoming, buf + 16, sizeof(incoming));
	int authority = sender == multi_who_is_master();
	if (!count && authority) epoch = incoming_epoch;
	if (incoming_epoch != epoch) return;
	if (buf[1] == REC_BEGIN || buf[1] == REC_END) return;
	if (buf[1] == REC_LIFE) {
		if (authority && incoming.id && incoming.id <= MAX_PLAYERS && incoming.life > player_life[incoming.id - 1])
			player_life[incoming.id - 1] = incoming.life;
		return;
	}
	if (!incoming.id || incoming.state < COOP_RECOVERY_LIVE || incoming.state > COOP_RECOVERY_RECLAIMING ||
	    !memchr(incoming.client_id, 0, sizeof(incoming.client_id)) || !memchr(incoming.callsign, 0, sizeof(incoming.callsign))) return;
	size_t index;
	for (index = 0; index < count && items[index].id != incoming.id; index++) {}
	if (buf[1] == REC_COLLECT || buf[1] == REC_FROZEN) {
		if (index == count) return;
		coop_recovery_item *item = &items[index];
		if (buf[1] == REC_FROZEN) {
			if (!multi_i_am_master() || item->state != COOP_RECOVERY_RECLAIMING || incoming.revision != item->revision) return;
			intersect_gear(&item->gear, &incoming.gear);
			freeze_waiting[index] &= ~(1u << sender);
		} else {
			if (incoming.revision != item->revision || item->state == COOP_RECOVERY_RECLAIMING) return;
			intersect_gear(&item->gear, &incoming.gear);
			if (empty(&item->gear) || incoming.state == COOP_RECOVERY_TAKEN) item->state = COOP_RECOVERY_TAKEN;
			else if (incoming.state == COOP_RECOVERY_CREDIT) item->state = COOP_RECOVERY_CREDIT;
			if (item->state != COOP_RECOVERY_LIVE) remove_world(item);
		}
		return;
	}
	if (!authority || multi_i_am_master() || (buf[1] != REC_ITEM && buf[1] != REC_FREEZE)) return;
	if (index < count && items[index].revision > incoming.revision) return;
	int prior_object = index < count ? bound_object(&items[index]) : -1;
	if (index < count) {
		intersect_gear(&incoming.gear, &items[index].gear);
		if (incoming.state == COOP_RECOVERY_LIVE &&
		    (items[index].state == COOP_RECOVERY_CREDIT || items[index].state == COOP_RECOVERY_TAKEN)) incoming.state = items[index].state;
	}
	if (index == count) {
		if (!reserve(count + 1)) Error("Cannot receive coop drop tags");
		count++;
		freeze_waiting[index] = 0;
	}
	incoming.object_index = -1;
	if (incoming.remote_index >= 0) {
		int objnum = prior_object >= 0 ? prior_object : objnum_remote_to_local(incoming.remote_index, incoming.network_owner);
		if (objnum >= 0 && objnum <= Highest_object_index && Objects[objnum].type == OBJ_POWERUP &&
		    Objects[objnum].id == incoming.powerup && (Objects[objnum].flags & OF_COOP_RECOVERY)) {
			incoming.object_index = (int16_t) objnum;
			incoming.signature = Objects[objnum].signature;
		}
	}
	if (buf[1] == REC_FREEZE) incoming.state = COOP_RECOVERY_RECLAIMING;
	items[index] = incoming;
	if (incoming.state != COOP_RECOVERY_LIVE && incoming.state != COOP_RECOVERY_RECLAIMING) remove_world(&items[index]);
	if (next_id <= incoming.id) next_id = incoming.id + 1;
	if (buf[1] == REC_FREEZE) send_item(&items[index], REC_FROZEN, sender);
}

void coop_recovery_frame(void)
{
	if (!coop_recovery_active()) return;
	for (size_t i = 0; i < count; i++) {
		coop_recovery_item *item = &items[i];
		if (item->remote_index < 0 || item->object_index >= 0) continue;
		int newer = 0;
		for (size_t n = 0; n < count; n++)
			if (items[n].id > item->id && items[n].remote_index == item->remote_index && items[n].network_owner == item->network_owner) newer = 1;
		if (newer) continue;
		int objnum = objnum_remote_to_local(item->remote_index, item->network_owner);
		if (objnum >= 0 && objnum <= Highest_object_index && Objects[objnum].type == OBJ_POWERUP &&
		    Objects[objnum].id == item->powerup && (Objects[objnum].flags & OF_COOP_RECOVERY)) {
			item->object_index = (int16_t) objnum;
			item->signature = Objects[objnum].signature;
			if (item->state != COOP_RECOVERY_LIVE && item->state != COOP_RECOVERY_RECLAIMING) {
				remove_world(item);
				item->remote_index = -1;
			}
		}
	}
}

void coop_recovery_send_snapshot(int pnum)
{
	if (!coop_recovery_active() || !multi_i_am_master()) return;
	send_item(NULL, REC_BEGIN, pnum);
	for (size_t i = 0; i < count; i++) send_item(&items[i], REC_ITEM, pnum);
	for (int i = 0; i < MAX_PLAYERS; i++) {
		coop_recovery_item status = { 0 };
		status.id = (uint32_t) i + 1;
		status.life = player_life[i];
		send_item(&status, REC_LIFE, pnum);
	}
	send_item(NULL, REC_END, pnum);
}

void coop_recovery_host_changed(void)
{
	if (!coop_recovery_active() || !multi_i_am_master()) return;
	coop_recovery_frame();
	/* Host migration makes surviving objects locally owned for future joins */
	for (size_t i = 0; i < count; i++) {
		int objnum = bound_object(&items[i]);
		/* A new host must obtain its own acknowledgements before reclamation */
		if (items[i].state == COOP_RECOVERY_RECLAIMING) {
			items[i].state = objnum >= 0 ? COOP_RECOVERY_LIVE : COOP_RECOVERY_CREDIT;
			freeze_waiting[i] = 0;
		}
		if (items[i].state != COOP_RECOVERY_LIVE || objnum < 0) continue;
		items[i].remote_index = (int16_t) objnum_local_to_remote(objnum, &items[i].network_owner);
		items[i].revision++;
		send_item(&items[i], REC_ITEM, -1);
	}
}

/* Save transfer supplies one host-selected timeline, regardless of how many
 * local new-game/reset calls each peer made before loading the save */
void coop_recovery_begin_restore(uint32_t generation)
{
	restore_epoch = generation;
}

void coop_recovery_end_restore(void)
{
	COOPLOG("recovery restore epoch=%u rows=%u", epoch, (unsigned) count);
	restore_epoch = 0;
}

void coop_recovery_reset(void)
{
	count = 0;
	next_id = 1;
	epoch = restore_epoch ? restore_epoch : epoch + 1;
	if (!epoch) epoch = 1;
	freeze_sent_at = 0;
	memset(restore_serial, 0, sizeof(restore_serial));
	memset(applied_restore_serial, 0, sizeof(applied_restore_serial));
	memset(omega_charge, 0, sizeof(omega_charge));
	memset(player_life, 0, sizeof(player_life));
	memset(armed_mines, 0, sizeof(armed_mines));
}

size_t coop_recovery_count(void)
{
	return count;
}
const coop_recovery_item *coop_recovery_data(void)
{
	return items;
}

int coop_recovery_set_pending(const coop_recovery_item *data, size_t size)
{
	coop_recovery_item *copy = NULL;
	if (size > SIZE_MAX / sizeof(*copy) || (size && !data)) return 0;
	if (size) {
		copy = (coop_recovery_item *) malloc(size * sizeof(*copy));
		if (!copy) return 0;
		memcpy(copy, data, size * sizeof(*copy));
	}
	free(pending);
	pending = copy;
	pending_count = size;
	return 1;
}

int coop_recovery_apply_pending(void)
{
	size_t i;
	if (!reserve(pending_count)) return 0;
	for (i = 0; i < pending_count; i++) {
		coop_recovery_item *item = &pending[i];
		int j, found = -1;
		if (!item->id || item->id == UINT32_MAX || !memchr(item->client_id, 0, sizeof(item->client_id)) ||
		    !memchr(item->callsign, 0, sizeof(item->callsign)) ||
		    item->state < COOP_RECOVERY_LIVE || item->state > COOP_RECOVERY_RECLAIMING) return 0;
		for (size_t n = 0; n < i; n++)
			if (pending[n].id == item->id) return 0;
		if (item->state == COOP_RECOVERY_LIVE || (item->state == COOP_RECOVERY_RECLAIMING && item->remote_index >= 0)) {
			for (j = 0; j <= Highest_object_index; j++)
				if (Objects[j].signature == item->signature && Objects[j].type == OBJ_POWERUP && Objects[j].id == item->powerup) {
					if (found != -1) return 0;
					found = j;
				}
			if (found < 0) return 0;
			item->object_index = (int16_t) found;
			item->remote_index = (int16_t) objnum_local_to_remote(found, &item->network_owner);
		} else {
			item->object_index = item->remote_index = -1;
		}
	}
	coop_recovery_reset();
	for (i = 0; i < pending_count; i++) {
		items[count++] = pending[i];
		/* A loaded save is a new authoritative world; restart any pending freeze */
		if (items[i].state == COOP_RECOVERY_RECLAIMING) items[i].state = items[i].object_index >= 0 ? COOP_RECOVERY_LIVE : COOP_RECOVERY_CREDIT;
		freeze_waiting[i] = 0;
		if (next_id <= items[i].id) next_id = items[i].id + 1;
	}
	free(pending);
	pending = NULL;
	pending_count = 0;
	return 1;
}

fix coop_recovery_omega(int pnum)
{
#ifdef DXX_BUILD_DESCENT_II
	if (pnum == Player_num) return Omega_charge;
#endif
	return pnum >= 0 && pnum < MAX_PLAYERS ? omega_charge[pnum] : 0;
}

void coop_recovery_set_omega(int pnum, fix charge)
{
	if (pnum < 0 || pnum >= MAX_PLAYERS) return;
	omega_charge[pnum] = charge;
#ifdef DXX_BUILD_DESCENT_II
	if (pnum == Player_num) Omega_charge = charge;
#endif
}

void coop_recovery_remember_record(const coop_player_record *rec)
{
	coop_recovery_gear gear = from_record(rec);
	if (!coop_recovery_active() || !multi_i_am_master() || empty(&gear)) return;
	coop_recovery_item *item = append(Player_num, COOP_RECOVERY_CREDIT);
	if (!item) Error("Cannot retain coop absent inventory");
	memcpy(item->callsign, rec->callsign, sizeof(item->callsign));
	memcpy(item->client_id, rec->client_id, sizeof(item->client_id));
	item->gear = gear;
	send_item(item, REC_ITEM, -1);
}

void coop_recovery_set_life(int pnum, uint32_t life)
{
	if (pnum >= 0 && pnum < MAX_PLAYERS) player_life[pnum] = life;
}

void coop_recovery_begin_drop(int pnum)
{
	if (pnum >= 0 && pnum < MAX_PLAYERS) memset(armed_mines[pnum], 0, sizeof(armed_mines[pnum]));
}

void coop_recovery_consume_mine(int pnum, int secondary, int objnum)
{
	if (coop_recovery_active() && objnum >= 0 && pnum >= 0 && pnum < MAX_PLAYERS &&
	    secondary >= 0 && secondary < MAX_SECONDARY_WEAPONS)
		armed_mines[pnum][secondary]++;
}
