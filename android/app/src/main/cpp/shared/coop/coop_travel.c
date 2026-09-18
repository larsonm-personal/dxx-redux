#include "coop_travel.h"
#include "coop_campaign.h"
#include "coop_portable.h"
#include "coop_save.h"
#include "coop_recovery.h"
#include "coop_briefing.h"
#include "coop_endgame.h"
#include "coop_world_visit.h"
#include "android_log.h"
#include "game.h"
#include "gameseq.h"
#include "mission.h"
#include "multi.h"
#include "net_udp.h"
#include "multi_save_transfer_policy.h"
#include "timer.h"
#include "physfsx.h"
#include "state_android_shared.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#ifdef DXX_BUILD_DESCENT_II
#include "switch.h"
#include "wall.h"
#include "hudmsg.h"
#include "gamemine.h"
#include "gameseg.h"
#include "coop_start_positions.h"
#include "cntrlcen.h"
#include "endlevel.h"
#endif

/* Fixed little-endian envelope: operation/phase/roster, session and source
 * visit, restore epoch, revision, operation generation, remaining warning
 * time and initiator. Sender identity always comes from UDP authentication */
enum { PACKET_SIZE = 56 + COOP_PORTABLE_BYTES,
	   PREPARED_HEADER = 24 + MAX_PLAYERS * COOP_PORTABLE_BYTES,
	   CAMPAIGN_HEADER = PREPARED_HEADER + 8, /* Reserved terminal world visit */
	   CHECKPOINT_HEADER = PREPARED_HEADER + 8,
	   STATE = 1,
	   REQUEST = 2,
	   ACK = 3,
	   HELLO = 4,
	   PORTABLE = 5,
	   FAILED = 6,
	   FROZEN = 7 };
static coop_transition_policy policy;
static int armed, known, host, source_level, initiator, pause_owned, pumping;
static int world_changed, applied, committed, restored, normal_taken, release_needed;
static unsigned normal_granted, release_acked;
static uint32_t game_id, recovery_epoch, revision;
#ifdef DXX_BUILD_DESCENT_II
static uint32_t local_recovery_epoch;
static uint64_t previous_nonce;
static int physical_original_flags;
static unsigned char arrival_guard[MAX_TRIGGERS];
static int arrival_guard_level;
static uint32_t arrival_guard_game;
#endif
static uint64_t local_nonce, peer_nonce[MAX_PLAYERS];
static uint64_t campaign_generation, last_send, last_ack, last_request, last_host;
static uint64_t phase_since;
static coop_transition_phase observed_phase;
static coop_operation requested;
static coop_campaign_travel prepared_campaign;
static int automatic_campaign, prepare_started, load_started, prepared_ready, prepared_destination;
static int prepared_action, advancement_presented;
static unsigned advancement_briefing_flags;
static uint64_t terminal_visit;
static uint32_t prepared_checksum;
static coop_portable_player portable[MAX_PLAYERS];
static unsigned portable_received;
static unsigned operation_players, snapshot_players;
static unsigned frozen_received, deaths_settled;
static int freeze_ready;
static rewind_memory_buffer source_checkpoint;
static int checkpoint_started, checkpoint_ready;
static uint32_t checkpoint_checksum;
static int failure_pending, rollback_required, rollback_started, recovery_entered;
static int fail_after_load, failed_destination;
static int physical_segment = -1, physical_side = -1;
static uint32_t physical_life, physical_restore_serial;
static int physical_mode, physical_pending = -1, physical_accepted = -1, physical_replay = -1;
static int physical_consumed;
static uint64_t test_request_delay_until;
static unsigned char test_delayed_request[PACKET_SIZE];
static int test_request_buffered, test_request_sent;
static int arrivals_placed;
static uint32_t arrival_checksum;
static int test_hold_arrival;
static int test_disconnect_phase = -1;
static unsigned dead_players;
static int death_waiting;

static coop_operation physical_operation(int trigger_num)
{
#ifdef DXX_BUILD_DESCENT_II
	if (trigger_num < 0 || trigger_num >= Num_triggers) return COOP_OP_NONE;
	if (Triggers[trigger_num].type == TT_EXIT)
		return Current_level_num < 0 ? COOP_OP_SECRET_RETURN : COOP_OP_NORMAL_EXIT;
	if (Triggers[trigger_num].type == TT_SECRET_EXIT && Current_level_num > 0) return COOP_OP_SECRET_ENTER;
#else
	(void) trigger_num;
#endif
	return COOP_OP_NONE;
}

static int physical_enabled(void)
{
#ifdef DXX_BUILD_DESCENT_II
	return (Game_mode & GM_MULTI_COOP) && !EMULATING_D1 && !is_SHAREWARE && !is_MAC_SHARE &&
	       (Netgame.AllowSecretWarps || Current_level_num < 0);
#else
	return 0;
#endif
}

static void clear_checkpoint(void)
{
	rewind_memory_buffer_discard(&source_checkpoint);
	checkpoint_started = checkpoint_ready = 0;
	checkpoint_checksum = 0;
}

static uint64_t now_ms(void)
{
	return (uint64_t) timer_query() * 1000 / F1_0;
}
static unsigned bit(int p)
{
	return p >= 0 && p < MAX_PLAYERS ? 1u << p : 0;
}

#ifdef DXX_BUILD_DESCENT_II
static int player_near_trigger(int player_num, int trigger_num)
{
	int objnum = Players[player_num].objnum;
	if (objnum < 0 || objnum > Highest_object_index) return 0;
	for (int i = 0; i < Num_walls; ++i) {
		if (Walls[i].trigger != trigger_num || Walls[i].segnum < 0 || Walls[i].segnum > Highest_segment_index ||
		    Walls[i].sidenum < 0 || Walls[i].sidenum >= 6) continue;
		if (Objects[objnum].segnum == Walls[i].segnum ||
		    Objects[objnum].segnum == Segments[Walls[i].segnum].children[Walls[i].sidenum]) return 1;
	}
	return 0;
}

/* Android diagnostics: compare the local crossing with the host's replicated ship */
static void log_physical_position(const char *event, int trigger_num, int player_num)
{
	const int objnum = Players[player_num].objnum;
	if (objnum < 0 || objnum > Highest_object_index) return;
	const object *obj = &Objects[objnum];
	COOPLOG("physical exit %s: local=%d player=%d level=%d trigger=%d object=%d segment=%d pos=%d,%d,%d velocity=%d,%d,%d countdown=%d",
	        event, Player_num, player_num, Current_level_num, trigger_num, objnum, obj->segnum,
	        obj->pos.x, obj->pos.y, obj->pos.z,
	        obj->mtype.phys_info.velocity.x, obj->mtype.phys_info.velocity.y, obj->mtype.phys_info.velocity.z,
	        Countdown_seconds_left);
	for (int i = 0; i < Num_walls; ++i) {
		if (Walls[i].trigger != trigger_num || Walls[i].segnum < 0 || Walls[i].segnum > Highest_segment_index ||
		    Walls[i].sidenum < 0 || Walls[i].sidenum >= 6) continue;
		COOPLOG("physical exit source: local=%d trigger=%d wall=%d segment=%d side=%d child=%d",
		        Player_num, trigger_num, i, Walls[i].segnum, Walls[i].sidenum,
		        Segments[Walls[i].segnum].children[Walls[i].sidenum]);
	}
}

static int arrival_trigger_blocked(int trigger_num, int player_num)
{
	if (arrival_guard_level != Current_level_num || arrival_guard_game != (uint32_t) Netgame.protocol.udp.GameID) {
		memset(arrival_guard, 0, sizeof(arrival_guard));
		return 0;
	}
	if (trigger_num < 0 || trigger_num >= Num_triggers || !bit(player_num) || !(arrival_guard[trigger_num] & bit(player_num))) return 0;
	if (player_near_trigger(player_num, trigger_num)) return 1;
	arrival_guard[trigger_num] &= ~bit(player_num);
	return 0;
}
#endif
static void put32(unsigned char *p, uint32_t v)
{
	for (unsigned i = 0; i < 4; ++i) p[i] = (unsigned char) (v >> (i * 8));
}
static uint32_t get32(const unsigned char *p)
{
	return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}
static void put64(unsigned char *p, uint64_t v)
{
	put32(p, (uint32_t) v);
	put32(p + 4, (uint32_t) (v >> 32));
}
static uint64_t get64(const unsigned char *p)
{
	return get32(p) | (uint64_t) get32(p + 4) << 32;
}

static int place_arrivals(void)
{
#ifdef DXX_BUILD_DESCENT_II
	obj_position anchor, positions[MAX_PLAYERS];
	int count = 0, slot = 0;
	if (!automatic_campaign || !prepared_ready || Current_level_num != prepared_destination) return 0;
	if (prepared_campaign.action == COOP_CAMPAIGN_RETURN) {
		if (Secret_return_segment < 0 || Secret_return_segment > Highest_segment_index) return 0;
		anchor.segnum = (short) Secret_return_segment;
		anchor.orient = Secret_return_orient;
		compute_segment_center(&anchor.pos, &Segments[anchor.segnum]);
	} else anchor = Player_init[0];
	for (int i = 0; i < N_players; ++i)
		if (snapshot_players & bit(i)) {
			int objnum = Players[i].objnum;
			if ((policy.participants & bit(i)) && Players[i].connected != CONNECT_DISCONNECTED &&
			    (objnum < 0 || objnum > Highest_object_index || Objects[objnum].type != OBJ_PLAYER)) return 0;
			++count;
		}
	if (!coop_find_arrival_positions(&anchor, count, positions)) {
		COOPLOG("travel arrival placement failed: level=%d anchor=%d players=%d", Current_level_num, anchor.segnum, count);
		return 0;
	}
	memset(arrival_guard, 0, sizeof(arrival_guard));
	arrival_guard_level = Current_level_num;
	arrival_guard_game = (uint32_t) Netgame.protocol.udp.GameID;
	arrival_checksum = 2166136261u;
	for (int i = 0; i < N_players; ++i) {
		if (!(snapshot_players & bit(i))) continue;
		const obj_position *position = &positions[slot++];
		unsigned char encoded[56];
		put32(encoded, (uint32_t) i);
		put32(encoded + 4, (uint32_t) position->segnum);
		const vms_vector vectors[] = { position->pos, position->orient.rvec, position->orient.uvec, position->orient.fvec };
		for (int v = 0; v < 4; ++v) {
			put32(encoded + 8 + v * 12, (uint32_t) vectors[v].x);
			put32(encoded + 12 + v * 12, (uint32_t) vectors[v].y);
			put32(encoded + 16 + v * 12, (uint32_t) vectors[v].z);
		}
		arrival_checksum = coop_save_checksum(encoded, sizeof(encoded), arrival_checksum);
		if (!(policy.participants & bit(i)) || Players[i].connected == CONNECT_DISCONNECTED) continue;
		object *obj = &Objects[Players[i].objnum];
		obj->pos = obj->last_pos = position->pos;
		obj->orient = position->orient;
		obj_relink(Players[i].objnum, position->segnum);
		vm_vec_zero(&obj->mtype.phys_info.velocity);
		vm_vec_zero(&obj->mtype.phys_info.thrust);
		vm_vec_zero(&obj->mtype.phys_info.rotvel);
		vm_vec_zero(&obj->mtype.phys_info.rotthrust);
		obj->mtype.phys_info.turnroll = 0;
		for (int t = 0; t < Num_triggers; ++t)
			if (physical_operation(t) && player_near_trigger(i, t)) arrival_guard[t] |= bit(i);
		COOPLOG("travel arrival: local=%d player=%d level=%d anchor=%d segment=%d pos=%d,%d,%d",
		        Player_num, i, Current_level_num, anchor.segnum, obj->segnum, obj->pos.x, obj->pos.y, obj->pos.z);
	}
	arrivals_placed = 1;
	game_flush_inputs();
	return 1;
#else
	return 0;
#endif
}

void coop_travel_get_arrival_state(int *placed, uint32_t *checksum, int *blocked)
{
	if (placed) *placed = arrivals_placed;
	if (checksum) *checksum = arrival_checksum;
	if (blocked) {
		*blocked = 0;
#ifdef DXX_BUILD_DESCENT_II
		if (arrival_guard_level == Current_level_num && arrival_guard_game == (uint32_t) Netgame.protocol.udp.GameID)
			for (int t = 0; t < Num_triggers; ++t)
				if ((arrival_guard[t] & bit(Player_num)) && player_near_trigger(Player_num, t)) ++*blocked;
#endif
	}
}

int coop_travel_test_disconnect_at(int phase)
{
	if (!armed || Player_num != host || policy.phase != COOP_PHASE_SETTLED) return 0;
	test_disconnect_phase = phase;
	return 1;
}

int coop_travel_test_warning_delay(void)
{
	if (!armed || policy.phase != COOP_PHASE_SETTLED) return 0;
	policy.test_secret_warning_ms = COOP_SECRET_TEST_WARNING_MS;
	return 1;
}

int coop_travel_test_hold_arrival(int hold)
{
	if (!armed || !known || (policy.phase != COOP_PHASE_WARNING && policy.phase != COOP_PHASE_LOADING)) return 0;
	test_hold_arrival = !!hold;
	return 1;
}

static int packet_trigger(const unsigned char *packet)
{
	uint32_t encoded = get32(packet + 56);
	return encoded < 65536 ? (int) encoded - 1 : -1;
}

int coop_travel_exit_side_blocked(int segnum, int side)
{
#ifdef DXX_BUILD_DESCENT_II
	if (!physical_enabled() || segnum < 0 || segnum > Highest_segment_index || side < 0 || side >= 6) return 0;
	int wall_nums[2] = { Segments[segnum].sides[side].wall_num, -1 };
	int child = Segments[segnum].children[side];
	if (child >= 0 && child <= Highest_segment_index)
		for (int s = 0; s < 6; ++s)
			if (Segments[child].children[s] == segnum) wall_nums[1] = Segments[child].sides[s].wall_num;
	for (int i = 0; i < 2; ++i) {
		if (wall_nums[i] < 0 || wall_nums[i] >= Num_walls) continue;
		coop_operation operation = physical_operation(Walls[wall_nums[i]].trigger);
		if (operation == COOP_OP_SECRET_ENTER && !coop_secret_entry_available()) return 1;
		if (!physical_mode || !armed || source_level != Current_level_num ||
		    (physical_pending < 0 && (!known || policy.phase == COOP_PHASE_SETTLED))) continue;
		if (operation && (physical_pending >= 0 || policy.phase != COOP_PHASE_NORMAL_WAIT || operation != COOP_OP_NORMAL_EXIT)) return 1;
	}
#else
	(void) segnum;
	(void) side;
#endif
	return 0;
}

void coop_travel_get_physical_state(int *mode, int *pending, int *accepted)
{
	if (mode) *mode = physical_mode;
	if (pending) *pending = physical_pending;
	if (accepted) *accepted = physical_accepted;
}

int coop_travel_active(void)
{
	return armed && known && (policy.phase != COOP_PHASE_SETTLED || (Player_num == host && release_needed && release_acked != policy.participants));
}

int coop_travel_blocks_gameplay(void)
{
	return coop_endgame_active() || coop_travel_ending_campaign() || (coop_travel_active() && policy.phase != COOP_PHASE_NORMAL_WAIT);
}

int coop_travel_ending_campaign(void)
{
	return armed && committed && prepared_action == COOP_CAMPAIGN_ENDGAME;
}

int coop_travel_blocks_world_updates(void)
{
	return coop_travel_blocks_gameplay() &&
	       !(automatic_campaign && policy.phase == COOP_PHASE_FREEZING && !freeze_ready);
}

int coop_travel_blocks_state_actions(void)
{
	return coop_endgame_active() || coop_travel_ending_campaign() || coop_travel_active() || coop_travel_waiting_after_death() ||
	       (armed && (physical_pending >= 0 || requested != COOP_OP_NONE));
}

void coop_travel_reset(void)
{
	if (pause_owned) start_time();
	armed = known = pause_owned = pumping = 0;
	world_changed = applied = committed = restored = normal_taken = release_needed = 0;
	normal_granted = release_acked = revision = 0;
	requested = COOP_OP_NONE;
	last_send = last_ack = last_request = 0;
	memset(&policy, 0, sizeof(policy));
	memset(peer_nonce, 0, sizeof(peer_nonce));
	coop_campaign_travel_clear(&prepared_campaign);
	automatic_campaign = prepare_started = load_started = prepared_ready = prepared_destination = 0;
	prepared_action = advancement_presented = 0;
	terminal_visit = 0;
	prepared_checksum = 0;
	portable_received = 0;
	operation_players = snapshot_players = 0;
	frozen_received = deaths_settled = 0;
	freeze_ready = 0;
	memset(portable, 0, sizeof(portable));
	clear_checkpoint();
	failure_pending = rollback_required = rollback_started = recovery_entered = 0;
	fail_after_load = failed_destination = 0;
	physical_mode = 0;
	physical_segment = physical_side = -1;
	physical_life = physical_restore_serial = 0;
	physical_pending = physical_accepted = physical_replay = -1;
	physical_consumed = 0;
	test_request_delay_until = 0;
	test_request_buffered = test_request_sent = 0;
	arrivals_placed = 0;
	arrival_checksum = 0;
	test_hold_arrival = 0;
	test_disconnect_phase = -1;
	dead_players = 0;
	death_waiting = 0;
}

int coop_travel_arm(void)
{
#ifdef DXX_BUILD_DESCENT_II
	const coop_campaign *campaign = coop_campaign_current();
	unsigned participants = 0;
	if (coop_travel_active() || !(Game_mode & GM_MULTI_COOP) || EMULATING_D1 ||
	    (!Netgame.AllowSecretWarps && Current_level_num > 0) || !coop_campaign_valid(campaign) ||
	    campaign->active_level != Current_level_num || coop_briefing_active() || multi_save_transfer_busy()) return 0;
	/* Rollback can land on the same level/generation and even the same local
	 * recovery counter on one peer. Both sides still need a new nonce handshake */
	if (armed && !rollback_required && source_level == Current_level_num && campaign_generation == campaign->generation &&
	    local_recovery_epoch == coop_recovery_epoch() && game_id == (uint32_t) Netgame.protocol.udp.GameID &&
	    host == multi_who_is_master()) return 1;
	coop_travel_reset();
	host = multi_who_is_master();
	for (int i = 0; i < N_players; ++i)
		if (Players[i].connected == CONNECT_PLAYING) participants |= bit(i);
	/* The host may already be on its ordinary exit waiting screen */
	if (Players[host].connected != CONNECT_DISCONNECTED) participants |= bit(host);
	if (!coop_transition_init(&policy, now_ms() + 1, host, participants, now_ms())) return 0;
	source_level = Current_level_num;
	campaign_generation = campaign->generation;
	recovery_epoch = coop_recovery_epoch();
	local_recovery_epoch = recovery_epoch;
	local_nonce = now_ms() + 1;
	if (local_nonce <= previous_nonce) local_nonce = previous_nonce + 1;
	previous_nonce = local_nonce;
	game_id = (uint32_t) Netgame.protocol.udp.GameID;
	initiator = host;
	last_host = phase_since = now_ms();
	observed_phase = COOP_PHASE_SETTLED;
	armed = 1;
	known = Player_num == host;
	COOPLOG("travel armed: player=%d host=%d source=%d campaign=%llu local_epoch=%u nonce=%llu",
	        Player_num, host, source_level, (unsigned long long) campaign_generation,
	        local_recovery_epoch, (unsigned long long) local_nonce);
	return 1;
#else
	return 0;
#endif
}

static void send_packet(int kind)
{
	unsigned char packet[PACKET_SIZE] = { MULTI_COOP_TRAVEL };
	uint64_t now = now_ms();
	packet[1] = (unsigned char) kind;
	packet[2] = (unsigned char) (kind == REQUEST ? requested : policy.operation);
	packet[3] = (unsigned char) policy.phase;
	packet[4] = policy.participants;
	packet[5] = policy.acknowledged;
	packet[6] = (unsigned char) host;
	packet[7] = (unsigned char) normal_granted;
	put32(packet + 8, game_id);
	put32(packet + 12, (uint32_t) source_level);
	put64(packet + 16, campaign_generation);
	put32(packet + 24, recovery_epoch);
	put32(packet + 28, kind == STATE ? ++revision : revision);
	put64(packet + 32, policy.generation);
	put32(packet + 40, (uint32_t) (policy.deadline_ms > now ? policy.deadline_ms - now : 0));
	packet[44] = (unsigned char) initiator;
	packet[45] = (unsigned char) automatic_campaign;
	packet[46] = (unsigned char) rollback_required;
	packet[47] = (unsigned char) physical_mode;
	put64(packet + 48, kind == STATE ? 0 : local_nonce);
	if (kind == PORTABLE) memcpy(packet + 56, &portable[Player_num], COOP_PORTABLE_BYTES);
	else {
		put32(packet + 56, (uint32_t) ((kind == REQUEST ? physical_pending : physical_accepted) + 1));
		put32(packet + 60, arrival_checksum);
		put32(packet + 64, frozen_received);
		packet[68] = (unsigned char) freeze_ready;
		if (kind == REQUEST) {
			/* Immutable crossing context; ship position packets may overtake this request */
			put32(packet + 72, (uint32_t) (physical_segment + 1));
			put32(packet + 76, (uint32_t) (physical_side + 1));
			put32(packet + 80, physical_life);
			put32(packet + 84, physical_restore_serial);
		}
	}
	if (kind == REQUEST && test_request_delay_until && now < test_request_delay_until) {
		if (!test_request_buffered) {
			memcpy(test_delayed_request, packet, sizeof(packet));
			test_request_buffered = 1;
			COOPLOG("travel test buffered physical request: trigger=%d generation=%llu", physical_pending,
			        (unsigned long long) policy.generation);
		}
		return;
	}
	if (kind == STATE) {
		for (int i = 0; i < N_players; ++i)
			if (i != host && (policy.participants & bit(i))) {
				put64(packet + 48, peer_nonce[i]);
				multi_send_data_direct(packet, sizeof(packet), i, 0);
			}
		if (Netgame.numobservers) {
			put64(packet + 48, 0);
			multi_send_data_direct(packet, sizeof(packet), host, 0);
		}
	} else multi_send_data_direct(packet, sizeof(packet), host, 0);
}

int coop_travel_arm_campaign(void)
{
	if (!coop_travel_arm()) return 0;
	automatic_campaign = 1;
	return 1;
}

static int defer_reactor_departure(void)
{
#ifdef DXX_BUILD_DESCENT_II
	const coop_campaign *campaign = coop_campaign_current();
	return physical_enabled() && Control_center_destroyed && !Endlevel_sequence &&
	       coop_campaign_valid(campaign) && campaign->active_level == Current_level_num &&
	       !(armed && source_level == Current_level_num && known && policy.phase == COOP_PHASE_NORMAL_WAIT);
#else
	return 0;
#endif
}

int coop_travel_defer_player_death(int player)
{
#ifdef DXX_BUILD_DESCENT_II
	if (!physical_enabled() || !bit(player) || player >= N_players || is_observer()) return 0;
	if ((!armed || source_level != Current_level_num || campaign_generation != coop_campaign_current()->generation) &&
	    !coop_travel_arm_campaign()) return 0;
	dead_players |= bit(player);
	physical_mode = automatic_campaign = 1;
	return defer_reactor_departure();
#else
	(void) player;
	return 0;
#endif
}

int coop_travel_defer_death_screen(void)
{
#ifdef DXX_BUILD_DESCENT_II
	if (is_observer() || !defer_reactor_departure()) return 0;
	if ((!armed || source_level != Current_level_num || campaign_generation != coop_campaign_current()->generation) &&
	    !coop_travel_arm_campaign()) return 0;
	physical_mode = automatic_campaign = 1;
	if (!death_waiting) {
		death_waiting = 1;
		COOPLOG("reactor death waiting for team: player=%d level=%d", Player_num, Current_level_num);
	}
	return 1;
#else
	return 0;
#endif
}

void coop_travel_player_reappeared(int player)
{
	dead_players &= ~bit(player);
	if (player == Player_num) death_waiting = 0;
}

int coop_travel_waiting_after_death(void)
{
	return death_waiting && Player_is_dead && armed && source_level == Current_level_num;
}

int coop_travel_test_delay_request(int verify)
{
	if (verify) {
		if (test_request_sent != 1 || test_request_buffered || physical_pending >= 0 || !known) return 0;
		if (test_delayed_request[2] == COOP_OP_NORMAL_EXIT)
			return policy.operation == COOP_OP_SECRET_ENTER && !normal_granted && !normal_taken;
		return policy.phase == COOP_PHASE_NORMAL_WAIT;
	}
	if (!armed || Player_num == host || physical_pending < 0 || test_request_delay_until) return 0;
	test_request_delay_until = now_ms() + 8000;
	test_request_buffered = test_request_sent = 0;
	return 1;
}

int coop_travel_campaign_stage_allowed(void)
{
	return armed && known && pause_owned && policy.phase == COOP_PHASE_CAPTURING;
}

static int frozen_roster_valid(const unsigned char *bytes, size_t size, int checkpoint)
{
	if (!bytes || size <= PREPARED_HEADER || !coop_travel_campaign_stage_allowed() ||
	    get32(bytes) != game_id || get32(bytes + 4) != recovery_epoch ||
	    get64(bytes + 8) != policy.generation ||
	    !bytes[16] || (bytes[16] & ~operation_players) ||
	    !(bytes[16] & bit(host)) || (!is_observer() && !(bytes[16] & bit(Player_num))) || bytes[17] != checkpoint) return 0;
	if (bytes[18] & ~(checkpoint ? 0u : 2u)) return 0;
	for (int i = 19; i < 24; ++i)
		if (bytes[i]) return 0;
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		coop_portable_player record;
		memcpy(&record, bytes + 24 + i * COOP_PORTABLE_BYTES, sizeof(record));
		if (bytes[16] & bit(i)) {
			if (!coop_portable_valid(&record)) return 0;
			/* The host must echo the player's immutable frozen snapshot */
			if (i == Player_num && (!(portable_received & bit(i)) || memcmp(&record, &portable[i], sizeof(record)))) return 0;
		} else {
			coop_portable_player empty = { 0 };
			if (memcmp(&record, &empty, sizeof(record))) return 0;
		}
	}
	return 1;
}

static int checkpoint_file_matches(const char *path, const void *data, size_t size)
{
	unsigned char chunk[4096];
	const unsigned char *bytes = (const unsigned char *) data;
	size_t offset = 0;
	PHYSFS_file *file = PHYSFS_openRead(path);
	int valid = file && PHYSFS_fileLength(file) == (PHYSFS_sint64) size;
	while (valid && offset < size) {
		size_t count = size - offset < sizeof(chunk) ? size - offset : sizeof(chunk);
		valid = PHYSFS_read(file, chunk, 1, (PHYSFS_uint32) count) == (PHYSFS_sint64) count &&
		        !memcmp(chunk, bytes + offset, count);
		offset += count;
	}
	if (file && !PHYSFS_close(file)) valid = 0;
	return valid;
}

static int sync_checkpoint_path(const char *path, int parent)
{
	char absolute[PATH_MAX];
	int fd, result;
	if (!PHYSFSX_getRealPath(path, absolute)) return 0;
	if (parent) {
		char *slash = strrchr(absolute, '/');
		if (!slash || slash == absolute) return 0;
		*slash = 0;
	}
	fd = open(absolute, O_RDONLY | (parent ? O_DIRECTORY : 0));
	if (fd < 0) return 0;
	result = fsync(fd) == 0;
	if (close(fd)) result = 0;
	return result;
}

static int persist_checkpoint(const void *data, size_t size)
{
	char path[PATH_MAX], temporary[PATH_MAX];
	PHYSFS_file *file;
	int written;
	if (!state_android_build_coop_sidecar_filename(path, sizeof(path), "secret_travel_source.chk") ||
	    snprintf(temporary, sizeof(temporary), "%s.tmp", path) >= (int) sizeof(temporary)) return 0;
	state_android_ensure_parent_dirs_for_path(path);
	file = PHYSFS_openWrite(temporary);
	if (!file) return 0;
	written = PHYSFS_write(file, data, 1, (PHYSFS_uint32) size) == (PHYSFS_sint64) size;
	if (!PHYSFS_close(file)) written = 0;
	/* Publish only a closed, fully reread copy; preserve the previous checkpoint
	 * on write/validation failure. Android rename replaces the target atomically */
	if (!written || !sync_checkpoint_path(temporary, 0) || !checkpoint_file_matches(temporary, data, size) ||
	    !PHYSFSX_rename(temporary, path)) {
		PHYSFS_delete(temporary);
		return 0;
	}
	return sync_checkpoint_path(path, 1);
}

int coop_travel_verify_checkpoint_file(void)
{
	char path[PATH_MAX];
	return checkpoint_ready && source_checkpoint.data &&
	       state_android_build_coop_sidecar_filename(path, sizeof(path), "secret_travel_source.chk") &&
	       checkpoint_file_matches(path, source_checkpoint.data, source_checkpoint.size);
}

static uint32_t source_checkpoint_checksum(const unsigned char *bytes, size_t size)
{
	return coop_save_checksum(bytes + CHECKPOINT_HEADER, size - CHECKPOINT_HEADER,
	                          coop_save_checksum(bytes, PREPARED_HEADER + 4, 2166136261u));
}

int coop_travel_stage_checkpoint(const void *data, size_t size)
{
	const unsigned char *bytes = (const unsigned char *) data;
	rewind_memory_buffer save = { 0 };
	coop_save_metadata meta;
	unsigned matched = 0;
	unsigned char *copy;
	if (size <= CHECKPOINT_HEADER || size > MULTI_SAVE_TRANSFER_MAX_BYTES ||
	    !frozen_roster_valid(bytes, size, 1) || get32(bytes + PREPARED_HEADER) != size - CHECKPOINT_HEADER ||
	    get32(bytes + PREPARED_HEADER + 4) != source_checkpoint_checksum(bytes, size)) return 0;
	save.data = (unsigned char *) bytes + CHECKPOINT_HEADER;
	save.size = save.capacity = size - CHECKPOINT_HEADER;
	if (!coop_source_checkpoint_metadata(&save, source_level, campaign_generation, &meta)) return 0;
	for (unsigned i = 0; i < meta.num_active_players; ++i) {
		const coop_player_record *record = &meta.active_players[i];
		unsigned slot = record->original_slot;
		if (slot >= MAX_PLAYERS || !(bytes[16] & bit(slot)) || (matched & bit(slot)) ||
		    strncmp(record->callsign, Players[slot].callsign, sizeof(record->callsign)) ||
		    strncmp(record->client_id, Netgame.players[slot].client_id, sizeof(record->client_id))) return 0;
		matched |= bit(slot);
	}
	if (matched != bytes[16]) return 0;
	copy = (unsigned char *) malloc(size);
	if (!copy) return 0;
	memcpy(copy, bytes, size);
	if (!persist_checkpoint(copy, size)) {
		free(copy);
		return 0;
	}
	rewind_memory_buffer_discard(&source_checkpoint);
	source_checkpoint.data = copy;
	source_checkpoint.size = source_checkpoint.capacity = size;
	checkpoint_checksum = get32(copy + PREPARED_HEADER + 4);
	checkpoint_ready = 1;
	memcpy(portable, copy + 24, sizeof(portable));
	snapshot_players = portable_received = bytes[16];
	COOPLOG("travel source checkpoint retained: player=%d level=%d bytes=%u checksum=%u generation=%llu",
	        Player_num, source_level, (unsigned) size, checkpoint_checksum, (unsigned long long) policy.generation);
	return 1;
}

void coop_travel_get_checkpoint_state(int *ready, uint32_t *checksum, size_t *size)
{
	if (ready) *ready = checkpoint_ready;
	if (checksum) *checksum = checkpoint_checksum;
	if (size) *size = source_checkpoint.size;
}

int coop_travel_rollback_allowed(void)
{
	return armed && known && pause_owned && rollback_required && policy.phase == COOP_PHASE_RECOVERING;
}

void coop_travel_get_rollback_state(int *required, int *complete, int *failed_level)
{
	if (required) *required = rollback_required;
	if (complete) *complete = restored;
	if (failed_level) *failed_level = failed_destination;
}

void coop_travel_test_fail_after_load(void)
{
	fail_after_load = 1;
}

int coop_travel_restore_source(const void *data, size_t size)
{
	rewind_memory_buffer save = { 0 };
	coop_save_metadata meta;
	if (!coop_travel_rollback_allowed() || !checkpoint_ready || !data ||
	    size != source_checkpoint.size || memcmp(data, source_checkpoint.data, size) ||
	    get64(source_checkpoint.data + 8) + 1 != policy.generation ||
	    !coop_travel_verify_checkpoint_file()) return 0;
	save.data = source_checkpoint.data + CHECKPOINT_HEADER;
	save.size = save.capacity = size - CHECKPOINT_HEADER;
	if (!coop_source_checkpoint_metadata(&save, source_level, campaign_generation, &meta) ||
	    !coop_source_checkpoint_restore(&save, source_level, campaign_generation) || Current_level_num != source_level ||
	    coop_campaign_current()->generation != campaign_generation || !game_is_time_paused()) return 0;
	/* The full restore has reinstated the source ownership ledger. Apply the
	 * captured private fields after that restore, never over destination state */
	for (int i = 0; i < MAX_PLAYERS; ++i) {
		if (!(policy.participants & bit(i)) || Players[i].connected == CONNECT_DISCONNECTED) continue;
		if (!coop_apply_portable(i, &portable[i])) return 0;
		Netgame.killed[i] = Players[i].net_killed_total;
		Netgame.player_score[i] = Players[i].score;
		Netgame.player_flags[i] = Players[i].flags;
		COOPLOG("travel rollback player restored: player=%d score=%d", i, Players[i].score);
	}
	restored = 1;
	arrivals_placed = 0;
	arrival_checksum = 0;
	failure_pending = 0;
	COOPLOG("travel source restored: player=%d level=%d checkpoint=%u generation=%llu",
	        Player_num, Current_level_num, checkpoint_checksum, (unsigned long long) policy.generation);
	return 1;
}

static int capture_source_checkpoint(void)
{
	rewind_memory_buffer save = { 0 }, package = { 0 };
	int result;
	if ((portable_received & policy.participants) != policy.participants || !coop_travel_apply_portable()) return 0;
	portable_received &= policy.participants;
	for (int i = 0; i < MAX_PLAYERS; ++i)
		if (!(policy.participants & bit(i))) memset(&portable[i], 0, sizeof(portable[i]));
	/* The save serializer balances this additional pause */
	stop_time();
	result = state_save_to_memory(&save, "Secret travel source", ANDROID_SAVE_META_KIND_MANUAL, 1);
	if (!result || save.size > MULTI_SAVE_TRANSFER_MAX_BYTES - CHECKPOINT_HEADER) {
		result = 0;
		goto done;
	}
	package.size = package.capacity = CHECKPOINT_HEADER + save.size;
	package.data = (unsigned char *) calloc(1, package.size);
	if (!package.data) {
		result = 0;
		goto done;
	}
	put32(package.data, game_id);
	put32(package.data + 4, recovery_epoch);
	put64(package.data + 8, policy.generation);
	package.data[16] = policy.participants;
	package.data[17] = 1;
	memcpy(package.data + 24, portable, sizeof(portable));
	put32(package.data + PREPARED_HEADER, (uint32_t) save.size);
	memcpy(package.data + CHECKPOINT_HEADER, save.data, save.size);
	put32(package.data + PREPARED_HEADER + 4, source_checkpoint_checksum(package.data, package.size));
	result = multi_send_coop_checkpoint_transfer(&package);
done:
	rewind_memory_buffer_discard(&save);
	rewind_memory_buffer_discard(&package);
	return result;
}

int coop_travel_stage_campaign(const void *data, size_t size)
{
	const unsigned char *bytes = (const unsigned char *) data;
	coop_campaign_travel next = { 0 };
	if (!checkpoint_ready || size <= CAMPAIGN_HEADER || !frozen_roster_valid(bytes, size, 0) ||
	    memcmp(portable, bytes + 24, sizeof(portable))) return 0;
	if (!coop_campaign_travel_decode(&next, bytes + CAMPAIGN_HEADER, size - CAMPAIGN_HEADER)) return 0;
	const uint64_t ending_visit = get64(bytes + PREPARED_HEADER);
	if (next.source_level != source_level || next.source_generation != campaign_generation ||
	    strcmp(next.mission, coop_campaign_current()->mission) ||
	    !coop_validate_secret_travel(&next) ||
	    (next.action == COOP_CAMPAIGN_ENDGAME ? ending_visit <= coop_world_visit_current() : ending_visit != 0) ||
	    (policy.operation == COOP_OP_SECRET_ENTER ? next.action != COOP_CAMPAIGN_ENTER : next.action != COOP_CAMPAIGN_RETURN && next.action != COOP_CAMPAIGN_ADVANCE && next.action != COOP_CAMPAIGN_ENDGAME)) {
		coop_campaign_travel_clear(&next);
		return 0;
	}
	coop_campaign_travel_clear(&prepared_campaign);
	prepared_campaign = next;
	memcpy(portable, bytes + 24, sizeof(portable));
	snapshot_players = portable_received = bytes[16];
	prepared_checksum = coop_save_checksum(bytes, size, 2166136261u);
	prepared_destination = next.destination.level;
	prepared_action = next.action;
	advancement_briefing_flags = bytes[18];
	terminal_visit = ending_visit;
	if (ending_visit) coop_world_visit_observe(ending_visit);
	prepared_ready = automatic_campaign = 1;
	COOPLOG("travel campaign staged: player=%d destination=%d bytes=%u checksum=%u generation=%llu",
	        Player_num, prepared_destination, (unsigned) size, prepared_checksum, (unsigned long long) policy.generation);
	return 1;
}

static int prepare_campaign_transfer(void)
{
	coop_campaign_travel next = { 0 };
	unsigned char *encoded = NULL;
	size_t size = 0;
	rewind_memory_buffer buffer = { 0 };
	int result = 0;
	uint64_t ending_visit = 0;
	if ((portable_received & policy.participants) != policy.participants || !coop_travel_apply_portable() ||
	    !coop_prepare_secret_travel(&next) ||
	    (next.action != COOP_CAMPAIGN_ENTER && next.action != COOP_CAMPAIGN_RETURN && next.action != COOP_CAMPAIGN_ADVANCE && next.action != COOP_CAMPAIGN_ENDGAME) ||
	    !coop_campaign_travel_encode(&next, &encoded, &size)) goto done;
	if (next.action == COOP_CAMPAIGN_ENDGAME && !(ending_visit = coop_world_visit_reserve())) goto done;
	buffer.data = (unsigned char *) calloc(1, size + CAMPAIGN_HEADER);
	if (!buffer.data) goto done;
	buffer.size = buffer.capacity = size + CAMPAIGN_HEADER;
	put32(buffer.data, game_id);
	put32(buffer.data + 4, recovery_epoch);
	put64(buffer.data + 8, policy.generation);
	buffer.data[16] = snapshot_players;
#ifdef DXX_BUILD_DESCENT_II
	/* Carry the host's empty-intro decision in the existing campaign transfer */
	if (next.action == COOP_CAMPAIGN_ADVANCE && Netgame.CoopBriefings &&
	    !coop_briefing_count_intro(ShowLevelIntro, next.destination.level)) buffer.data[18] = 2;
#endif
	memcpy(buffer.data + 24, portable, sizeof(portable));
	put64(buffer.data + PREPARED_HEADER, ending_visit);
	memcpy(buffer.data + CAMPAIGN_HEADER, encoded, size);
	result = multi_send_coop_campaign_transfer(&buffer);
done:
	free(buffer.data);
	free(encoded);
	coop_campaign_travel_clear(&next);
	return result;
}

static int load_prepared_campaign(void)
{
	rewind_memory_buffer destination = { 0 };
	if (prepared_campaign.destination.size) {
		destination.data = prepared_campaign.destination.data;
		destination.size = destination.capacity = prepared_campaign.destination.size;
		return multi_send_coop_world_restore_transfer(&destination);
	}
	return (prepared_campaign.action == COOP_CAMPAIGN_ENTER || prepared_campaign.action == COOP_CAMPAIGN_ADVANCE) &&
	       multi_send_coop_world_initialize_transfer(prepared_campaign.destination.level);
}

int coop_travel_fresh_destination_allowed(int level)
{
	return coop_travel_world_apply_allowed() && automatic_campaign && prepared_ready &&
	       level == prepared_destination && !prepared_campaign.destination.size &&
	       ((prepared_action == COOP_CAMPAIGN_ENTER && level < 0) ||
	        (prepared_action == COOP_CAMPAIGN_ADVANCE && level > 0));
}

void coop_travel_get_campaign_state(int *ready, uint32_t *checksum, int *destination)
{
	if (ready) *ready = prepared_ready;
	if (checksum) *checksum = prepared_checksum;
	if (destination) *destination = prepared_destination;
}

int coop_travel_apply_portable(void)
{
	if (!armed || !automatic_campaign) return 1;
	if (!known || !pause_owned || (portable_received & policy.participants) != policy.participants ||
	    (policy.phase != COOP_PHASE_CAPTURING && policy.phase != COOP_PHASE_LOADING)) return 0;
	/* Do not roll a newer death/drop or inventory repair back to the captured
	 * life. Check the complete roster before changing any player's fields */
	for (int i = 0; i < MAX_PLAYERS; ++i)
		if ((policy.participants & bit(i)) && Players[i].connected != CONNECT_DISCONNECTED &&
		    !coop_recovery_accept_ship_status(i, portable[i].restore_serial, portable[i].life)) {
			COOPLOG("travel portable rejected revision: player=%d life=%u/%u serial=%u/%u", i,
			        portable[i].life, coop_recovery_life(i), portable[i].restore_serial, coop_recovery_restore_serial(i));
			return 0;
		}
	for (int i = 0; i < MAX_PLAYERS; ++i)
		if ((policy.participants & bit(i)) && Players[i].connected != CONNECT_DISCONNECTED && !coop_apply_portable(i, &portable[i])) {
			COOPLOG("travel portable rejected player: player=%d connected=%d object=%d paused=%d", i,
			        Players[i].connected, Players[i].objnum, game_is_time_paused());
			return 0;
		}
	return 1;
}

void coop_travel_get_portable_state(unsigned *received, uint32_t *checksum)
{
	if (received) *received = portable_received;
	if (checksum) *checksum = portable_received ? coop_save_checksum(portable, sizeof(portable), 2166136261u) : 0;
}

static int accept_request(coop_operation operation, int sender, uint64_t generation, int team_dead)
{
	if (team_dead && (Player_num != host || sender != host || !defer_reactor_departure() ||
	                  (dead_players & policy.participants) != policy.participants ||
	                  operation != (source_level < 0 ? COOP_OP_SECRET_RETURN : COOP_OP_NORMAL_EXIT))) return 0;
	if (Current_level_num != source_level || coop_campaign_current()->generation != campaign_generation ||
	    coop_recovery_epoch() != recovery_epoch ||
	    sender < 0 || sender >= N_players || !(policy.participants & bit(sender)) ||
	    Players[sender].connected != CONNECT_PLAYING || (!team_dead && ((dead_players & bit(sender)) || Players[sender].shields <= 0)) ||
	    operation < COOP_OP_NORMAL_EXIT || operation > COOP_OP_SECRET_RETURN ||
	    (operation == COOP_OP_SECRET_RETURN ? source_level >= 0 : source_level <= 0) ||
	    (operation == COOP_OP_SECRET_ENTER && !coop_secret_entry_available())) return 0;
	if (operation == COOP_OP_NORMAL_EXIT && coop_transition_normal_exit_allowed(&policy, generation, sender)) {
		normal_granted |= bit(sender);
		return 1;
	}
	if (coop_travel_active() || coop_briefing_active() || multi_save_transfer_busy() ||
	    !coop_transition_begin(&policy, generation, operation, sender, now_ms())) return 0;
	initiator = sender;
	operation_players = policy.participants;
	snapshot_players = 0;
	normal_granted = operation == COOP_OP_NORMAL_EXIT && !team_dead ? bit(sender) : 0;
	normal_taken = 0;
	world_changed = applied = committed = restored = 0;
	prepare_started = load_started = prepared_ready = prepared_destination = 0;
	prepared_action = advancement_presented = 0;
	prepared_checksum = 0;
	portable_received = 0;
	frozen_received = deaths_settled = 0;
	freeze_ready = 0;
	memset(portable, 0, sizeof(portable));
	clear_checkpoint();
	failure_pending = rollback_required = rollback_started = recovery_entered = 0;
	failed_destination = 0;
	coop_campaign_travel_clear(&prepared_campaign);
	release_needed = operation != COOP_OP_NORMAL_EXIT;
	release_acked = 0;
	COOPLOG("travel exit accepted: operation=%d initiator=%d level=%d generation=%llu",
	        operation, sender, source_level, (unsigned long long) policy.generation);
	return 1;
}

int coop_travel_request(coop_operation operation)
{
	if (!armed || !known || is_observer() || Player_is_dead ||
	    operation < COOP_OP_NORMAL_EXIT || operation > COOP_OP_SECRET_RETURN) return 0;
	if (Player_num == host) return accept_request(operation, Player_num, policy.generation, 0);
	if (policy.phase != COOP_PHASE_SETTLED &&
	    !(operation == COOP_OP_NORMAL_EXIT && policy.phase == COOP_PHASE_NORMAL_WAIT)) return 0;
	requested = operation;
	last_request = 0;
	return 1;
}

int coop_travel_take_normal_exit(void)
{
	if (!armed || !known || policy.phase != COOP_PHASE_NORMAL_WAIT || normal_taken ||
	    !(normal_granted & bit(Player_num))) return 0;
	normal_taken = 1;
	return 1;
}

static int accept_physical(int trigger_num, int sender, uint64_t generation, int crossing_segment, int crossing_side, uint32_t life, uint32_t restore_serial)
{
#ifdef DXX_BUILD_DESCENT_II
	coop_operation operation = physical_operation(trigger_num);
	int valid_crossing = 0;
	if (!physical_enabled() || !operation || sender < 0 || sender >= N_players) return 0;
	if (life != coop_recovery_life(sender) || restore_serial != coop_recovery_restore_serial(sender)) return 0;
	if (arrival_trigger_blocked(trigger_num, sender)) return 0;
	const int objnum = Players[sender].objnum;
	if (objnum < 0 || objnum > Highest_object_index || Objects[objnum].type != OBJ_PLAYER) return 0;
	if ((Triggers[trigger_num].flags & TF_DISABLED) &&
	    !(operation == COOP_OP_NORMAL_EXIT && policy.phase == COOP_PHASE_NORMAL_WAIT)) return 0;
	for (int i = 0; i < Num_walls; ++i) {
		if (Walls[i].trigger != trigger_num || Walls[i].segnum < 0 || Walls[i].segnum > Highest_segment_index ||
		    Walls[i].sidenum < 0 || Walls[i].sidenum >= 6) continue;
		if (crossing_segment == Walls[i].segnum && crossing_side == Walls[i].sidenum) valid_crossing = 1;
	}
	if (!valid_crossing || !accept_request(operation, sender, generation, 0)) {
		COOPLOG("physical exit rejected: trigger=%d player=%d operation=%d valid_crossing=%d phase=%d epoch=%u/%u generation=%llu/%llu segment=%d pos=%d,%d,%d",
		        trigger_num, sender, operation, valid_crossing, policy.phase, coop_recovery_epoch(), recovery_epoch,
		        (unsigned long long) generation, (unsigned long long) policy.generation,
		        Objects[objnum].segnum, Objects[objnum].pos.x, Objects[objnum].pos.y, Objects[objnum].pos.z);
		return 0;
	}
	if (physical_accepted < 0 || operation != COOP_OP_NORMAL_EXIT) physical_accepted = trigger_num;
	COOPLOG("physical exit granted: trigger=%d player=%d operation=%d crossing=%d:%d current_segment=%d",
	        trigger_num, sender, operation, crossing_segment, crossing_side, Objects[objnum].segnum);
	return 1;
#else
	(void) trigger_num;
	(void) sender;
	(void) generation;
	(void) crossing_segment;
	(void) crossing_side;
	(void) life;
	(void) restore_serial;
	return 0;
#endif
}

int coop_travel_handle_exit_trigger(int trigger_num, int pnum, int shot)
{
	return coop_travel_handle_exit_crossing(trigger_num, pnum, shot, -1, -1);
}

int coop_travel_handle_exit_crossing(int trigger_num, int pnum, int shot, int crossing_segment, int crossing_side)
{
#ifdef DXX_BUILD_DESCENT_II
	coop_operation operation = physical_operation(trigger_num);
	if (!physical_enabled() || !operation) return 0;
	if (physical_replay == trigger_num && pnum == Player_num) {
		physical_replay = -1;
		return 0;
	}
	/* Legacy trigger broadcasts and Guide-Bot shots cannot authorize travel */
	if (pnum != Player_num || shot || is_observer() || Player_is_dead) return 1;
	/* Only the physics crossing callback supplies a wall; legacy broadcasts do not */
	if (crossing_segment < 0 || crossing_segment > Highest_segment_index || crossing_side < 0 || crossing_side >= 6) return 1;
	if (operation == COOP_OP_SECRET_ENTER && !coop_secret_entry_available()) {
		HUD_init_message(HM_DEFAULT, "Secret area unavailable");
		return 1;
	}
	if (arrival_trigger_blocked(trigger_num, pnum)) return 1;
	if ((Triggers[trigger_num].flags & TF_DISABLED) &&
	    !(operation == COOP_OP_NORMAL_EXIT && known && policy.phase == COOP_PHASE_NORMAL_WAIT)) return 1;
	if (!armed || source_level != Current_level_num || campaign_generation != coop_campaign_current()->generation ||
	    (!coop_travel_active() && local_recovery_epoch != coop_recovery_epoch())) {
		if (!coop_travel_arm_campaign()) return 1;
	}
	physical_mode = 1;
	if (known && policy.phase != COOP_PHASE_SETTLED &&
	    !(operation == COOP_OP_NORMAL_EXIT && policy.phase == COOP_PHASE_NORMAL_WAIT)) return 1;
	if (physical_pending >= 0) return 1;
	physical_pending = trigger_num;
	physical_segment = crossing_segment;
	physical_side = crossing_side;
	physical_life = coop_recovery_life(pnum);
	physical_restore_serial = coop_recovery_restore_serial(pnum);
	requested = operation;
	last_request = 0;
	log_physical_position("requested", trigger_num, pnum);
	game_flush_inputs();
	return 1;
#else
	(void) trigger_num;
	(void) pnum;
	(void) shot;
	(void) crossing_segment;
	(void) crossing_side;
	return 0;
#endif
}

void coop_travel_receive(const unsigned char *packet, int sender)
{
	uint64_t generation = get64(packet + 32), now = now_ms();
	if (packet[47] > 1) return;
	/* Establish the gate lazily, before either peer can take a local exit */
	if (packet[47] && physical_enabled() && sender >= 0 && sender < N_players && sender != Player_num &&
	    get32(packet + 8) == (uint32_t) Netgame.protocol.udp.GameID &&
	    (int32_t) get32(packet + 12) == Current_level_num &&
	    get64(packet + 16) == coop_campaign_current()->generation &&
	    ((multi_i_am_master() && (packet[1] == HELLO || packet[1] == REQUEST) && Players[sender].connected == CONNECT_PLAYING) ||
	     (sender == multi_who_is_master() && packet[1] == STATE))) {
		if (!armed || source_level != Current_level_num || campaign_generation != coop_campaign_current()->generation) {
			if (!coop_travel_arm_campaign()) return;
		}
		physical_mode = 1;
	}
	if (!armed || !(Game_mode & GM_MULTI_COOP) || get32(packet + 8) != game_id ||
	    (int32_t) get32(packet + 12) != source_level || get64(packet + 16) != campaign_generation ||
	    packet[6] != host || !bit(sender)) return;
	/* Fresh games can have different local recovery reset counts. Establish the
	 * host's epoch through a nonce echo, without changing the recovery ledger */
	if (Player_num == host && packet[1] == HELLO && (policy.participants & bit(sender)) &&
	    sender != host && get64(packet + 48)) {
		/* A peer can rearm before the host's next engine-frame arm call. Do not
		 * bind its new nonce to the completed rollback's old recovery epoch */
		if (rollback_required && policy.phase == COOP_PHASE_SETTLED && get64(packet + 48) > peer_nonce[sender]) {
			if (!coop_travel_arm_campaign()) return;
			COOPLOG("travel host renewed after rollback for player=%d", sender);
		}
		if (get64(packet + 48) >= peer_nonce[sender] &&
		    (!peer_nonce[sender] || peer_nonce[sender] == get64(packet + 48) || !coop_travel_active())) {
			peer_nonce[sender] = get64(packet + 48);
			send_packet(STATE);
		}
		return;
	}
	if (packet[1] == STATE && sender == host && Player_num != host) {
		if (!is_observer() && get64(packet + 48) != local_nonce) return;
		if (known && get32(packet + 24) != recovery_epoch) return;
	} else if (get32(packet + 24) != recovery_epoch || !peer_nonce[sender] ||
	           get64(packet + 48) != peer_nonce[sender]) return;
	if (packet[1] == STATE && sender == host && Player_num != host) {
		coop_transition_phase phase = (coop_transition_phase) packet[3];
		coop_operation operation = (coop_operation) packet[2];
		uint32_t next_revision = get32(packet + 28), remaining = get32(packet + 40);
		if (!generation || !next_revision || (known && (generation < policy.generation || next_revision <= revision)) ||
		    packet[45] > 1 || packet[46] > 1 || packet[68] > 1 || (get32(packet + 64) & ~packet[4]) ||
		    phase > COOP_PHASE_LOBBY || (phase >= COOP_PHASE_BRIEFING_PREPARE && phase <= COOP_PHASE_CLOSING_PRESENTATION) ||
		    operation > COOP_OP_SECRET_RETURN || !(packet[4] & bit(host)) ||
		    (!is_observer() && !(packet[4] & bit(Player_num))) || (packet[5] & ~packet[4]) ||
		    (packet[7] & ~packet[4]) || packet[44] >= MAX_PLAYERS || remaining > COOP_SECRET_TEST_WARNING_MS ||
		    (phase == COOP_PHASE_NORMAL_WAIT && operation != COOP_OP_NORMAL_EXIT)) return;
		if (generation != policy.generation && phase == COOP_PHASE_FREEZING) {
			operation_players = packet[4];
			snapshot_players = 0;
			world_changed = applied = committed = restored = prepared_ready = 0;
			prepared_action = advancement_presented = 0;
			prepared_checksum = 0;
			portable_received = 0;
			deaths_settled = 0;
			memset(portable, 0, sizeof(portable));
			clear_checkpoint();
			failure_pending = rollback_required = rollback_started = recovery_entered = 0;
			failed_destination = 0;
			coop_campaign_travel_clear(&prepared_campaign);
		}
		if (known && generation == policy.generation && (packet[4] & ~policy.participants)) return;
		/* Recovery advances the generation too; its roster is still authoritative */
		if (known)
			for (int i = 0; i < N_players; ++i)
				if ((policy.participants & ~packet[4]) & bit(i)) multi_disconnect_player(i);
		policy.phase = phase;
		frozen_received = get32(packet + 64);
		freeze_ready = packet[68];
		automatic_campaign = packet[45];
		physical_mode = packet[47];
		physical_accepted = packet_trigger(packet);
		rollback_required = packet[46];
		if (phase == COOP_PHASE_RECOVERING) failure_pending = 0;
		policy.operation = operation;
		policy.generation = generation;
		policy.participants = packet[4];
		policy.acknowledged = packet[5];
		policy.now_ms = now;
		policy.deadline_ms = remaining ? now + remaining : 0;
		normal_granted = packet[7];
		initiator = packet[44];
		revision = next_revision;
		known = 1;
		recovery_epoch = get32(packet + 24);
		last_host = now;
		if (phase != COOP_PHASE_SETTLED && phase != COOP_PHASE_NORMAL_WAIT) release_needed = 1;
		if (phase != COOP_PHASE_SETTLED &&
		    !(requested == COOP_OP_NORMAL_EXIT && phase == COOP_PHASE_NORMAL_WAIT &&
		      !(normal_granted & bit(Player_num)))) requested = COOP_OP_NONE;
		if (phase != COOP_PHASE_SETTLED &&
		    !(phase == COOP_PHASE_NORMAL_WAIT && physical_operation(physical_pending) == COOP_OP_NORMAL_EXIT)) physical_pending = -1;
	} else if (Player_num == host && (policy.participants & bit(sender))) {
		if (packet[1] == FROZEN && automatic_campaign && generation == policy.generation &&
		    policy.phase == COOP_PHASE_FREEZING && packet[3] == COOP_PHASE_FREEZING) {
			frozen_received |= bit(sender);
		} else if (packet[1] == PORTABLE && automatic_campaign && freeze_ready && generation == policy.generation &&
		           policy.phase == COOP_PHASE_FREEZING && packet[3] == COOP_PHASE_FREEZING) {
			coop_portable_player record;
			memcpy(&record, packet + 56, sizeof(record));
			if (!coop_portable_valid(&record) ||
			    !coop_recovery_accept_ship_status(sender, record.restore_serial, record.life)) return;
			if (!(portable_received & bit(sender))) {
				portable[sender] = record;
				portable_received |= bit(sender);
			}
		} else if (packet[1] == FAILED && generation == policy.generation && packet[3] == policy.phase &&
		           policy.phase >= COOP_PHASE_FREEZING && policy.phase <= COOP_PHASE_LOADING) {
			failure_pending = 1;
		} else if (packet[1] == REQUEST) {
			if (physical_mode) {
				int trigger_num = packet_trigger(packet);
				if (packet[47] && get32(packet + 72) > 0 && get32(packet + 72) <= (uint32_t) (Highest_segment_index + 1) &&
				    get32(packet + 76) > 0 && get32(packet + 76) <= 6 &&
				    physical_operation(trigger_num) == (coop_operation) packet[2])
					accept_physical(trigger_num, sender, generation, (int32_t) get32(packet + 72) - 1,
					                (int32_t) get32(packet + 76) - 1, get32(packet + 80), get32(packet + 84));
			} else accept_request((coop_operation) packet[2], sender, generation, 0);
			send_packet(STATE);
		} else if (packet[1] == ACK && generation == policy.generation && packet[3] == policy.phase) {
			if (automatic_campaign && policy.phase == COOP_PHASE_FREEZING && !(portable_received & bit(sender))) return;
			if (automatic_campaign && policy.phase == COOP_PHASE_LOADING) {
				if (!arrivals_placed) return;
				if (get32(packet + 60) != arrival_checksum) {
					COOPLOG("travel arrival disagreement: player=%d checksum=%u/%u", sender, get32(packet + 60), arrival_checksum);
					failure_pending = 1;
					return;
				}
			}
			if (policy.phase == COOP_PHASE_SETTLED) release_acked |= bit(sender);
			else coop_transition_ack(&policy, generation, policy.phase, sender, now);
		}
	}
}

void coop_travel_network_frame(void)
{
	uint64_t now = now_ms();
#ifdef DXX_BUILD_DESCENT_II
	/* Rearm only after leaving both segments touching an arrival exit */
	for (int t = 0; t < Num_triggers; ++t)
		if (arrival_guard[t])
			for (int p = 0; p < N_players; ++p) arrival_trigger_blocked(t, p);
#endif
	if (!armed) return;
	/* A respawn or restore requires a new crossing */
	if (physical_pending >= 0 &&
	    (physical_life != coop_recovery_life(Player_num) || physical_restore_serial != coop_recovery_restore_serial(Player_num))) {
		physical_pending = -1;
		requested = COOP_OP_NONE;
	}
	/* Fault injection preserves the original envelope after a competing decision */
	if (test_request_buffered && now >= test_request_delay_until) {
		multi_send_data_direct(test_delayed_request, sizeof(test_delayed_request), host, 0);
		test_request_buffered = 0;
		test_request_delay_until = 0;
		++test_request_sent;
		COOPLOG("travel test delivered delayed physical request: phase=%d generation=%llu", policy.phase,
		        (unsigned long long) get64(test_delayed_request + 32));
	}
	if (Player_num == host && physical_pending >= 0 && known && policy.phase != COOP_PHASE_SETTLED &&
	    !(policy.phase == COOP_PHASE_NORMAL_WAIT && physical_operation(physical_pending) == COOP_OP_NORMAL_EXIT)) {
		physical_pending = -1;
		requested = COOP_OP_NONE;
	}
	if (Player_num == host && physical_pending >= 0 && requested && known) {
		if (accept_physical(physical_pending, Player_num, policy.generation, physical_segment, physical_side, physical_life, physical_restore_serial)) {
			requested = COOP_OP_NONE;
			if (policy.phase != COOP_PHASE_NORMAL_WAIT) physical_pending = -1;
		}
	}
	if (Player_num != host && !known && now >= last_request + 250) {
		last_request = now;
		send_packet(HELLO);
	}
	if (Player_num == host && now >= last_send + 250) {
		last_send = now;
		send_packet(STATE);
	} else if (Player_num != host && requested && known && now >= last_request + 250) {
		last_request = now;
		send_packet(REQUEST);
	}
	if (Player_num != host && failure_pending && known && policy.phase != COOP_PHASE_RECOVERING &&
	    policy.phase != COOP_PHASE_SETTLED && now >= last_request + 250) {
		last_request = now;
		send_packet(FAILED);
	}
	/* Release retries continue after gameplay resumes */
	if (known && policy.phase == COOP_PHASE_SETTLED && release_needed && now >= last_ack + 100) {
		last_ack = now;
		release_acked |= bit(Player_num);
		if (Player_num != host && !is_observer()) send_packet(ACK);
	}
}

int coop_travel_source_captured(void)
{
	return armed && known && Player_num == host && pause_owned &&
	       coop_transition_source_captured(&policy, policy.generation, now_ms());
}
void coop_travel_destination_started(void)
{
	if (armed && policy.phase == COOP_PHASE_LOADING) world_changed = 1;
}
void coop_travel_destination_applied(void)
{
	if (armed && world_changed && policy.phase == COOP_PHASE_LOADING && prepared_ready) {
		const coop_campaign_world *source = coop_campaign_find_world(&prepared_campaign.next, source_level);
		/* Every peer applies this from the same prepared campaign, before ACK.
		 * Do not broadcast new ledger revisions until the operation commits */
		if ((!source || source->state == COOP_CAMPAIGN_DESTROYED) && !coop_recovery_retire_world(source_level)) {
			coop_travel_world_failed();
			return;
		}
	}
	if (armed && world_changed && policy.phase == COOP_PHASE_LOADING && fail_after_load) {
		fail_after_load = 0;
		failed_destination = Current_level_num;
		Players[Player_num].connected = Netgame.players[Player_num].connected = CONNECT_WAITING;
		coop_travel_world_failed();
		return;
	}
	if (armed && world_changed && policy.phase == COOP_PHASE_LOADING) {
		if (!place_arrivals()) {
			coop_travel_world_failed();
			return;
		}
		applied = 1;
	}
}
int coop_travel_transfer_buffer_allowed(int kind)
{
	switch (kind) {
		case MULTI_SAVE_TRANSFER_KIND_WORLD:
		case MULTI_SAVE_TRANSFER_KIND_FRESH_WORLD:
			/* The last preparation ACK can let the host send the world before
			 * its LOADING snapshot reaches us. Buffer it once prepared; application
			 * still requires the authoritative LOADING phase */
			return !armed || (known && (policy.phase == COOP_PHASE_WARNING || policy.phase == COOP_PHASE_LOADING ||
			                            (policy.phase == COOP_PHASE_CAPTURING && prepared_ready)));
		case MULTI_SAVE_TRANSFER_KIND_CAMPAIGN:
		case MULTI_SAVE_TRANSFER_KIND_CHECKPOINT:
			return armed && known && (policy.phase == COOP_PHASE_FREEZING || policy.phase == COOP_PHASE_CAPTURING);
		case MULTI_SAVE_TRANSFER_KIND_ROLLBACK:
			return armed && known && checkpoint_ready && (policy.phase == COOP_PHASE_LOADING || policy.phase == COOP_PHASE_RECOVERING);
		default: return !coop_travel_active();
	}
}
int coop_travel_world_apply_allowed(void)
{
	/* An armed but settled gate can be the result of an abort. A delayed
	 * transfer must not become applicable merely because gameplay resumed */
	return !armed || (known && pause_owned && !failure_pending && policy.phase == COOP_PHASE_LOADING);
}

int coop_travel_settling_players(void)
{
	return armed && known && automatic_campaign && pause_owned && policy.phase == COOP_PHASE_FREEZING && !freeze_ready;
}

void coop_travel_get_freeze_state(unsigned *frozen, int *ready, unsigned *deaths)
{
	if (frozen) *frozen = frozen_received;
	if (ready) *ready = freeze_ready;
	if (deaths) *deaths = deaths_settled;
}
void coop_travel_world_failed(void)
{
	if (!coop_travel_blocks_gameplay()) return;
	COOPLOG("travel world application failed: player=%d phase=%d", Player_num, policy.phase);
	if (policy.phase >= COOP_PHASE_FREEZING && policy.phase <= COOP_PHASE_LOADING) {
		failure_pending = 1;
		return;
	}
	policy.phase = COOP_PHASE_LOBBY;
	multi_quit_game = 1;
}
void coop_travel_campaign_committed(void)
{
	if (armed && applied && policy.phase == COOP_PHASE_COMMITTED) committed = 1;
}
void coop_travel_source_restored(void)
{
	if (armed && policy.phase == COOP_PHASE_RECOVERING) restored = 1;
}
int coop_travel_abort(void)
{
	int needs_restore = automatic_campaign && policy.phase == COOP_PHASE_LOADING;
	if (!armed || !known || Player_num != host || !coop_transition_abort(&policy, policy.generation)) return 0;
	rollback_required = needs_restore;
#ifdef DXX_BUILD_DESCENT_II
	if (!needs_restore && physical_consumed && physical_accepted >= 0 && physical_accepted < Num_triggers) {
		Triggers[physical_accepted].flags = physical_original_flags;
		physical_consumed = 0;
	}
#endif
	failure_pending = 0;
	return 1;
}

int coop_travel_host_disconnected(int player)
{
	if (bit(player) && !coop_travel_blocks_gameplay()) peer_nonce[player] = 0;
	if (!coop_travel_blocks_gameplay() || player != host) return 0;
	/* Released campaign results are local; leaving cannot undo completion */
	if (coop_travel_ending_campaign() && !coop_travel_active()) {
		COOPLOG("host left completed campaign: level=%d", Current_level_num);
		return 1;
	}
	policy.phase = COOP_PHASE_LOBBY;
	multi_quit_game = 1;
	return 1;
}

int coop_travel_frame(void)
{
	uint64_t now;
	int can_ack = 0;
	if (!armed || pumping) return 0;
	/* The score window pumps the protocol; the underlying mine stays suspended */
	if (coop_travel_ending_campaign() && advancement_presented) return 1;
	pumping = 1;
#ifdef DXX_BUILD_DESCENT_II
	if (known && Player_num == host && policy.phase == COOP_PHASE_SETTLED && defer_reactor_departure() &&
	    (dead_players & policy.participants) == policy.participants) {
		if (accept_request(source_level < 0 ? COOP_OP_SECRET_RETURN : COOP_OP_NORMAL_EXIT, host, policy.generation, 1))
			COOPLOG("reactor team death departure: level=%d participants=%u operation=%d", source_level, policy.participants, policy.operation);
	}
	/* Once normal progression wins, dead players finish the ordinary wait path */
	if (known && policy.phase == COOP_PHASE_NORMAL_WAIT && Control_center_destroyed && Player_is_dead)
		Death_sequence_aborted = 1;
#endif
	if (coop_travel_blocks_gameplay()) {
		multi_do_protocol_frame(0, 1);
		coop_recovery_frame();
	}
	now = now_ms();
	if (known && observed_phase != policy.phase) {
		observed_phase = policy.phase;
		phase_since = now;
		COOPLOG("travel phase: phase=%d generation=%llu player=%d", policy.phase,
		        (unsigned long long) policy.generation, Player_num);
	}
	/* Automation fault injection: use the real disconnect path at a chosen barrier */
	if (known && Player_num == host && coop_travel_active() && test_disconnect_phase == (int) policy.phase &&
	    (policy.phase != COOP_PHASE_CAPTURING || (checkpoint_started && multi_save_transfer_busy())) &&
	    (policy.phase != COOP_PHASE_LOADING || load_started)) {
		test_disconnect_phase = -1;
		for (int i = 0; i < N_players; ++i)
			if (i != host && (policy.participants & bit(i))) {
				COOPLOG("travel test disconnect: player=%d phase=%d", i, policy.phase);
				multi_disconnect_player(i);
				break;
			}
	}
	if (known && Player_num == host && failure_pending && policy.phase != COOP_PHASE_RECOVERING)
		if (!coop_travel_abort()) coop_travel_world_failed();
	/* Cover departure between release and opening the local score screen too */
	if (coop_travel_blocks_gameplay() && !(coop_travel_ending_campaign() && !coop_travel_active())) {
		unsigned timeout = policy.phase == COOP_PHASE_LOADING || policy.phase == COOP_PHASE_RECOVERING ||
		                           policy.phase == COOP_PHASE_CAPTURING
		                       ? 180000
		                       : 30000;
		if (policy.phase == COOP_PHASE_CAPTURING || policy.phase == COOP_PHASE_RECOVERING)
			timeout = 2u * (unsigned) multi_save_transfer_limit_ms(
			                   (MULTI_SAVE_TRANSFER_MAX_BYTES + MULTI_REWIND_SAVE_CHUNK_PAYLOAD - 1) /
			                   MULTI_REWIND_SAVE_CHUNK_PAYLOAD) +
			          30000u;
		if (multi_quit_game || multi_who_is_master() != host || now > phase_since + timeout ||
		    (Player_num != host && now > last_host + timeout)) policy.phase = COOP_PHASE_LOBBY;
		if (Player_num == host)
			for (int i = 0; i < N_players; ++i)
				if (i != host && (policy.participants & bit(i)) && Players[i].connected == CONNECT_DISCONNECTED) {
					coop_transition_remove_player(&policy, i, now);
					frozen_received &= policy.participants;
					release_acked &= policy.participants;
					normal_granted &= policy.participants;
					peer_nonce[i] = 0;
					COOPLOG("travel client removed: player=%d phase=%d remaining=%u", i, policy.phase, policy.participants);
					send_packet(STATE);
				}
	}
	if (policy.phase == COOP_PHASE_LOBBY) {
		multi_quit_game = 1;
		pumping = 0;
		return -1;
	}
	if (coop_travel_blocks_gameplay() && !pause_owned) {
		stop_time();
		pause_owned = 1;
		game_flush_inputs();
	}
	if (coop_travel_settling_players()) {
		int ready = is_observer();
#ifdef DXX_BUILD_DESCENT_II
		if (!ready) {
			int result = coop_finish_death_for_travel();
			if (result == 2) ++deaths_settled;
			ready = result != 0;
		}
#endif
		if (!Netgame.PacketLossPrevention) failure_pending = 1;
		if (ready && coop_recovery_save_ready() && !net_udp_reliable_pending() && now >= last_ack + 100) {
			last_ack = now;
			if (Player_num == host) frozen_received |= bit(Player_num);
			else send_packet(FROZEN);
		}
		/* Every peer has settled its accepted death/drop and drained outgoing
		 * reliable traffic. Host kill confirmations must reach everyone too */
		if (Player_num == host && frozen_received == policy.participants && !net_udp_reliable_pending() &&
		    coop_recovery_save_ready() && !failure_pending) {
			freeze_ready = 1;
			last_send = 0;
			COOPLOG("travel freeze drained: generation=%llu participants=%u deaths=%u",
			        (unsigned long long) policy.generation, frozen_received, deaths_settled);
		}
	}
	/* GameProcessFrame is suspended while this gate owns the pause. Transfers
	 * still need their frame-boundary apply path, outside the UDP parser */
	if (known && policy.phase == COOP_PHASE_RECOVERING && !recovery_entered) {
		recovery_entered = 1;
		multi_cancel_coop_travel_transfer();
		if (rollback_required && !checkpoint_ready) coop_travel_world_failed();
	}
	if (coop_travel_rollback_allowed() && Player_num == host && !rollback_started) {
		rollback_started = 1;
		if (!multi_send_coop_rollback_transfer(&source_checkpoint)) coop_travel_world_failed();
	}
	if (automatic_campaign && Player_num == host && policy.phase == COOP_PHASE_CAPTURING && !checkpoint_started) {
		checkpoint_started = 1;
		if (!capture_source_checkpoint()) coop_travel_abort();
	}
	if (automatic_campaign && Player_num == host && policy.phase == COOP_PHASE_CAPTURING && checkpoint_ready && !prepare_started) {
#ifdef DXX_BUILD_DESCENT_II
		/* The rollback checkpoint precedes consuming an authored one-shot exit */
		if (physical_mode && physical_accepted >= 0 && physical_accepted < Num_triggers &&
		    (Triggers[physical_accepted].flags & TF_ONE_SHOT)) {
			physical_original_flags = Triggers[physical_accepted].flags;
			physical_consumed = 1;
			Triggers[physical_accepted].flags |= TF_DISABLED;
		}
#endif
		prepare_started = 1;
		if (!prepare_campaign_transfer()) coop_travel_abort();
	}
	if (known && (policy.phase == COOP_PHASE_CAPTURING || policy.phase == COOP_PHASE_LOADING ||
	              policy.phase == COOP_PHASE_RECOVERING)) multi_save_transfer_frame();
	if (automatic_campaign && policy.phase == COOP_PHASE_LOADING && prepared_ready && prepared_action == COOP_CAMPAIGN_ENDGAME) {
		/* There is no destination mine. Agree on the prepared terminal record */
		if (!applied && !failure_pending) {
			if (!coop_travel_apply_portable()) coop_travel_world_failed();
			else {
				load_started = arrivals_placed = applied = 1;
				arrival_checksum = prepared_checksum;
			}
		}
	} else if (automatic_campaign && Player_num == host && policy.phase == COOP_PHASE_LOADING && prepared_ready && !load_started) {
		load_started = 1;
		if (!load_prepared_campaign()) coop_travel_abort();
	}
	if (automatic_campaign && policy.phase == COOP_PHASE_COMMITTED && applied && !committed) {
		if (coop_commit_secret_travel(&prepared_campaign) &&
		    (prepared_action != COOP_CAMPAIGN_ENDGAME || coop_world_visit_activate(terminal_visit))) coop_travel_campaign_committed();
		else coop_travel_world_failed();
	}
	if (known && Player_num == host) coop_transition_tick(&policy, now);
	policy.now_ms = now;
	can_ack = policy.phase == COOP_PHASE_FREEZING ? pause_owned && (!automatic_campaign || freeze_ready) &&
	                                                    (is_observer() || (!Player_is_dead && Players[Player_num].shields > 0)) &&
	                                                    coop_recovery_save_ready()
	          : policy.phase == COOP_PHASE_CAPTURING  ? prepared_ready
	          : policy.phase == COOP_PHASE_LOADING    ? applied && !test_hold_arrival
	          : policy.phase == COOP_PHASE_COMMITTED  ? committed
	          : policy.phase == COOP_PHASE_RECOVERING ? (rollback_required ? restored : !world_changed)
	                                                  : 0;
	if (failure_pending) can_ack = 0;
	if (automatic_campaign && policy.phase == COOP_PHASE_FREEZING && can_ack && !(portable_received & bit(Player_num))) {
		if (coop_capture_portable(&portable[Player_num])) portable_received |= bit(Player_num);
		else can_ack = 0;
	}
	if (known && can_ack && (!is_observer() || Player_num == host) && now >= last_ack + 100) {
		last_ack = now;
		if (automatic_campaign && policy.phase == COOP_PHASE_FREEZING && Player_num != host) send_packet(PORTABLE);
		if (Player_num == host) coop_transition_ack(&policy, policy.generation, policy.phase, Player_num, now);
		else send_packet(ACK);
	}
	if (known && Player_num == host && policy.phase == COOP_PHASE_RECOVERING)
		coop_transition_recovered(&policy, policy.generation);
	coop_travel_network_frame();
	if (!coop_travel_active() && pause_owned) {
#ifdef DXX_BUILD_DESCENT_II
		if (coop_travel_ending_campaign() && !advancement_presented) {
			advancement_presented = 1;
			pumping = 0;
			coop_finish_secret_campaign();
			return 1;
		}
		/* A new normal mine gets its authored presentation before releasing play */
		if (committed && prepared_action == COOP_CAMPAIGN_ADVANCE && !advancement_presented) {
			advancement_presented = 1;
			coop_briefing_arm(ShowLevelIntro, Current_level_num);
			coop_briefing_apply_sync_flags(advancement_briefing_flags, Current_level_num);
			coop_briefing_run(ShowLevelIntro, Current_level_num);
		}
#endif
		game_flush_inputs();
		if (pause_owned) start_time();
		pause_owned = 0;
	}
	pumping = 0;
#ifdef DXX_BUILD_DESCENT_II
	if (physical_pending >= 0 && coop_travel_take_normal_exit()) {
		int trigger_num = physical_pending;
		physical_pending = -1;
		requested = COOP_OP_NONE;
		physical_replay = trigger_num;
		check_trigger_sub(trigger_num, Player_num, 0);
		physical_replay = -1;
		return 1;
	}
#endif
	return 0;
}

const char *coop_travel_status_message(void)
{
	static char message[160];
	if (!coop_travel_active() && coop_travel_waiting_after_death()) return "Waiting for teammates to leave the mine";
	if (!coop_travel_active()) return NULL;
	if (policy.phase == COOP_PHASE_WARNING) {
		snprintf(message, sizeof(message), "%s: %s in %u seconds", Players[initiator].callsign,
		         policy.operation == COOP_OP_SECRET_ENTER ? "Going to the secret area" : prepared_action == COOP_CAMPAIGN_ENDGAME ? "Finishing the mission"
		                                                                             : prepared_action == COOP_CAMPAIGN_ADVANCE   ? "Advancing to the next mine"
		                                                                                                                          : "Returning to the main level",
		         coop_transition_seconds_remaining(&policy));
		return message;
	}
	if (policy.phase == COOP_PHASE_FREEZING) return "Waiting for all players before secret travel";
	if (policy.phase == COOP_PHASE_CAPTURING) return "Preparing secret travel checkpoint";
	if (prepared_action == COOP_CAMPAIGN_ENDGAME && policy.phase >= COOP_PHASE_LOADING && policy.phase <= COOP_PHASE_COMMITTED)
		return "Waiting for all players to finish the mission";
	if (policy.phase == COOP_PHASE_LOADING) return "Waiting for all players to load the mine";
	if (policy.phase == COOP_PHASE_COMMITTED || policy.phase == COOP_PHASE_SETTLED) return "Waiting to resume together";
	return coop_transition_block_reason(&policy);
}

void coop_travel_get_state(coop_transition_policy *state, unsigned *granted, unsigned *released, int *have_state)
{
	if (state) *state = policy;
	if (granted) *granted = normal_granted;
	if (released) *released = release_acked;
	if (have_state) *have_state = known;
}
