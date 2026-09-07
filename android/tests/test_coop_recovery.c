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
int Highest_object_index, Game_mode, Player_num, Difficulty_level;
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
static int master;
static int mapping_offset;
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
int objnum_remote_to_local(int n, int owner) { (void)owner; return n - mapping_offset; }
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
    memset(Objects, 0, sizeof(Objects));
    memset(Players, 0, sizeof(Players));
    memset(&Netgame, 0, sizeof(Netgame));
    Game_mode = GM_MULTI_COOP;
    Netgame.game_flags = NETGAME_FLAG_COOP_QOL;
    Player_num = 0;
    master = 1;
    Highest_object_index = 30;
    Net_create_loc = packet_count = mapping_offset = 0;
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

static void request(int n, int pnum)
{
    ubyte buf[160] = {0};
    const coop_recovery_item *rows = coop_recovery_data();
    for (size_t i = 0; i < coop_recovery_count(); i++) if (rows[i].object_index == n && rows[i].state == COOP_RECOVERY_LIVE) {
        coop_recovery_item item = rows[i];
        item.life = coop_recovery_life(pnum);
        buf[1] = 3;
        PUT_INTEL_INT(buf + 4, coop_recovery_epoch());
        memcpy(buf + 16, &item, sizeof(item));
        coop_recovery_receive(buf, pnum);
        return;
    }
    CHECK(0);
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
    coop_recovery_drop(1, coop_recovery_player_revision(1), coop_recovery_life(1));
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
    coop_recovery_drop(1, coop_recovery_player_revision(1), coop_recovery_life(1));
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
    coop_recovery_drop(1, coop_recovery_player_revision(1), coop_recovery_life(1));
    coop_recovery_departure_record(1, &retained);
    coop_recovery_alive(1);
	retained.secondary_ammo[HOMING_INDEX] = 8;
    coop_recovery_prepare_rejoin(1, &retained);
    CHECK(retained.secondary_ammo[HOMING_INDEX] == 10);
    retained.secondary_ammo[HOMING_INDEX] = 0;
    coop_recovery_prepare_rejoin(1, &retained);
    CHECK(retained.secondary_ammo[HOMING_INDEX] == 8);
}

static void test_grant_racing_death(void)
{
    coop_player_record absent;
    reset();
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, coop_recovery_player_revision(1), coop_recovery_life(1));
    request(10, 2);
    CHECK(Players[2].secondary_ammo[HOMING_INDEX] == 4);
    /* The collector died before receiving this grant, so its drop reports 0 */
    Players[2].secondary_ammo[HOMING_INDEX] = 0;
    Net_create_loc = 0;
    coop_recovery_drop(2, 0, 0);
    coop_snapshot_player(2, &absent);
    coop_recovery_departure_record(2, &absent);
    coop_recovery_prepare_rejoin(2, &absent);
    CHECK(absent.secondary_ammo[HOMING_INDEX] == 4);
    coop_snapshot_player(1, &absent);
    coop_recovery_departure_record(1, &absent);
    coop_recovery_prepare_rejoin(1, &absent);
    CHECK(absent.secondary_ammo[HOMING_INDEX] == 0);
}

static void test_reordered_duplicate_grants(void)
{
    ubyte grants[2][160];
    int n = 0;
    reset();
    Players[1].secondary_ammo[HOMING_INDEX] = 2;
    egg(10, POW_HOMING_AMMO_1);
    egg(11, POW_HOMING_AMMO_1);
    coop_recovery_drop(1, coop_recovery_player_revision(1), coop_recovery_life(1));
    request(10, 2);
    request(11, 2);
    for (int i = 0; i < packet_count; i++) if (packets[i][1] == 2) {
        CHECK(n < 2);
        memcpy(grants[n++], packets[i], 160);
    }
    CHECK(n == 2);
    reset();
	Player_num = 2;
	master = 0;
	/* A snapshot can overtake the live grant; it must not swallow the grant */
	ubyte snapshot_row[160];
	memcpy(snapshot_row, grants[0], sizeof(snapshot_row));
	snapshot_row[1] = 1;
	coop_recovery_receive(snapshot_row, 0);
	coop_recovery_receive(grants[1], 0);
    coop_recovery_frame();
    CHECK(Players[2].secondary_ammo[HOMING_INDEX] == 0);
    coop_recovery_receive(grants[0], 0);
    coop_recovery_frame();
    CHECK(Players[2].secondary_ammo[HOMING_INDEX] == 2);
    coop_recovery_receive(grants[0], 0);
    coop_recovery_receive(grants[1], 0);
    coop_recovery_frame();
    CHECK(Players[2].secondary_ammo[HOMING_INDEX] == 2);
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
    coop_recovery_drop(1, coop_recovery_player_revision(1), coop_recovery_life(1));
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
	coop_recovery_drop(1, 0, 0);
	coop_recovery_prepare_rejoin(1, &cached);
	CHECK(cached.secondary_ammo[HOMING_INDEX] == 4);
	CHECK(cached.secondary_ammo[PROXIMITY_INDEX] == 2);
	CHECK(Objects[10].flags & OF_SHOULD_BE_DEAD);
	/* A packet from before rejoin cannot create another ownership batch */
	size_t rows = coop_recovery_count();
	coop_recovery_drop(1, 0, 0);
	CHECK(coop_recovery_count() == rows);
}

static void test_host_migration_object_mapping(void)
{
    coop_player_record returning;
    reset();
    mapping_offset = 100;
    Players[1].secondary_ammo[HOMING_INDEX] = 4;
    egg(10, POW_HOMING_AMMO_4);
    coop_recovery_drop(1, 0, 0);
    CHECK(coop_recovery_data()[0].remote_index == 110);
    mapping_offset = 0;
    coop_recovery_host_changed();
    CHECK(coop_recovery_data()[0].remote_index == 10);
    coop_snapshot_player(1, &returning);
    coop_recovery_prepare_rejoin(1, &returning);
    CHECK(returning.secondary_ammo[HOMING_INDEX] == 4);
    CHECK(Objects[10].flags & OF_SHOULD_BE_DEAD);
}

int main(void)
{
    test_partial_collection_expiry_and_rejoin();
    test_capacity_partial_weapon_and_save_restore();
    test_disconnect_before_drop_and_overflow();
    test_grant_racing_death();
    test_reordered_duplicate_grants();
	test_identity_level_transition_and_omega();
	test_late_drop_and_consumed_mines();
    test_host_migration_object_mapping();
    puts("Coop recovery integration scenarios passed");
    return 0;
}

