#include "coop_endgame.h"
#include "coop_travel.h"
#include "coop_world_visit.h"
#include "coop_multi_status.h"
#include "android_log.h"
#include "event.h"
#include "game.h"
#include "gameseq.h"
#include "gr.h"
#include "gamefont.h"
#include "key.h"
#include "mission.h"
#include "multi.h"
#include "net_udp.h"
#include "screens.h"
#include "timer.h"
#include "window.h"
#include <stdint.h>
#include <string.h>

/* Keep the 160-byte MULTI_COOP_ENDGAME size in both engines' multi.h in sync
 * Header: kind, phase, roster, acknowledgments, host, game ID, level, visit
 * Eight 16-byte final score/deaths/robot-kills records follow at offset 32
 * UDP authenticates the sender; the world visit rejects delayed prior endings */
enum { PACKET_SIZE = 160,
	   READY = 1,
	   STATE = 2,
	   ACK = 3,
	   WAITING = 1,
	   COMMITTED = 2,
	   RELEASED = 3 };
static int initialized, active, released, phase, host, level, pumping, pause_owned;
static uint32_t game_id;
static uint64_t visit, last_send;
static unsigned participants, ready, acknowledged;
static unsigned char results[MAX_PLAYERS][16];

static unsigned bit(int p)
{
	return p >= 0 && p < MAX_PLAYERS ? 1u << p : 0;
}
static uint32_t get32(const unsigned char *p)
{
	return (uint32_t) p[0] | (uint32_t) p[1] << 8 | (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}
static void put32(unsigned char *p, uint32_t v)
{
	for (unsigned i = 0; i < 4; ++i) p[i] = (unsigned char) (v >> (8 * i));
}
static uint64_t get64(const unsigned char *p)
{
	return get32(p) | (uint64_t) get32(p + 4) << 32;
}
static void put64(unsigned char *p, uint64_t v)
{
	put32(p, (uint32_t) v);
	put32(p + 4, (uint32_t) (v >> 32));
}

void coop_endgame_reset(void)
{
	if (pause_owned) start_time();
	initialized = active = released = phase = pumping = pause_owned = 0;
	participants = ready = acknowledged = 0;
	last_send = 0;
	memset(results, 0, sizeof(results));
}
int coop_endgame_active(void)
{
	return active;
}
int coop_endgame_released(void)
{
	return released;
}

static int context(void)
{
	if (!(Game_mode & GM_MULTI_COOP) || !Game_wind || !coop_world_visit_current() ||
	    (Current_level_num != Last_level && !coop_travel_ending_campaign())) return 0;
	if (initialized && game_id == (uint32_t) Netgame.protocol.udp.GameID &&
	    visit == coop_world_visit_current() && level == Current_level_num &&
	    (active || phase >= COMMITTED || host == multi_who_is_master())) return 1;
	coop_endgame_reset();
	game_id = (uint32_t) Netgame.protocol.udp.GameID;
	visit = coop_world_visit_current();
	level = Current_level_num;
	host = multi_who_is_master();
	for (int i = 0; i < N_players; ++i)
		if (Players[i].connected != CONNECT_DISCONNECTED && (i != OBSERVER_PLAYER_ID || !Netgame.max_numobservers || i == host))
			participants |= bit(i);
	phase = WAITING;
	initialized = 1;
	return 1;
}

static void apply_results(void)
{
	for (int i = 0; i < N_players; ++i) {
		if (i == Player_num || !(participants & bit(i))) continue;
		Players[i].score = (int32_t) get32(results[i]);
		Players[i].net_killed_total = (short) get32(results[i] + 4);
		Coop_kill_stats[i].robots_killed = (int32_t) get32(results[i] + 8);
		if (Players[i].connected != CONNECT_DISCONNECTED) Players[i].connected = CONNECT_END_MENU;
	}
}

static void send_packet(int kind)
{
	unsigned char packet[PACKET_SIZE] = { MULTI_COOP_ENDGAME };
	packet[1] = (unsigned char) kind;
	packet[2] = (unsigned char) phase;
	packet[3] = (unsigned char) participants;
	packet[4] = (unsigned char) acknowledged;
	packet[5] = (unsigned char) host;
	put32(packet + 8, game_id);
	put32(packet + 12, (uint32_t) level);
	put64(packet + 16, visit);
	memcpy(packet + 32, results, sizeof(results));
	if (kind == STATE) {
		for (int i = 0; i < N_players; ++i)
			if (i != host && (participants & bit(i))) multi_send_data_direct(packet, sizeof(packet), i, 0);
		if (Netgame.numobservers) multi_send_data_direct(packet, sizeof(packet), host, 0);
	} else if (!is_observer()) multi_send_data_direct(packet, sizeof(packet), host, 0);
}

void coop_endgame_receive(const unsigned char *packet, int sender)
{
	if (!context() || !bit(sender) || packet[5] != host ||
	    get32(packet + 8) != game_id || (int32_t) get32(packet + 12) != level ||
	    get64(packet + 16) != visit) return;
	if (Player_num == host && (participants & bit(sender))) {
		if (packet[1] == READY && phase == WAITING) {
			if (!(ready & bit(sender))) memcpy(results[sender], packet + 32 + 16 * sender, 16);
			ready |= bit(sender);
		} else if (packet[1] == ACK && packet[2] == phase && phase >= COMMITTED) {
			acknowledged |= bit(sender);
		}
	} else if (Player_num != host && sender == host && packet[1] == STATE) {
		if (packet[2] < phase || packet[2] > RELEASED || !(packet[3] & bit(host)) ||
		    (!is_observer() && !(packet[3] & bit(Player_num))) || (packet[4] & ~packet[3])) return;
		participants = packet[3];
		if (packet[2] >= COMMITTED && phase < COMMITTED) {
			/* A commit may arrive before this observer reaches its local exit */
			memcpy(results, packet + 32, sizeof(results));
			apply_results();
		}
		phase = packet[2];
		if (active && phase == RELEASED) released = 1;
	}
}

int coop_endgame_host_disconnected(int player)
{
	if (!initialized || player != host || (!active && phase < COMMITTED)) return 0;
	if (phase >= COMMITTED) {
		/* All final outcomes were committed; host loss cannot revoke victory */
		phase = RELEASED;
		released = 1;
		COOPLOG("endgame host left completed campaign: player=%d level=%d", Player_num, level);
	} else multi_quit_game = 1;
	return 1;
}

void coop_endgame_network_frame(void)
{
	uint64_t now = (uint64_t) timer_query() * 1000 / F1_0;
	if (!active || multi_quit_game) return;
	if (Player_num == host) {
		/* Authoritative disconnects remove waiters, never a viewer's page timer */
		for (int i = 0; i < N_players; ++i)
			if (i != host && Players[i].connected == CONNECT_DISCONNECTED) participants &= ~bit(i);
		if (phase == WAITING && (ready & participants) == participants) {
			phase = COMMITTED;
			acknowledged = bit(host);
			apply_results();
			last_send = 0;
		}
		if (phase == COMMITTED && (acknowledged & participants) == participants) {
			phase = RELEASED;
			acknowledged = bit(host);
			last_send = 0;
		}
		if (phase == RELEASED && (acknowledged & participants) == participants) released = 1;
	}
	if (now >= last_send + 100) {
		last_send = now;
		send_packet(Player_num == host ? STATE : phase == WAITING ? READY
		                                                          : ACK);
	}
}

void coop_endgame_pump(void)
{
	/* Observers do not fly through exits. Dispatch outside packet callbacks,
	 * once all playing slots finished (host) or the host committed (client) */
	if (!active && initialized && !pumping && Game_wind && is_observer() &&
	    ((Player_num == host && (ready & (participants & ~bit(host))) == (participants & ~bit(host))) ||
	     (Player_num != host && phase >= COMMITTED))) {
		PlayerFinishedLevel(0);
		return;
	}
	if (!active || pumping || !Game_wind) return;
	pumping = 1;
	multi_do_protocol_frame(0, 1);
	pumping = 0;
}

static int waiting_handler(window *wind, d_event *event, void *unused)
{
	(void) wind;
	(void) unused;
	if (event->type == EVENT_KEY_COMMAND && event_key_get(event) == KEY_ESC) {
		multi_quit_game = 1;
		return 1;
	}
	if (event->type == EVENT_WINDOW_DRAW) {
		gr_set_current_canvas(NULL);
		gr_clear_canvas(BM_XRGB(0, 0, 0));
		grd_curcanv->cv_font = MEDIUM3_FONT;
		gr_set_fontcolor(BM_XRGB(63, 63, 63), -1);
		gr_string(0x8000, SHEIGHT / 2, "Waiting for players to finish");
		timer_delay2(50);
	}
	return event->type == EVENT_KEY_COMMAND || event->type == EVENT_MOUSE_BUTTON_DOWN ||
	       event->type == EVENT_JOYSTICK_BUTTON_DOWN;
}

void coop_endgame_begin(void)
{
	window *waiting;
	if (!(Game_mode & GM_MULTI_COOP) || active) return;
	if (!context() || multi_quit_game) {
		if (Game_wind) window_close(Game_wind);
		return;
	}
	active = 1;
	if (phase == RELEASED) released = 1;
	window_set_visible(Game_wind, 0);
	stop_time();
	pause_owned = 1;
	put32(results[Player_num], (uint32_t) Players[Player_num].score);
	put32(results[Player_num] + 4, (uint32_t) Players[Player_num].net_killed_total);
	put32(results[Player_num] + 8, (uint32_t) Coop_kill_stats[Player_num].robots_killed);
	ready |= bit(Player_num);
	Players[Player_num].connected = CONNECT_END_MENU;
	Network_status = NETSTAT_ENDLEVEL;
	set_screen_mode(SCREEN_MENU);
	game_flush_inputs();
	waiting = window_create(&grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT, waiting_handler, NULL);
	if (!waiting) multi_quit_game = 1;
	while (waiting && !released && !multi_quit_game) event_process();
	if (waiting && window_exists(waiting)) window_close(waiting);
	game_flush_inputs();
	if (multi_quit_game) {
		if (Game_wind) window_close(Game_wind);
		return;
	}
	COOPLOG("endgame presentation released: player=%d level=%d participants=%u", Player_num, level, participants);
}
