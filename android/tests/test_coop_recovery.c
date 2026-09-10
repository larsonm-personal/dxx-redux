/* Multiplayer recovery integration harness: real ownership/transfer code with
 * simulated engine objects, inventory and an adversarial packet transport */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>
#include "coop_recovery.h"
#include "byteswap.h"
#include "game.h"
#include "multi.h"
#include "powerup.h"
#include "weapon.h"

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

object Objects[MAX_OBJECTS];
int Highest_object_index, Game_mode, Player_num, Difficulty_level, N_players;
struct netgame_info Netgame;
#ifdef DXX_BUILD_DESCENT_II
player Players[MAX_PLAYERS + 4];
fix Omega_charge;
const int Primary_ammo_max[MAX_PRIMARY_WEAPONS] = {0, 1568};
const ubyte Secondary_ammo_max[MAX_SECONDARY_WEAPONS] = {20,10,10,5,5,20,20,15,10,10};
#else
player Players[MAX_PLAYERS];
const int Primary_ammo_max[MAX_PRIMARY_WEAPONS] = {0, 1568};
const ubyte Secondary_ammo_max[MAX_PRIMARY_WEAPONS] = {20,10,10,5,5};
#endif
int Net_create_loc, Net_create_objnums[MAX_NET_CREATE_OBJECTS];
powerup_type_info Powerup_info[MAX_POWERUP_TYPES];
static int pickup_sounds, pickup_flashes, broadcast_sounds, last_pickup_sound;
void digi_play_sample(int sound, fix volume)
{
    CHECK(volume == F1_0);
    pickup_sounds++;
    last_pickup_sound = sound;
}
void multi_send_play_sound(int sound, fix volume)
{
    CHECK(volume == F1_0 && sound >= 0);
    broadcast_sounds++;
}
void PALETTE_FLASH_ADD(int red, int green, int blue)
{
    CHECK(red || green || blue);
    pickup_flashes++;
}
static int master;
static int mapping_offset;
static int reused_remote;
static ubyte packets[4096][160];
static int packet_count;

int d_stricmp(const char *a, const char *b)
{
#ifdef _WIN32
    return _stricmp(a,b);
#else
    return strcasecmp(a,b);
#endif
}
int objnum_local_to_remote(int n, sbyte *owner) { *owner = -1; return n + mapping_offset; }
int objnum_remote_to_local(int n, int owner) { (void)owner; return n == reused_remote ? 10 : n - mapping_offset; }
int multi_i_am_master(void) { return master; }
int multi_who_is_master(void) { return 0; }
fix64 timer_query(void) { return 100 * F1_0; }
#ifdef DXX_BUILD_DESCENT_II
fix vm_vec_dist_quick(const vms_vector *a, const vms_vector *b)
#else
fix vm_vec_dist_quick(vms_vector *a, vms_vector *b)
#endif
{ return abs(a->x - b->x); }
void Error(const char *fmt, ...) { (void)fmt; abort(); }
void multi_send_remobj(int n) { CHECK(n >= 0); }
void multi_send_ship_status(void) {}
void multi_send_data(unsigned char *buf, int len, int priority)
{
    CHECK(len == 160 && priority == 2 && packet_count < 4096);
    memcpy(packets[packet_count++], buf, 160);
}
#ifdef DXX_BUILD_DESCENT_II
void multi_send_data_direct(unsigned char *buf, int len, int pnum, int priority)
#else
void multi_send_data_direct(const ubyte *buf, int len, int pnum, int priority)
#endif
{
    (void)pnum;
    multi_send_data((ubyte *)buf, len, priority);
}

void coop_snapshot_player(int pnum, coop_player_record *r)
{
    player *p = &Players[pnum];
    memset(r, 0, sizeof(*r));
    memcpy(r->callsign, p->callsign, sizeof(r->callsign));
    memcpy(r->client_id, Netgame.players[pnum].client_id, sizeof(r->client_id));
    r->primary_weapon_flags = p->primary_weapon_flags;
    r->secondary_weapon_flags = p->secondary_weapon_flags;
    r->laser_level = p->laser_level;
    r->flags = p->flags;
    r->omega_charge = coop_recovery_omega(pnum);
    memcpy(r->primary_ammo, p->primary_ammo, sizeof(p->primary_ammo));
    memcpy(r->secondary_ammo, p->secondary_ammo, sizeof(p->secondary_ammo));
}

static void reset(void)
{
    coop_recovery_reset();
    pickup_sounds = pickup_flashes = broadcast_sounds = 0;
    last_pickup_sound = -1;
    for (int i = 0; i < MAX_POWERUP_TYPES; i++) Powerup_info[i].hit_sound = 100 + i;
    memset(Objects, 0, sizeof(Objects));
    memset(Players, 0, sizeof(Players));
    memset(&Netgame, 0, sizeof(Netgame));
    Game_mode = GM_MULTI_COOP;
    Netgame.game_flags = NETGAME_FLAG_COOP_QOL;
    Player_num = 0;
    master = 1;
    Highest_object_index = 30;
    N_players = 3;
    Net_create_loc = packet_count = mapping_offset = 0;
    reused_remote = -1;
    for (int i = 0; i < 3; i++) {
        snprintf(Players[i].callsign, sizeof(Players[i].callsign), "p%d", i);
        snprintf(Netgame.players[i].client_id, sizeof(Netgame.players[i].client_id), "identity-%d", i);
        Players[i].objnum = i;
        Players[i].connected = CONNECT_PLAYING;
        Players[i].primary_weapon_flags = 1;
        Objects[i].type = OBJ_PLAYER;
        Objects[i].shields = F1_0;
    }
}

static void egg(int n, int id)
{
    Objects[n].type = OBJ_POWERUP;
    Objects[n].id = (ubyte)id;
    Objects[n].signature = 1000 + n;
    Objects[n].flags = OF_PLAYER_DROPPED;
    Objects[n].ctype.powerup_info.count = 196;
    Net_create_objnums[Net_create_loc++] = n;
}

/* Simulate the normal engine's inventory result, then exercise the real
 * tracking hook. Paired emulator tests exercise do_powerup itself */
static void request(int n, int pnum)
{
    int local = Player_num;
    Player_num = pnum;
    coop_player_record before;
    coop_snapshot_player(pnum, &before);
    CHECK(!coop_recovery_pickup_blocked(&Objects[n]));
    int used = 1;
    switch (Objects[n].id) {
    case POW_HOMING_AMMO_4:
    case POW_HOMING_AMMO_1: {
        int amount = coop_recovery_pickup_count(&Objects[n], HOMING_INDEX, Objects[n].id == POW_HOMING_AMMO_4 ? 4 : 1);
        if (amount > Secondary_ammo_max[HOMING_INDEX] - Players[pnum].secondary_ammo[HOMING_INDEX]) amount = Secondary_ammo_max[HOMING_INDEX] - Players[pnum].secondary_ammo[HOMING_INDEX];
        Players[pnum].secondary_ammo[HOMING_INDEX] += amount;
        used = amount > 0;
        break;
    }
    case POW_VULCAN_WEAPON: {
        int amount = Primary_ammo_max[VULCAN_INDEX] - Players[pnum].primary_ammo[VULCAN_INDEX];
        Players[pnum].primary_ammo[VULCAN_INDEX] += amount < 100 ? amount : 100;
        used = !(Players[pnum].primary_weapon_flags & (1u << VULCAN_INDEX));
        Players[pnum].primary_weapon_flags |= 1u << VULCAN_INDEX;
        break;
    }
    default: CHECK(0);
    }
    coop_recovery_note_pickup(&Objects[n], &before, used);
    if (used) Objects[n].flags |= OF_SHOULD_BE_DEAD;
    Player_num = local;
}

static void test_partial_collection_expiry_and_rejoin(void)
{
    coop_player_record absent, again;
    reset();
    Players[1].primary_weapon_flags |= 1u << PLASMA_INDEX;
    Players[1].secondary_ammo[HOMING_INDEX] = 6;
    egg(10, POW_PLASMA_WEAPON);
    egg(11, POW_HOMING_AMMO_4);
    egg(12, POW_HOMING_AMMO_1);
    egg(13, POW_HOMING_AMMO_1);
    coop_recovery_drop(1, coop_recovery_life(1));
    coop_snapshot_player(1, &absent);
    coop_recovery_departure_record(1, &absent);
    CHECK(absent.primary_weapon_flags == 1 && absent.secondary_ammo[HOMING_INDEX] == 0);
    request(11, 2);
    CHECK(Players[2].secondary_ammo[HOMING_INDEX] == 4);
    CHECK(Objects[11].flags & OF_SHOULD_BE_DEAD);
    coop_recovery_expire(&Objects[12]);
    CHECK(coop_recovery_prepare_rejoin(1, &absent) == 3);
    CHECK(absent.primary_weapon_flags & (1u << PLASMA_INDEX));
    CHECK(absent.secondary_ammo[HOMING_INDEX] == 2);
    CHECK(Objects[10].flags & OF_SHOULD_BE_DEAD);
    CHECK(Objects[13].flags & OF_SHOULD_BE_DEAD);
    again = absent;
    CHECK(coop_recovery_prepare_rejoin(1, &again) == 0);
    CHECK(again.secondary_ammo[HOMING_INDEX] == 2);
    /* Failed delivery caches the host's resulting inventory, never the spew */
    coop_recovery_departure_record(1, &again);
    CHECK(again.primary_weapon_flags == absent.primary_weapon_flags);
    CHECK(again.secondary_ammo[HOMING_INDEX] + Players[2].secondary_ammo[HOMING_INDEX] == 6);
    CHECK(pickup_sounds == 0 && pickup_flashes == 0 && broadcast_sounds == 0);
}

static void test_capacity_partial_weapon_and_save_restore(void)
{
    coop_player_record absent;
    reset();
    Players[1].primary_weapon_flags |= 1u << VULCAN_INDEX;
    Players[1].primary_ammo[VULCAN_INDEX] = 100;
    Players[2].primary_weapon_flags |= 1u << VULCAN_INDEX;
    Players[2].primary_ammo[VULCAN_INDEX] = Primary_ammo_max[VULCAN_INDEX] - 30;
    egg(10, POW_VULCAN_WEAPON);
    coop_recovery_drop(1, coop_recovery_life(1));
    request(10, 2);
    CHECK(Players[2].primary_ammo[VULCAN_INDEX] == Primary_ammo_max[VULCAN_INDEX]);
    CHECK(!(Objects[10].flags & OF_SHOULD_BE_DEAD));
    CHECK(coop_recovery_set_pending(coop_recovery_data(), coop_recovery_count()));
    Objects[20] = Objects[10];
    Objects[10].type = OBJ_NONE;
    CHECK(coop_recovery_apply_pending());
    coop_snapshot_player(1, &absent);
    coop_recovery_departure_record(1, &absent);
    CHECK(coop_recovery_prepare_rejoin(1, &absent) == 1);
    CHECK(absent.primary_ammo[VULCAN_INDEX] == 70);
    CHECK(absent.primary_weapon_flags & (1u << VULCAN_INDEX));
    CHECK(Objects[20].flags & OF_SHOULD_BE_DEAD);
}

static void test_disconnect_before_drop_and_overflow(void)
{
    coop_player_record retained;
    reset();
    Players[1].primary_weapon_flags |= 1u << FUSION_INDEX;
    Players[1].secondary_ammo[HOMING_INDEX] = 10;
    coop_snapshot_player(1, &retained);
    coop_recovery_departure_record(1, &retained);
    CHECK(coop_recovery_prepare_rejoin(1, &retained) == 0);
    CHECK(retained.primary_weapon_flags & (1u << FUSION_INDEX));
    /* No free object slots: all undropped gear must survive as credits */
    coop_recovery_drop(1, coop_recovery_life(1));
    coop_recovery_departure_record(1, &retained);
    coop_recovery_alive(1);
	retained.secondary_ammo[HOMING_INDEX] = 8;
    coop_recovery_prepare_rejoin(1, &retained);
    CHECK(retained.secondary_ammo[HOMING_INDEX] == 10);
    retained.secondary_ammo[HOMING_INDEX] = 0;
    coop_recovery_prepare_rejoin(1, &retained);
    CHECK(retained.secondary_ammo[HOMING_INDEX] == 8);
}



static void test_identity_level_transition_and_omega(void)
{
    coop_player_record absent;
    reset();
    Players[1].flags |= PLAYER_FLAGS_QUAD_LASERS;
    egg(10, POW_QUAD_FIRE);
#ifdef DXX_BUILD_DESCENT_II
    Players[1].primary_weapon_flags |= 1u << OMEGA_INDEX;
    coop_recovery_set_omega(1, F1_0 / 3);
    egg(11, POW_OMEGA_WEAPON);
#endif
    coop_recovery_drop(1, coop_recovery_life(1));
    coop_recovery_level_leave();
    coop_snapshot_player(1, &absent);
    coop_recovery_departure_record(1, &absent);
    strcpy(Players[2].callsign, Players[1].callsign);
    coop_recovery_prepare_rejoin(2, &absent);
    CHECK(!(absent.flags & PLAYER_FLAGS_QUAD_LASERS));
    coop_recovery_prepare_rejoin(1, &absent);
    CHECK(absent.flags & PLAYER_FLAGS_QUAD_LASERS);
#ifdef DXX_BUILD_DESCENT_II
    CHECK(absent.omega_charge == F1_0 / 3);
#endif
}

static void test_late_drop_and_consumed_mines(void)
{
	coop_player_record cached;
	reset();
	Players[1].secondary_ammo[HOMING_INDEX] = 4;
	Players[1].secondary_ammo[PROXIMITY_INDEX] = 3;
	/* Disconnect snapshot precedes the reliable death packet */
	coop_snapshot_player(1, &cached);
	egg(10, POW_HOMING_AMMO_4);
	coop_recovery_begin_drop(1);
	coop_recovery_consume_mine(1, PROXIMITY_INDEX, 20);
	coop_recovery_consume_mine(1, PROXIMITY_INDEX, -1);
	coop_recovery_drop(1, 0);
	coop_recovery_prepare_rejoin(1, &cached);
	CHECK(cached.secondary_ammo[HOMING_INDEX] == 4);
	CHECK(cached.secondary_ammo[PROXIMITY_INDEX] == 2);
	CHECK(Objects[10].flags & OF_SHOULD_BE_DEAD);
	/* A packet from before rejoin cannot create another ownership batch */
	size_t rows = coop_recovery_count();
	coop_recovery_drop(1, 0);
	CHECK(coop_recovery_count() == rows);
}

static void test_host_migration_object_mapping(void)
{
    coop_player_record returning;
    reset();
    mapping_offset = 100;
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    CHECK(coop_recovery_data()[0].remote_index == 110);
    mapping_offset = 0;
    coop_recovery_host_changed();
    CHECK(coop_recovery_data()[0].remote_index == 10);
    coop_snapshot_player(1, &returning);
    coop_recovery_prepare_rejoin(1, &returning);
    CHECK(returning.secondary_ammo[HOMING_INDEX] == 4);
    CHECK(Objects[10].flags & OF_SHOULD_BE_DEAD);
}



static void test_reordered_collection_and_reclaim(void)
{
    reset();
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    coop_recovery_item initial = coop_recovery_data()[0];
    request(10, 2);
    CHECK(coop_recovery_count() == 2); /* One tag and one life marker, no receipts */
    ubyte collected[160];
    memcpy(collected, packets[packet_count - 1], sizeof(collected));
    uint32_t generation = coop_recovery_epoch();
    CHECK(coop_recovery_set_pending(&initial, 1));
    coop_recovery_begin_restore(generation);
    Objects[10].flags &= ~OF_SHOULD_BE_DEAD;
    CHECK(coop_recovery_apply_pending());
    coop_recovery_end_restore();
    Objects[10].flags &= ~OF_SHOULD_BE_DEAD;
    Players[2].secondary_ammo[HOMING_INDEX] = 0;
    coop_recovery_receive(collected, 2);
    coop_recovery_receive(collected, 2);
    CHECK(Players[2].secondary_ammo[HOMING_INDEX] == 0); /* Tracking never grants inventory */
    CHECK(coop_recovery_data()[0].state == COOP_RECOVERY_TAKEN);
    CHECK(Objects[10].flags & OF_SHOULD_BE_DEAD);
}

static void test_freeze_includes_late_pickup(void)
{
    reset();
    Players[1].connected = CONNECT_DISCONNECTED;
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    CHECK(!coop_recovery_rejoin_ready(Players[1].callsign, Netgame.players[1].client_id));
    CHECK(coop_recovery_pickup_blocked(&Objects[10]));
    CHECK(!coop_recovery_save_ready());
    ubyte reply[160] = {0};
    reply[1] = 4; /* REC_FROZEN */
    PUT_INTEL_INT(reply + 4, coop_recovery_epoch());
    coop_recovery_item remaining = coop_recovery_data()[0];
    remaining.gear.missiles[HOMING_INDEX] = 1; /* Three collected before freeze arrived */
    memcpy(reply + 16, &remaining, sizeof(remaining));
    coop_recovery_receive(reply, 2);
    CHECK(coop_recovery_rejoin_ready(Players[1].callsign, Netgame.players[1].client_id));
    CHECK(coop_recovery_save_ready());
    coop_player_record returning;
    coop_snapshot_player(1, &returning);
    coop_recovery_prepare_rejoin(1, &returning);
    CHECK(returning.secondary_ammo[HOMING_INDEX] == 1);
    CHECK(!coop_recovery_accept_ship_status(1, 0, coop_recovery_life(1)));
    CHECK(coop_recovery_accept_ship_status(1, coop_recovery_restore_serial(1), coop_recovery_life(1)));
    CHECK(Objects[10].flags & OF_SHOULD_BE_DEAD);
    CHECK(coop_recovery_prepare_rejoin(1, &returning) == 0);
    CHECK(returning.secondary_ammo[HOMING_INDEX] == 1);
}

static void test_remove_before_partial_collection(void)
{
    reset();
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    ubyte report[160] = {0};
    report[1] = 2; /* REC_COLLECT */
    PUT_INTEL_INT(report + 4, coop_recovery_epoch());
    coop_recovery_item partial = coop_recovery_data()[0];
    partial.state = COOP_RECOVERY_CREDIT;
    partial.gear.missiles[HOMING_INDEX] = 3;
    memcpy(report + 16, &partial, sizeof(partial));
    coop_recovery_note_remove(&Objects[10]);
    CHECK(coop_recovery_data()[0].state == COOP_RECOVERY_TAKEN);
    coop_recovery_expire(&Objects[10]); /* Removal must not turn into expiry credit */
    CHECK(coop_recovery_data()[0].state == COOP_RECOVERY_TAKEN);
    coop_recovery_receive(report, 2);
    CHECK(coop_recovery_data()[0].state == COOP_RECOVERY_CREDIT);
    CHECK(coop_recovery_data()[0].gear.missiles[HOMING_INDEX] == 3);
    coop_player_record returning;
    coop_snapshot_player(1, &returning);
    coop_recovery_prepare_rejoin(1, &returning);
    CHECK(returning.secondary_ammo[HOMING_INDEX] == 3);
    coop_recovery_receive(report, 2); /* Late old result must not refill reclaimed credit */
    CHECK(coop_recovery_prepare_rejoin(1, &returning) == 0);
    CHECK(returning.secondary_ammo[HOMING_INDEX] == 3);
}

static void test_repeated_freeze_preserves_local_collection(void)
{
    reset();
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    coop_recovery_item frozen = coop_recovery_data()[0];
    frozen.state = COOP_RECOVERY_RECLAIMING;
    frozen.revision++;
    Players[2].secondary_ammo[HOMING_INDEX] = Secondary_ammo_max[HOMING_INDEX] - 1;
    request(10, 2);
    Player_num = 2;
    master = 0;
    ubyte freeze[160] = {0};
    freeze[1] = 3; /* REC_FREEZE */
    PUT_INTEL_INT(freeze + 4, coop_recovery_epoch());
    memcpy(freeze + 16, &frozen, sizeof(frozen));
    coop_recovery_receive(freeze, 0);
    coop_recovery_receive(freeze, 0);
    coop_recovery_item reply;
    memcpy(&reply, packets[packet_count - 1] + 16, sizeof(reply));
    CHECK(reply.gear.missiles[HOMING_INDEX] == 3);
    CHECK(coop_recovery_pickup_blocked(&Objects[10]));
}

static void test_consumed_remote_slot_cannot_remove_new_host_gear(void)
{
    reset();
    mapping_offset = 100;
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    coop_recovery_item retired = coop_recovery_data()[0];
    /* A remove packet retires the old object before its receipt arrives */
    coop_recovery_note_remove(&Objects[10]);
    Objects[10].type = OBJ_NONE;
    /* The old remote mapping still points here, but this is new host gear */
    Net_create_loc = 0;
    mapping_offset = 0;
    reused_remote = 110;
    Players[0].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    Objects[10].signature = 18117;
    coop_recovery_drop(0, 0);
    coop_recovery_frame();
    CHECK(!(Objects[10].flags & OF_SHOULD_BE_DEAD));
    CHECK(coop_recovery_data()[0].signature != 18117);
    /* Delayed terminal and older live packets must not revive that binding */
    ubyte delayed[160] = {0};
    delayed[1] = 1; /* REC_ITEM */
    PUT_INTEL_INT(delayed + 4, coop_recovery_epoch());
    retired.state = COOP_RECOVERY_TAKEN;
    retired.revision++;
    memcpy(delayed + 16, &retired, sizeof(retired));
    master = 0;
    Player_num = 2;
    coop_recovery_receive(delayed, 0);
    retired.state = COOP_RECOVERY_LIVE;
    retired.revision--;
    memcpy(delayed + 16, &retired, sizeof(retired));
    coop_recovery_receive(delayed, 0);
    coop_recovery_frame();
    CHECK(!(Objects[10].flags & OF_SHOULD_BE_DEAD));
    CHECK(coop_recovery_data()[0].object_index == -1);
}

static void test_replayed_live_tag_cannot_rebind_a_lost_generation(void)
{
    reset();
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    coop_recovery_item old = coop_recovery_data()[0];
    /* Same owner/index and type, but a different local object generation */
    Objects[10].signature = 18117;
    ubyte delayed[160] = {0};
    delayed[1] = 1;
    PUT_INTEL_INT(delayed + 4, coop_recovery_epoch());
    memcpy(delayed + 16, &old, sizeof(old));
    master = 0;
    Player_num = 2;
    coop_recovery_receive(delayed, 0);
    CHECK(coop_recovery_data()[0].gear.missiles[HOMING_INDEX] == 0);
    old.state = COOP_RECOVERY_TAKEN;
    old.revision++;
    memcpy(delayed + 16, &old, sizeof(old));
    coop_recovery_receive(delayed, 0);
    coop_recovery_frame();
    CHECK(!(Objects[10].flags & OF_SHOULD_BE_DEAD));
}

static void test_terminal_receipt_preserves_known_partial_pickup_credit(void)
{
    reset();
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    Player_num = 1;
    coop_player_record before;
    coop_snapshot_player(1, &before);
    Players[1].secondary_ammo[HOMING_INDEX]++;
    coop_recovery_note_pickup(&Objects[10], &before, 1);
    coop_recovery_item receipt = coop_recovery_data()[0];
    CHECK(receipt.state == COOP_RECOVERY_CREDIT);
    CHECK(receipt.gear.missiles[HOMING_INDEX] == 3);
    Objects[10].type = OBJ_NONE;
    ubyte packet[160] = {0};
    packet[1] = 1;
    PUT_INTEL_INT(packet + 4, coop_recovery_epoch());
    memcpy(packet + 16, &receipt, sizeof(receipt));
    master = 0;
    coop_recovery_receive(packet, 0);
    CHECK(coop_recovery_data()[0].gear.missiles[HOMING_INDEX] == 3);
}

static void test_delayed_freeze_cannot_rebind_consumed_gear(void)
{
    reset();
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    coop_recovery_item original = coop_recovery_data()[0];
    coop_recovery_note_remove(&Objects[10]);
    Objects[10].signature = 18117;
    ubyte packet[160] = {0};
    packet[1] = 3; /* REC_FREEZE */
    original.revision++;
    PUT_INTEL_INT(packet + 4, coop_recovery_epoch());
    memcpy(packet + 16, &original, sizeof(original));
    master = 0;
    Player_num = 2;
    coop_recovery_receive(packet, 0);
    coop_recovery_frame();
    CHECK(coop_recovery_data()[0].object_index == -1);
    CHECK(!(Objects[10].flags & OF_SHOULD_BE_DEAD));
}

static void test_restore_discards_missing_gear(void)
{
    reset();
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    coop_recovery_item saved[2];
    saved[0] = coop_recovery_data()[0];
    saved[1] = saved[0];
    saved[1].id = 81;
    saved[1].signature = 18117;
    saved[1].object_index = 254;
    saved[1].remote_index = 254;
    CHECK(coop_recovery_set_pending(saved, 2));
    CHECK(coop_recovery_apply_pending());
    CHECK(coop_recovery_count() == 1);
    CHECK(coop_recovery_restore_result().discarded == 1);
    CHECK(coop_recovery_data()[0].id == saved[0].id);
    CHECK(coop_recovery_set_pending(coop_recovery_data(), coop_recovery_count()));
    CHECK(coop_recovery_apply_pending());
    coop_player_record returning;
    coop_snapshot_player(1, &returning);
    returning.secondary_ammo[HOMING_INDEX] = 0;
    CHECK(coop_recovery_prepare_rejoin(1, &returning) == 1);
    CHECK(returning.secondary_ammo[HOMING_INDEX] == 4);
}

static void test_restore_discards_conflicts_without_inventory_credit(void)
{
    reset();
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0);
    coop_recovery_item original = coop_recovery_data()[0];
    for (int conflict = 0; conflict < 3; conflict++) {
        coop_recovery_item saved[3] = {original, original, original};
        /* An unrelated terminal record survives every malformed claim */
        saved[2].id = 90;
        saved[2].state = COOP_RECOVERY_TAKEN;
        saved[2].object_index = saved[2].remote_index = -1;
        if (conflict == 1) saved[1].id = 81; /* Distinct IDs claim one object */
        if (conflict == 2) {
            saved[1].id = 81;
            saved[1].signature = 18117;
            saved[1].gear.missiles[HOMING_INDEX] = UINT16_MAX;
        }
        CHECK(coop_recovery_set_pending(saved, 3));
        CHECK(coop_recovery_apply_pending());
        CHECK(coop_recovery_restore_result().discarded == (conflict == 2 ? 1 : 2));
        CHECK(coop_recovery_count() == (conflict == 2 ? 2 : 1));
        CHECK(!(Objects[10].flags & OF_SHOULD_BE_DEAD));
        CHECK(Players[1].secondary_ammo[HOMING_INDEX] == 4);
    }
}

int main(void)
{
    test_delayed_freeze_cannot_rebind_consumed_gear();
    test_terminal_receipt_preserves_known_partial_pickup_credit();
    test_replayed_live_tag_cannot_rebind_a_lost_generation();
    test_restore_discards_conflicts_without_inventory_credit();
    test_consumed_remote_slot_cannot_remove_new_host_gear();
    test_restore_discards_missing_gear();
    test_repeated_freeze_preserves_local_collection();
    test_remove_before_partial_collection();
    test_reordered_collection_and_reclaim();
    test_freeze_includes_late_pickup();
    test_partial_collection_expiry_and_rejoin();
    test_capacity_partial_weapon_and_save_restore();
    test_disconnect_before_drop_and_overflow();
	test_identity_level_transition_and_omega();
	test_late_drop_and_consumed_mines();
    test_host_migration_object_mapping();
    puts("Coop recovery integration scenarios passed");
    return 0;
}

