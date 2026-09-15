#include "coop_briefing.h"
#include "coop_gameplay_runtime.h"
#include "coop_transition_policy.h"
#include "coop_save.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

#include "event.h"
#include "game.h"
#include "gameseq.h"
#include "gr.h"
#include "key.h"
#include "multi.h"
#include "newmenu.h"
#include "screens.h"
#include "songs.h"
#include "timer.h"
#include "text.h"
#include "window.h"

/* MULTI_COOP_BRIEFING is a 128-byte, explicitly little-endian envelope
 * Header: kind/phase/reason/roster/ready/host, game ID, level, generation,
 * snapshot revision, remaining milliseconds. Eight 12-byte progress records
 * follow at offset 32, with each player's own page count and completion state
 * Sender identity comes exclusively from UDP validation */
enum { PACKET_SIZE = 128,
	   STATE = 1,
	   ACK = 2,
	   PROGRESS = 3,
	   PEER_TIMEOUT_MS = 30000 };
static coop_transition_policy policy;
static coop_presentation_progress local_progress;
static int plan_ready;
static int armed, running, planning, presenting, skipped, unavailable;
static int destination, host, have_state, presentation_closed, pumping;
static int suppress_for_restore;
static const char *failure_reason;
static unsigned presentations_started;
static unsigned release_acknowledged;
static unsigned test_release_delay_ms, test_release_packets_dropped;
static uint64_t test_release_delay_until;
static uint32_t game_id, snapshot_revision;
static uint64_t last_generation, last_send, last_ack, last_host_packet, release_since;
static coop_transition_phase observed_phase;
static uint64_t peer_seen[COOP_TRANSITION_PLAYERS];
static pthread_mutex_t ui_lock = PTHREAD_MUTEX_INITIALIZER;
static struct {
	char text[1024];
	uint64_t generation, requested;
	int launch;
} ui;

static uint64_t now_ms(void)
{
	return (uint64_t) timer_query() * 1000 / F1_0;
}
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
static unsigned bit(int player)
{
	return player >= 0 && player < COOP_TRANSITION_PLAYERS ? 1u << player : 0;
}

int coop_briefing_plan_ready(void)
{
	return plan_ready;
}

static int local_observer(void)
{
	/* An observing host still owns the deadline and launch controls */
	return is_observer() && Player_num != host;
}

static void encode_progress(unsigned char *p, const coop_presentation_progress *v)
{
	put32(p, v->revision);
	p[4] = (unsigned char) v->completed;
	p[5] = (unsigned char) (v->completed >> 8);
	p[6] = (unsigned char) v->total;
	p[7] = (unsigned char) (v->total >> 8);
	p[8] = (unsigned char) v->state;
}
static int decode_progress(const unsigned char *p, coop_presentation_progress *v)
{
	v->revision = get32(p);
	v->completed = p[4] | (unsigned) p[5] << 8;
	v->total = p[6] | (unsigned) p[7] << 8;
	v->state = (coop_presentation_state) p[8];
	return v->completed <= v->total && v->state <= COOP_PRESENTATION_UNAVAILABLE;
}

static void send_packet(int kind)
{
	unsigned char packet[PACKET_SIZE] = { MULTI_COOP_BRIEFING };
	uint64_t now = now_ms();
	uint64_t remaining = policy.deadline_ms > now ? policy.deadline_ms - now : 0;
	packet[1] = (unsigned char) kind;
	packet[2] = (unsigned char) policy.phase;
	packet[3] = (unsigned char) policy.launch_reason;
	packet[4] = policy.participants;
	packet[5] = policy.presentation_ready;
	packet[6] = (unsigned char) host;
	packet[7] = (unsigned char) suppress_for_restore;
	put32(packet + 8, game_id);
	put32(packet + 12, (uint32_t) destination);
	put64(packet + 16, policy.generation);
	if (kind == STATE) {
		put32(packet + 24, ++snapshot_revision);
		put32(packet + 28, (uint32_t) remaining);
		for (int i = 0; i < COOP_TRANSITION_PLAYERS; ++i)
			encode_progress(packet + 32 + 12 * i, &policy.progress[i]);
		for (int i = 0; i < N_players; ++i)
			if (i != host && (policy.participants & bit(i)))
				multi_send_data_direct(packet, sizeof(packet), i, 0);
		/* The direct-to-self path forwards only to observers. Briefing control
		 * bypasses their optional gameplay delay in the UDP transport */
		if (Netgame.numobservers)
			multi_send_data_direct(packet, sizeof(packet), host, 0);
	} else {
		if (kind == PROGRESS)
			encode_progress(packet + 32, &local_progress);
		multi_send_data_direct(packet, sizeof(packet), host, 0);
	}
}

void coop_briefing_receive(const unsigned char *packet, int sender)
{
	uint64_t generation = get64(packet + 16), now = now_ms();
	if (!armed || !(Game_mode & GM_MULTI_COOP) || !Netgame.CoopBriefings ||
	    get32(packet + 8) != game_id || (int32_t) get32(packet + 12) != destination ||
	    !bit(sender) || packet[6] != host)
		return;
	if (packet[1] == STATE && sender == host && Player_num != host) {
		coop_transition_policy next = policy;
		uint32_t revision = get32(packet + 24), remaining = get32(packet + 28);
		coop_transition_phase phase = (coop_transition_phase) packet[2];
		if (!generation || (have_state ? generation != policy.generation : generation <= last_generation) ||
		    (have_state && revision <= snapshot_revision) || !revision ||
		    (!have_state && !local_observer() && phase != COOP_PHASE_BRIEFING_PREPARE) ||
		    (phase != COOP_PHASE_SETTLED && (phase < COOP_PHASE_BRIEFING_PREPARE || phase > COOP_PHASE_COMMITTED)) ||
		    (have_state && phase != COOP_PHASE_SETTLED && phase < policy.phase) ||
		    !(packet[4] & bit(host)) || (!local_observer() && !(packet[4] & bit(Player_num))) ||
		    (packet[5] & ~packet[4]) || packet[3] > COOP_LAUNCH_DEADLINE ||
		    remaining > COOP_BRIEFING_LIMIT_MS || packet[7] > 1 ||
		    (have_state && packet[7] != suppress_for_restore))
			return;
		for (int i = 0; i < COOP_TRANSITION_PLAYERS; ++i)
			if (!decode_progress(packet + 32 + 12 * i, &next.progress[i])) return;
		if (phase == COOP_PHASE_SETTLED && test_release_delay_ms) {
			if (!test_release_delay_until) test_release_delay_until = now + test_release_delay_ms;
			if (now < test_release_delay_until) {
				++test_release_packets_dropped;
				return;
			}
			test_release_delay_ms = 0;
		}
		next.generation = generation;
		next.phase = phase;
		next.operation = phase == COOP_PHASE_SETTLED ? COOP_OP_NONE : COOP_OP_BRIEFING;
		next.launch_reason = (coop_launch_reason) packet[3];
		next.participants = packet[4];
		next.presentation_ready = packet[5];
		next.host = (uint8_t) host;
		next.now_ms = now;
		next.deadline_ms = phase == COOP_PHASE_BRIEFING ? now + remaining : 0;
		policy = next;
		suppress_for_restore = packet[7];
		snapshot_revision = revision;
		last_generation = generation;
		have_state = 1;
		last_host_packet = now;
	} else if (Player_num == host && have_state && generation == policy.generation &&
	           (policy.participants & bit(sender))) {
		if (packet[1] == ACK) {
			if (policy.phase == COOP_PHASE_SETTLED && packet[2] == COOP_PHASE_SETTLED) {
				release_acknowledged |= bit(sender);
				peer_seen[sender] = now;
			} else if (coop_transition_ack(&policy, generation, (coop_transition_phase) packet[2], sender, now))
				peer_seen[sender] = now;
			/* A peer can still be waiting for the final release */
			if (policy.phase == COOP_PHASE_SETTLED) send_packet(STATE);
		} else if (packet[1] == PROGRESS && packet[2] == COOP_PHASE_BRIEFING) {
			coop_presentation_progress progress;
			if (!decode_progress(packet + 32, &progress)) return;
			if (coop_transition_progress(&policy, generation, sender, progress.revision,
			                             progress.completed, progress.total, progress.state, now) ||
			    (policy.phase == COOP_PHASE_BRIEFING && progress.revision == policy.progress[sender].revision))
				peer_seen[sender] = now;
		}
	}
}

static const char *progress_name(coop_presentation_state state)
{
	switch (state) {
		case COOP_PRESENTATION_READING: return "Reading";
		case COOP_PRESENTATION_VIDEO: return "Watching video";
		case COOP_PRESENTATION_READY: return "Ready";
		case COOP_PRESENTATION_SKIPPED: return "Ready - skipped";
		case COOP_PRESENTATION_UNAVAILABLE: return "Ready - media unavailable";
		default: return "Preparing";
	}
}

static void update_ui(void)
{
	char text[sizeof(ui.text)];
	int launch = running && have_state && policy.phase == COOP_PHASE_BRIEFING &&
	             Player_num == host && (policy.presentation_ready & bit(host));
	size_t used = 0;
	if (running) {
		if (suppress_for_restore) {
			snprintf(text, sizeof(text), "Waiting to restore save...");
			launch = 0;
		} else if (have_state && policy.phase == COOP_PHASE_BRIEFING) {
			unsigned seconds = coop_transition_seconds_remaining(&policy);
			used = (size_t) snprintf(text, sizeof(text), "Briefings: %u:%02u remaining%s",
			                         seconds / 60, seconds % 60,
			                         Player_num != host && (policy.presentation_ready & bit(host)) ? "\nHost is ready and waiting for you" : "");
			if (!presenting || Player_num == host)
				for (int i = 0; i < COOP_TRANSITION_PLAYERS; ++i)
					if (policy.participants & bit(i)) {
						const coop_presentation_progress *p = &policy.progress[i];
						used += (size_t) snprintf(text + used, sizeof(text) - used, "\n%.8s: %u/%u %s",
						                          Players[i].callsign, p->completed, p->total, progress_name(p->state));
					}
		} else {
			snprintf(text, sizeof(text), "%s", !have_state ? "Waiting for host to prepare briefings" : policy.phase == COOP_PHASE_BRIEFING_PREPARE ? "Preparing briefings"
			                                                                                       : policy.phase == COOP_PHASE_LOBBY              ? "Connection lost - returning to lobby"
			                                                                                                                                       : "Waiting to start the mine...");
		}
	} else text[0] = 0;
	pthread_mutex_lock(&ui_lock);
	memcpy(ui.text, text, strlen(text) + 1);
	ui.generation = running && have_state ? policy.generation : 0;
	ui.launch = launch;
	pthread_mutex_unlock(&ui_lock);
}

void coop_briefing_ui(char *text, size_t capacity, uint64_t *generation, int *launch)
{
	pthread_mutex_lock(&ui_lock);
	snprintf(text, capacity, "%s", ui.text);
	*generation = ui.generation;
	*launch = ui.launch;
	pthread_mutex_unlock(&ui_lock);
}
int coop_briefing_request_launch(uint64_t generation)
{
	int accepted;
	pthread_mutex_lock(&ui_lock);
	accepted = generation && generation == ui.generation && ui.launch && !ui.requested;
	if (accepted) {
		ui.requested = generation;
		ui.launch = 0;
	}
	pthread_mutex_unlock(&ui_lock);
	return accepted;
}

int coop_briefing_active(void)
{
	return armed && (!have_state || policy.phase != COOP_PHASE_SETTLED ||
	                 (Player_num == host && (release_acknowledged & policy.participants) != policy.participants));
}
unsigned coop_briefing_release_acknowledged(void)
{
	return release_acknowledged;
}
int coop_briefing_test_delay_release(unsigned milliseconds)
{
	if (!running || !have_state || policy.phase != COOP_PHASE_BRIEFING ||
	    Player_num == host || local_observer() || !milliseconds || milliseconds > 20000)
		return 0;
	test_release_delay_ms = milliseconds;
	test_release_delay_until = 0;
	test_release_packets_dropped = 0;
	return 1;
}
unsigned coop_briefing_test_release_packets_dropped(void)
{
	return test_release_packets_dropped;
}
int coop_briefing_suppressed_for_restore(void)
{
	return suppress_for_restore;
}
unsigned coop_briefing_presentations_started(void)
{
	return presentations_started;
}
void coop_briefing_get_state(coop_transition_policy *state, coop_presentation_progress *local)
{
	*state = policy;
	*local = local_progress;
}
int coop_briefing_presenting(void)
{
	return presenting || planning;
}
int coop_briefing_planning(void)
{
	return planning;
}
int coop_briefing_cancelled(void)
{
	return running && (skipped || multi_quit_game ||
	                   (have_state && policy.phase != COOP_PHASE_BRIEFING));
}
void coop_briefing_plan_add(unsigned steps)
{
	if (planning) {
		unsigned total = local_progress.total + steps;
		local_progress.total = (uint16_t) (total > UINT16_MAX ? UINT16_MAX : total);
	}
}

/* Logical authored pages; overflow caused by local font/layout is not a new
 * shared progress step. The engine supplies the selected message section */
unsigned coop_briefing_count_message(const char *message)
{
	unsigned count = message && *message ? 1 : 0;
	int line_start = 1;
	while (message && *message) {
		char ch = *message++;
		if (ch == ';' && line_start) {
			while (*message && *message != '\n') ++message;
		} else if (ch == '$' && *message) {
			char command = *message++;
			if (command == 'S') break;
			if (command == 'P' && count < UINT16_MAX) ++count;
			if (strchr("DUCFTNOBZP", command))
				while (*message && *message != '\n') ++message;
		}
		line_start = ch == '\n';
	}
	return count;
}

void coop_briefing_plan_message(const char *message)
{
	coop_briefing_plan_add(coop_briefing_count_message(message));
}

void coop_briefing_step(int video)
{
	if (presenting && !coop_briefing_cancelled()) {
		local_progress.state = video ? COOP_PRESENTATION_VIDEO : COOP_PRESENTATION_READING;
		++local_progress.revision;
	}
}
void coop_briefing_step_complete(int viewed)
{
	if (presenting && !coop_briefing_cancelled()) {
		if (viewed && local_progress.completed < local_progress.total) ++local_progress.completed;
		if (!viewed) unavailable = 1;
		++local_progress.revision;
	}
}
void coop_briefing_skip(void)
{
	if (running && presenting) skipped = 1;
}

void coop_briefing_arm(int level)
{
	failure_reason = NULL;
	if (game_id != (uint32_t) Netgame.protocol.udp.GameID) last_generation = 0;
	armed = (Game_mode & GM_MULTI_COOP) && Netgame.CoopBriefings;
	running = planning = presenting = skipped = unavailable = have_state = presentation_closed = 0;
	destination = level;
	suppress_for_restore = 0;
	presentations_started = 0;
	release_acknowledged = 0;
	test_release_delay_ms = test_release_packets_dropped = 0;
	test_release_delay_until = 0;
	game_id = (uint32_t) Netgame.protocol.udp.GameID;
	host = multi_who_is_master();
	memset(&policy, 0, sizeof(policy));
	memset(&local_progress, 0, sizeof(local_progress));
	plan_ready = 0;
	snapshot_revision = 0;
	last_send = last_ack = release_since = 0;
	last_host_packet = now_ms();
	observed_phase = COOP_PHASE_SETTLED;
	pthread_mutex_lock(&ui_lock);
	memset(&ui, 0, sizeof(ui));
	pthread_mutex_unlock(&ui_lock);
}

void coop_briefing_disarm_for_rejoin(void)
{
	/* The host admitted this player to an existing mine after its presentation
	 * barrier settled; it will not send a new briefing PREPARE */
	armed = 0;
}

static void acknowledge_local(void)
{
	coop_transition_phase phase = policy.phase;
	if (local_observer()) return;
	if (Player_num != host && now_ms() < last_ack + 100) return;
	last_ack = now_ms();
	if (phase == COOP_PHASE_SETTLED) {
		release_acknowledged |= bit(Player_num);
		if (Player_num != host) send_packet(ACK);
		return;
	}
	int can_ack = phase == COOP_PHASE_BRIEFING_PREPARE ? plan_ready && !planning : phase == COOP_PHASE_CLOSING_PRESENTATION ? presentation_closed
	                                                                                                                        : phase == COOP_PHASE_LOADING || phase == COOP_PHASE_COMMITTED;
	if (phase == COOP_PHASE_BRIEFING && local_progress.revision) {
		if (Player_num == host)
			coop_transition_progress(&policy, policy.generation, Player_num, local_progress.revision,
			                         local_progress.completed, local_progress.total, local_progress.state, now_ms());
		else send_packet(PROGRESS);
	} else if (can_ack) {
		if (Player_num == host) coop_transition_ack(&policy, policy.generation, phase, Player_num, now_ms());
		else send_packet(ACK);
	}
}

void coop_briefing_network_frame(void)
{
	if (armed && have_state && policy.phase == COOP_PHASE_SETTLED) {
		acknowledge_local();
		/* Do not relinquish operation ownership until each participant has
		 * received release. The next save/load may use a different packet path */
		if (!coop_briefing_active()) {
			if (!release_since) release_since = now_ms();
			if (now_ms() > release_since + PEER_TIMEOUT_MS) armed = 0;
		}
	}
	if (armed && have_state && Player_num == host && now_ms() >= last_send + 250) {
		last_send = now_ms();
		send_packet(STATE);
	}
}

int coop_briefing_host_disconnected(int player)
{
	if (!coop_briefing_active() || player != host) return 0;
	failure_reason = "The host disconnected during briefings";
	policy.phase = COOP_PHASE_LOBBY;
	multi_quit_game = 1;
	return 1;
}

void coop_briefing_pump(void)
{
	uint64_t now, requested;
	if (!running || pumping) return;
	pumping = 1;
	multi_do_protocol_frame(0, 1);
	now = now_ms();
	if (multi_quit_game || multi_who_is_master() != host || !Game_wind) {
		policy.phase = COOP_PHASE_LOBBY;
		multi_quit_game = 1;
	} else if (have_state && Player_num == host) {
		if (observed_phase != policy.phase) {
			observed_phase = policy.phase;
			for (int i = 0; i < COOP_TRANSITION_PLAYERS; ++i) peer_seen[i] = now;
		}
		for (int i = 0; i < COOP_TRANSITION_PLAYERS; ++i)
			if (i != host && (policy.participants & bit(i)) &&
			    (Players[i].connected == CONNECT_DISCONNECTED || now > peer_seen[i] + PEER_TIMEOUT_MS)) {
				multi_disconnect_player(i);
				coop_transition_remove_player(&policy, i, now);
			}
		pthread_mutex_lock(&ui_lock);
		requested = ui.requested;
		ui.requested = 0;
		pthread_mutex_unlock(&ui_lock);
		if (requested) coop_transition_launch_now(&policy, requested, Player_num, now);
		coop_transition_tick(&policy, now);
	} else if (now > last_host_packet + PEER_TIMEOUT_MS) {
		failure_reason = "The host stopped responding during briefings";
		policy.phase = COOP_PHASE_LOBBY;
		multi_quit_game = 1;
	}
	if (have_state && policy.phase != COOP_PHASE_LOBBY) {
		policy.now_ms = now;
		acknowledge_local();
	}
	coop_briefing_network_frame();
	update_ui();
	pumping = 0;
}

int coop_briefing_show_failure(void)
{
	const char *reason = failure_reason;
	if (!reason || Game_wind) return 0;
	/* Window teardown longjmps out of the presentation loop; report afterward */
	failure_reason = NULL;
	game_flush_inputs();
	nm_messagebox("Co-op briefing interrupted", 1, TXT_OK,
	              "%s\n\nThe mine has been closed\nHost or join a co-op game to continue", reason);
	return 1;
}

static int waiting_handler(window *wind, d_event *event, void *unused)
{
	(void) wind;
	(void) unused;
	if (event->type == EVENT_WINDOW_DRAW) {
		gr_set_current_canvas(NULL);
		gr_clear_canvas(BM_XRGB(0, 0, 0));
		timer_delay2(50);
	}
	/* Consume generic confirm/advance events. Launch has its own UI action */
	return event->type == EVENT_KEY_COMMAND || event->type == EVENT_MOUSE_BUTTON_DOWN ||
	       event->type == EVENT_JOYSTICK_BUTTON_DOWN;
}

void coop_briefing_run(void (*present)(int), int level)
{
	window *waiting;
	if (!armed || level != destination || Current_level_num != level || multi_quit_game || !Game_wind) {
		armed = 0;
		return;
	}
	running = 1;
	last_host_packet = now_ms();
	window_set_visible(Game_wind, 0);
	stop_time();
	set_screen_mode(SCREEN_MENU);
	waiting = window_create(&grd_curscreen->sc_canvas, 0, 0, SWIDTH, SHEIGHT, waiting_handler, NULL);
	if (!waiting) {
		multi_quit_game = 1;
	} else {
		if (Player_num == host) {
			coop_arm_auto_restore();
			suppress_for_restore = coop_auto_restore_pending();
		}
		planning = 1;
		if (!suppress_for_restore) present(level);
		planning = 0;
		plan_ready = 1;
		if (Player_num == host) {
			unsigned participants = 0;
			uint64_t generation = now_ms() > last_generation ? now_ms() : last_generation + 1;
			for (int i = 0; i < N_players; ++i)
				if (Players[i].connected == CONNECT_PLAYING) participants |= bit(i);
			if (!coop_transition_init(&policy, generation, host, participants, now_ms()) ||
			    !coop_transition_begin(&policy, generation, COOP_OP_BRIEFING, host, now_ms()))
				multi_quit_game = 1;
			else {
				have_state = 1;
				last_generation = policy.generation;
			}
		}
		while (!multi_quit_game && (!have_state || policy.phase == COOP_PHASE_BRIEFING_PREPARE)) {
			coop_briefing_pump();
			event_process();
		}
		if (!multi_quit_game && policy.phase == COOP_PHASE_BRIEFING) {
			if (suppress_for_restore) {
				local_progress.total = local_progress.completed = 0;
			} else {
				presenting = 1;
				++presentations_started;
				coop_briefing_step(0);
				present(level);
				presenting = 0;
			}
			local_progress.state = skipped ? COOP_PRESENTATION_SKIPPED : unavailable ? COOP_PRESENTATION_UNAVAILABLE
			                                                                         : COOP_PRESENTATION_READY;
			++local_progress.revision;
		}
		presentation_closed = 1;
		while (!multi_quit_game && coop_briefing_active()) {
			coop_briefing_pump();
			event_process();
		}
		if (window_exists(waiting)) window_close(waiting);
	}
	running = presenting = planning = 0;
	update_ui();
	game_flush_inputs();
	start_time();
	if (multi_quit_game) {
		armed = 0;
		if (Game_wind) window_close(Game_wind);
	} else if (Game_wind) {
		coop_gameplay_restore_player_life();
		set_screen_mode(SCREEN_GAME);
		songs_play_level_song(Current_level_num, 0);
		window_set_visible(Game_wind, 1);
	}
}
