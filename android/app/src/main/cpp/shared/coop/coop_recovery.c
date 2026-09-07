#include <stdlib.h>
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
	   REC_GRANT,
	   REC_REQUEST,
	   REC_BEGIN,
	   REC_END,
	   REC_REVISION,
	   REC_SNAPSHOT_REVISION };
static coop_recovery_item *items, *pending;
static size_t count, capacity, pending_count;
static uint32_t next_id = 1, epoch = 1;
static uint32_t player_revision[MAX_PLAYERS];
static uint32_t applied_revision[MAX_PLAYERS];
static uint32_t player_life[MAX_PLAYERS];
static fix omega_charge[MAX_PLAYERS];
static uint16_t armed_mines[MAX_PLAYERS][COOP_SAVE_MAX_WEAPONS];
static uint32_t requested_id;
static fix64 requested_at;

static unsigned char *apply_grant;

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
		memcpy(flags, apply_grant, count);
	}
	free(items);
	free(apply_grant);
	items = grown;
	apply_grant = flags;
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
	item->recipient = 255;
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

void coop_recovery_drop(int pnum, uint32_t reported_revision, uint32_t life)
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
	/* A grant absent from the death packet never entered the generated eggs
	 * Retain it separately, including copies or ammo beyond ship capacity */
	size_t receipt_count = count;
	for (size_t n = 0; n < receipt_count; n++)
		if (items[n].recipient == pnum && items[n].id > reported_revision &&
		    items[n].state == COOP_RECOVERY_TAKEN) {
			coop_recovery_gear delta = items[n].gear;
			items[n].recipient = 255;
			items[n].revision++;
			send_item(&items[n], REC_ITEM, -1);
			coop_recovery_item *credit = append(pnum, COOP_RECOVERY_CREDIT);
			if (!credit) Error("Cannot retain coop undelivered grant");
			credit->gear = delta;
			send_item(credit, REC_ITEM, -1);
		}
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
		coop_recovery_item revision = { 0 };
		revision.recipient = (uint8_t) pnum;
		revision.id = player_revision[pnum];
		revision.life = player_life[pnum];
		send_item(&revision, REC_REVISION, -1);
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

static void apply_delta(int pnum, coop_recovery_gear delta)
{
	coop_player_record rec;
	coop_snapshot_player(pnum, &rec);
	rec.primary_weapon_flags |= delta.primary;
	rec.flags |= delta.flags;
	rec.laser_level = (uint8_t) take_amount(rec.laser_level + delta.laser,
#ifdef DXX_BUILD_DESCENT_II
	                                        5);
#else
	                                        3);
#endif
	rec.primary_ammo[VULCAN_INDEX] += (uint16_t) delta.vulcan;
	for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++) {
		rec.secondary_ammo[i] += delta.missiles[i];
		if (rec.secondary_ammo[i]) rec.secondary_weapon_flags |= 1u << i;
	}
	rec.omega_charge = (fix) take_amount(rec.omega_charge + delta.omega, F1_0);
	/* Inventory only: do not roll back score/resources or replay pickup rewards */
	Players[pnum].primary_weapon_flags = rec.primary_weapon_flags;
	Players[pnum].secondary_weapon_flags = rec.secondary_weapon_flags;
	Players[pnum].laser_level = rec.laser_level;
	Players[pnum].flags = rec.flags;
	Players[pnum].primary_ammo[VULCAN_INDEX] = rec.primary_ammo[VULCAN_INDEX];
	memcpy(Players[pnum].secondary_ammo, rec.secondary_ammo, sizeof(Players[pnum].secondary_ammo));
	coop_recovery_set_omega(pnum, rec.omega_charge);
}

static void grant_pickup(size_t index, int pnum, uint32_t life)
{
	coop_player_record rec;
	coop_recovery_gear remaining, taken;
	coop_recovery_item *grant;
	int objnum = bound_object(&items[index]);
	int ship = Players[pnum].objnum;
	if (items[index].state != COOP_RECOVERY_LIVE || objnum < 0 ||
	    ship < 0 || ship > Highest_object_index || Objects[ship].type != OBJ_PLAYER ||
	    Objects[ship].shields < 0 || (Objects[objnum].flags & OF_SHOULD_BE_DEAD)) return;
	if (vm_vec_dist_quick(&Objects[ship].pos, &Objects[objnum].pos) >
	    Objects[ship].size + Objects[objnum].size + i2f(20)) return;
	coop_snapshot_player(pnum, &rec);
	remaining = items[index].gear;
	taken = transfer(&rec, &remaining);
	if (empty(&taken)) return;
	if (!reserve(count + 1)) return;
	/* Retire or reduce the world claim before any recipient can receive it */
	items[index].gear = remaining;
	items[index].revision++;
	if (empty(&remaining)) {
		remove_world(&items[index]);
		items[index].state = COOP_RECOVERY_TAKEN;
	}
	send_item(&items[index], REC_ITEM, -1);
	grant = append(pnum, COOP_RECOVERY_TAKEN);
	if (!grant) Error("Cannot retain coop pickup receipt");
	grant->gear = taken;
	grant->recipient = (uint8_t) pnum;
	grant->previous_revision = player_revision[pnum];
	grant->life = life;
	player_revision[pnum] = grant->id;
	apply_delta(pnum, taken);
	applied_revision[pnum] = grant->id;
	send_item(grant, REC_GRANT, -1);
}

int coop_recovery_pickup(object *powerup)
{
	size_t i;
	int objnum = (int) (powerup - Objects);
	if (!coop_recovery_active() || !(powerup->flags & OF_COOP_RECOVERY)) return 0;
	for (i = 0; i < count; i++) {
		if (items[i].state != COOP_RECOVERY_LIVE || bound_object(&items[i]) != objnum) continue;
		if (multi_i_am_master()) grant_pickup(i, Player_num, player_life[Player_num]);
		else if (requested_id != items[i].id || timer_query() > requested_at + F1_0) {
			requested_id = items[i].id;
			requested_at = timer_query();
			coop_recovery_item request = items[i];
			request.life = player_life[Player_num];
			send_item(&request, REC_REQUEST, multi_who_is_master());
		}
		return 1;
	}
	/* Bonus shield/energy and timed powers retain the ordinary pickup path */
	return powerup->id != POW_ENERGY && powerup->id != POW_SHIELD_BOOST &&
	       powerup->id != POW_CLOAK && powerup->id != POW_INVULNERABILITY;
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
		if ((items[i].state != COOP_RECOVERY_LIVE && items[i].state != COOP_RECOVERY_CREDIT) ||
		    !same_owner(&items[i], pnum)) continue;
		taken = transfer(rec, &items[i].gear);
		if (empty(&taken) && items[i].state == COOP_RECOVERY_CREDIT) continue;
		if (!empty(&taken)) recovered++;
		remove_world(&items[i]);
		items[i].state = empty(&items[i].gear) ? COOP_RECOVERY_TAKEN : COOP_RECOVERY_CREDIT;
		items[i].revision++;
		send_item(&items[i], REC_ITEM, -1);
	}
	if (!next_id) Error("Coop inventory revision exhausted");
	player_revision[pnum] = next_id++;
	if (pnum == Player_num) applied_revision[pnum] = player_revision[pnum];
	{
		coop_recovery_item revision = { 0 };
		revision.id = player_revision[pnum];
		revision.life = player_life[pnum];
		revision.previous_revision = 1;
		revision.recipient = (uint8_t) pnum;
		send_item(&revision, REC_REVISION, -1);
	}
	coop_recovery_alive(pnum);
	return recovered;
}

uint32_t coop_recovery_player_revision(int pnum)
{
	return pnum == Player_num ? applied_revision[Player_num] : (pnum >= 0 && pnum < MAX_PLAYERS ? player_revision[pnum] : 0);
}

int coop_recovery_accept_ship_status(int pnum, uint32_t revision, uint32_t life)
{
	if (!coop_recovery_active()) return 1;
	if (pnum < 0 || pnum >= MAX_PLAYERS || life != player_life[pnum]) return 0;
	if (multi_i_am_master()) return revision == player_revision[pnum];
	return revision >= applied_revision[pnum];
}

uint32_t coop_recovery_life(int pnum)
{
	return pnum >= 0 && pnum < MAX_PLAYERS ? player_life[pnum] : 0;
}

void coop_recovery_set_player_revision(int pnum, uint32_t revision)
{
	if (pnum >= 0 && pnum < MAX_PLAYERS) {
		player_revision[pnum] = revision;
		applied_revision[pnum] = revision;
		if (next_id <= revision) next_id = revision + 1;
	}
}

uint32_t coop_recovery_epoch(void)
{
	return epoch;
}
int coop_recovery_accept_restore(uint32_t generation, uint32_t revision)
{
	if (!revision || (generation == epoch && revision <= applied_revision[Player_num])) return 0;
	epoch = generation;
	return 1;
}

void coop_recovery_receive(const ubyte *buf, int sender)
{
	coop_recovery_item incoming;
	size_t index;
	int prior_object = -1;
	uint32_t incoming_epoch = (uint32_t) GET_INTEL_INT(buf + 4);
	if (!coop_recovery_active() || sender < 0 || sender >= MAX_PLAYERS) return;
	memcpy(&incoming, buf + 16, sizeof(incoming));
	if (buf[1] == REC_REQUEST) {
		if (!multi_i_am_master() || incoming_epoch != epoch || Players[sender].connected != CONNECT_PLAYING || incoming.life != player_life[sender]) return;
		for (index = 0; index < count; index++)
			if (items[index].id == incoming.id && items[index].revision == incoming.revision) {
				grant_pickup(index, sender, incoming.life);
				break;
			}
		return;
	}
	if (sender != multi_who_is_master() || multi_i_am_master()) return;
	if (buf[1] == REC_BEGIN) {
		/* Snapshot rows merge by ID/revision: reliable UDP may reorder them */
		if (!count) epoch = incoming_epoch;
		return;
	}
	if (!count) epoch = incoming_epoch;
	if (incoming_epoch != epoch) return;
	if (buf[1] == REC_END) return;
	if (buf[1] == REC_SNAPSHOT_REVISION) {
		int pnum = incoming.recipient;
		if (pnum < MAX_PLAYERS && incoming.id >= applied_revision[pnum]) {
			player_life[pnum] = incoming.life;
			if (pnum != Player_num) {
				Players[pnum].primary_weapon_flags = 1 | incoming.gear.primary;
				Players[pnum].secondary_weapon_flags = 0;
				Players[pnum].laser_level = (ubyte) incoming.gear.laser;
				Players[pnum].flags = (Players[pnum].flags & ~equipment_flags()) | incoming.gear.flags;
				Players[pnum].primary_ammo[VULCAN_INDEX] = (uint16_t) incoming.gear.vulcan;
				for (int i = 0; i < MAX_SECONDARY_WEAPONS; i++) {
					Players[pnum].secondary_ammo[i] = incoming.gear.missiles[i];
					if (incoming.gear.missiles[i]) Players[pnum].secondary_weapon_flags |= 1u << i;
				}
				coop_recovery_set_omega(pnum, incoming.gear.omega);
				coop_recovery_set_player_revision(pnum, incoming.id);
			}
		}
		return;
	}
	if (buf[1] == REC_REVISION) {
		if (incoming.recipient < MAX_PLAYERS) {
			if (incoming.previous_revision == 1 && incoming.id >= player_revision[incoming.recipient])
				player_life[incoming.recipient] = incoming.life;
			if (!incoming.previous_revision && incoming.recipient == Player_num && incoming.life == player_life[Player_num] && incoming.id > applied_revision[Player_num])
				applied_revision[Player_num] = incoming.id;
			if (player_revision[incoming.recipient] < incoming.id)
				player_revision[incoming.recipient] = incoming.id;
			if (next_id <= incoming.id) next_id = incoming.id + 1;
		}
		return;
	}
	if ((buf[1] != REC_ITEM && buf[1] != REC_GRANT) || !incoming.id ||
	    incoming.state < COOP_RECOVERY_LIVE || incoming.state > COOP_RECOVERY_ALIVE ||
	    !memchr(incoming.client_id, 0, sizeof(incoming.client_id)) ||
	    !memchr(incoming.callsign, 0, sizeof(incoming.callsign))) return;
	for (index = 0; index < count; index++)
		if (items[index].id == incoming.id) break;
	if (index < count && (items[index].revision > incoming.revision ||
	                      (items[index].revision == incoming.revision && buf[1] != REC_GRANT))) return;
	if (index < count) prior_object = bound_object(&items[index]);
	if (index == count) {
		if (!reserve(count + 1)) Error("Cannot receive coop recovery state");
		count++;
		apply_grant[index] = 0;
	} else if (items[index].state == COOP_RECOVERY_LIVE && incoming.state != COOP_RECOVERY_LIVE)
		remove_world(&items[index]);
	incoming.object_index = -1;
	if (incoming.remote_index >= 0) {
		int objnum = prior_object >= 0 ? prior_object : objnum_remote_to_local(incoming.remote_index, incoming.network_owner);
		if (objnum >= 0 && objnum <= Highest_object_index && Objects[objnum].type == OBJ_POWERUP && Objects[objnum].id == incoming.powerup && (Objects[objnum].flags & OF_COOP_RECOVERY)) {
			incoming.object_index = (int16_t) objnum;
			incoming.signature = Objects[objnum].signature;
		}
	}
	items[index] = incoming;
	if (incoming.state != COOP_RECOVERY_LIVE && incoming.object_index >= 0) {
		remove_world(&items[index]);
		items[index].remote_index = -1;
	}
	if (next_id <= incoming.id) next_id = incoming.id + 1;
	if (buf[1] == REC_GRANT && incoming.recipient < MAX_PLAYERS) {
		if (player_revision[incoming.recipient] < incoming.id)
			player_revision[incoming.recipient] = incoming.id;
		apply_grant[index] = 1;
	}
}

void coop_recovery_frame(void)
{
	size_t i;
	int again;
	if (!coop_recovery_active()) return;
	for (i = 0; i < count; i++) {
		coop_recovery_item *item = &items[i];
		if (item->remote_index >= 0 && item->object_index < 0) {
			int newer = 0;
			for (size_t n = 0; n < count; n++)
				if (items[n].id > item->id && items[n].remote_index == item->remote_index &&
				    items[n].network_owner == item->network_owner) newer = 1;
			if (newer) continue;
			int objnum = objnum_remote_to_local(item->remote_index, item->network_owner);
			if (objnum >= 0 && objnum <= Highest_object_index && Objects[objnum].type == OBJ_POWERUP &&
			    Objects[objnum].id == item->powerup && (Objects[objnum].flags & OF_COOP_RECOVERY)) {
				item->object_index = (int16_t) objnum;
				item->signature = Objects[objnum].signature;
				if (item->state != COOP_RECOVERY_LIVE) {
					remove_world(item);
					item->remote_index = -1;
				}
			}
		}
	}
	do {
		again = 0;
		for (i = 0; i < count; i++)
			if (apply_grant[i]) {
				int pnum = items[i].recipient;
				if (pnum >= MAX_PLAYERS || items[i].id <= applied_revision[pnum] ||
				    (pnum == Player_num && items[i].life != player_life[Player_num])) {
					apply_grant[i] = 0;
					continue;
				}
				if (items[i].previous_revision != applied_revision[pnum]) continue;
				apply_grant[i] = 0;
				apply_delta(pnum, items[i].gear);
				applied_revision[pnum] = items[i].id;
				again = 1;
				if (pnum == Player_num) multi_send_ship_status();
			}
	} while (again);
}

void coop_recovery_send_snapshot(int pnum)
{
	size_t i;
	if (!coop_recovery_active() || !multi_i_am_master()) return;
	send_item(NULL, REC_BEGIN, pnum);
	for (i = 0; i < count; i++) send_item(&items[i], REC_ITEM, pnum);
	for (i = 0; i < MAX_PLAYERS; i++) {
		coop_recovery_item revision = { 0 };
		revision.id = player_revision[i];
		revision.recipient = (uint8_t) i;
		revision.life = player_life[i];
		coop_player_record rec;
		coop_snapshot_player((int) i, &rec);
		revision.gear = from_record(&rec);
		send_item(&revision, REC_SNAPSHOT_REVISION, pnum);
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
		if (items[i].state != COOP_RECOVERY_LIVE || objnum < 0) continue;
		items[i].remote_index = (int16_t) objnum_local_to_remote(objnum, &items[i].network_owner);
		items[i].revision++;
		send_item(&items[i], REC_ITEM, -1);
	}
}

void coop_recovery_reset(void)
{
	count = 0;
	next_id = 1;
	epoch++;
	requested_id = 0;
	memset(player_revision, 0, sizeof(player_revision));
	memset(applied_revision, 0, sizeof(applied_revision));
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
		    item->state < COOP_RECOVERY_LIVE || item->state > COOP_RECOVERY_ALIVE) return 0;
		for (size_t n = 0; n < i; n++)
			if (pending[n].id == item->id) return 0;
		if (item->state == COOP_RECOVERY_LIVE) {
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
		/* Saved player records already include committed transfers
		 * Receipts must not follow old slot numbers into the restored session */
		if (items[i].state == COOP_RECOVERY_TAKEN) items[i].recipient = 255;
		apply_grant[i] = 0;
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
