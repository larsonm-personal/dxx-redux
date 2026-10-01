#include "net_udp_join_wait.h"
#include "coop/coop_briefing.h"
#include "android_log.h"
#include "args.h"
#include "game.h"
#include "gameseq.h"
#include "multi.h"
#include "net_udp.h"
#include "event.h"
#include "key.h"
#include "newmenu.h"
#include "menu.h"
#include "gr.h"
#include "physfsx.h"
#include "text.h"
#include "timer.h"

#include <pthread.h>
#include <stdio.h>
#include <string.h>

static int active, pumping, contact_started;
static int prepared, target_valid, restart, target_level;
static uint64_t target_visit;
static net_join_status current;
static uint64_t received_at, contact_since, deadline, shown_briefing;
static unsigned presentations, object_packets, retries;
static const char *failure;
static pthread_mutex_t ui_lock = PTHREAD_MUTEX_INITIALIZER;
static struct {
	int active, remaining_ms, duration_ms, cancel;
	uint64_t generation;
	char text[512];
} ui;

static uint64_t now_ms(void)
{
	return (uint64_t) timer_query() * 1000 / F1_0;
}

static void publish(void)
{
	const uint64_t now = now_ms();
	int remaining = deadline > now ? (int) (deadline - now) : 0;
	const char *message;
	switch (current.phase) {
		case NET_JOIN_READY: message = "Joining the team\nWaiting for approval or level synchronization"; break;
		case NET_JOIN_PREPARING: message = "Preparing the next level\nWaiting for the team to finish loading"; break;
		case NET_JOIN_BRIEFING:
			message = current.host_ready ? "Briefing - the host is ready\nJoining with the remaining briefing time"
			                             : "Briefing - joining the team\nThe team's original countdown is running";
			break;
		case NET_JOIN_ESCAPE: message = "The mine is being evacuated\nYou will join the next level"; break;
		case NET_JOIN_FLYOUT: message = "Players are leaving the mine\nWaiting for the exit sequence to finish"; break;
		case NET_JOIN_SCORES: message = "The team is reviewing results\nYou will continue automatically"; break;
		case NET_JOIN_TRAVEL: message = "The team is changing levels\nWaiting for the destination"; break;
		case NET_JOIN_RESTORE: message = "The team is restoring game state\nWaiting for synchronization"; break;
		case NET_JOIN_COMPLETE: message = "Mission complete\nThere is no next level to join"; break;
		default: message = "Joining the team\nWaiting for the host to respond"; break;
	}
	if (now > received_at + 5000) message = "Waiting for the host to respond\nChecking the connection";
	pthread_mutex_lock(&ui_lock);
	ui.active = active;
	ui.generation = current.briefing ? current.briefing : 1;
	ui.remaining_ms = current.duration_ms && now <= received_at + 5000 ? remaining : -1;
	ui.duration_ms = current.duration_ms ? (int) current.duration_ms : -1;
	if (ui.remaining_ms >= 0 && current.phase == NET_JOIN_BRIEFING) {
		unsigned seconds = (remaining + 999) / 1000;
		snprintf(ui.text, sizeof(ui.text), "Briefing: up to %u:%02u remaining\n%s", seconds / 60, seconds % 60,
		         current.host_ready ? "Joining the team - host is ready" : "Joining with the team's remaining time");
	} else if (ui.remaining_ms >= 0) {
		unsigned seconds = (remaining + 999) / 1000;
		snprintf(ui.text, sizeof(ui.text), "%s\n%s%u:%02u remaining", message,
		         current.phase == NET_JOIN_ESCAPE ? "Reactor: " : "Up to ", seconds / 60, seconds % 60);
	} else snprintf(ui.text, sizeof(ui.text), "%s", message);
	pthread_mutex_unlock(&ui_lock);
}

void net_join_wait_begin(void)
{
	memset(&current, 0, sizeof(current));
	active = 1;
	pumping = contact_started = 0;
	prepared = target_valid = restart = 0;
	deadline = shown_briefing = 0;
	presentations = object_packets = retries = 0;
	failure = NULL;
	pthread_mutex_lock(&ui_lock);
	ui.cancel = 0;
	pthread_mutex_unlock(&ui_lock);
	received_at = now_ms();
	publish();
}

void net_join_wait_end(void)
{
	active = 0;
	publish();
}

int net_join_wait_active(void)
{
	return active;
}

int net_join_wait_cancel(uint64_t generation)
{
	pthread_mutex_lock(&ui_lock);
	int accepted = ui.active && generation == ui.generation;
	if (accepted) ui.cancel = 1;
	pthread_mutex_unlock(&ui_lock);
	return accepted;
}

const char *net_join_wait_failure(void)
{
	return failure;
}

void net_join_wait_receive(const net_join_status *status, unsigned transit_ms)
{
	if (!active || status->phase >= NET_JOIN_PHASE_COUNT || status->level < -127 || status->level > 127 ||
	    status->duration_ms > 3600000 || status->remaining_ms > status->duration_ms ||
	    status->completed > status->total || status->total > MAX_PLAYERS || status->host_ready > 1) return;
	const uint64_t now = now_ms();
	uint64_t next_deadline = now + (status->remaining_ms > transit_ms ? status->remaining_ms - transit_ms : 0);
	/* The same briefing never acquires time through retry latency or reordering */
	if (current.phase == NET_JOIN_BRIEFING && status->phase == NET_JOIN_BRIEFING &&
	    current.briefing == status->briefing && deadline < next_deadline) next_deadline = deadline;
	if (current.phase != status->phase)
		debug_log(DLOG_NETWORK, "Join wait: phase=%u level=%d remaining_ms=%u", status->phase, status->level, status->remaining_ms);
	current = *status;
	if (target_valid && (status->visit != target_visit || status->level != target_level ||
	                     (status->phase != NET_JOIN_READY && status->phase != NET_JOIN_BRIEFING &&
	                      status->phase != NET_JOIN_PREPARING))) restart = 1;
	received_at = now;
	deadline = next_deadline;
	publish();
}

int net_join_wait_can_request(void)
{
	return !active || (prepared && !restart && current.phase == NET_JOIN_READY);
}

int net_join_wait_reader_cancelled(void)
{
	return !active || current.phase != NET_JOIN_BRIEFING || current.briefing != shown_briefing ||
	       now_ms() >= deadline || now_ms() > received_at + 5000 || multi_quit_game || restart ||
	       Network_rejoined || Network_status != NETSTAT_WAITING;
}

static void reader_pump(void)
{
	if (!contact_started) {
		contact_since = now_ms();
		contact_started = 1;
	}
	net_udp_join_wait_poll();
	pthread_mutex_lock(&ui_lock);
	const int cancelled = ui.cancel;
	pthread_mutex_unlock(&ui_lock);
	if (now_ms() > received_at + 30000 && now_ms() > contact_since + 30000) failure = "The host stopped responding while you were joining";
	else if (current.phase == NET_JOIN_COMPLETE) failure = "The mission has finished. There is no next level to join";
	if (cancelled || failure) Network_status = NETSTAT_MENU;
	publish();
}

void net_join_wait_frame(void)
{
	if (!active || pumping) return;
	pumping = 1;
	reader_pump();
	if (prepared && !restart && current.phase == NET_JOIN_BRIEFING && current.briefing && current.briefing != shown_briefing &&
	    current.level == Current_level_num && now_ms() < deadline && now_ms() <= received_at + 5000 &&
	    Network_status == NETSTAT_WAITING && !multi_quit_game && !Network_rejoined && coop_briefing_plan_ready()) {
		shown_briefing = current.briefing;
		const unsigned before = coop_briefing_presentations_started();
		++presentations;
		debug_log(DLOG_NETWORK, "Join briefing: begin generation=%llu level=%d", (unsigned long long) shown_briefing, current.level);
		coop_briefing_join_run(reader_pump, net_join_wait_reader_cancelled);
		if (coop_briefing_presentations_started() == before) --presentations;
		debug_log(DLOG_NETWORK, "Join briefing: closed generation=%llu", (unsigned long long) shown_briefing);
	}
	publish();
	pumping = 0;
}

static int prepare_poll(newmenu *menu, d_event *event, void *unused)
{
	(void) menu;
	(void) unused;
	if (event->type == EVENT_KEY_COMMAND && event_key_get(event) == KEY_ESC) {
		Network_status = NETSTAT_MENU;
		return -2;
	}
	if (event->type != EVENT_WINDOW_DRAW) return 0;
	net_join_wait_frame();
	if (Network_status == NETSTAT_MENU) return -2;
	return current.level && current.visit && (current.phase == NET_JOIN_READY || current.phase == NET_JOIN_BRIEFING) ? -2 : 0;
}

int net_join_wait_prepare(void)
{
	newmenu_item item = { 0 };
	item.type = NM_TYPE_TEXT;
	item.text = "Following the team - press Esc to cancel";
	Network_status = NETSTAT_WAITING;
	int result;
	do {
		result = newmenu_do2(NULL, TXT_WAIT, 1, &item, prepare_poll, NULL, 0, Menu_pcx_name);
	} while (result >= 0);
	if (Network_status == NETSTAT_MENU || !current.level || !current.visit) return 0;
	Netgame.levelnum = target_level = current.level;
	target_visit = current.visit;
	target_valid = 1;
	return 1;
}

void net_join_wait_sync_begin(void)
{
	if (active) {
		prepared = 1;
		/* Local loading may have blocked polling; give a fresh query time to reply */
		contact_started = 0;
	}
}

int net_join_wait_restart(void)
{
	return active && restart && Network_status != NETSTAT_MENU;
}

void net_join_wait_interrupt(void)
{
	if (active) restart = 1;
}

void net_join_wait_retry(void)
{
	++retries;
	prepared = target_valid = restart = 0;
	Network_rejoined = 0;
	current.phase = NET_JOIN_CONTACT;
	publish();
}

uint64_t net_join_wait_visit(void)
{
	return target_valid ? target_visit : 0;
}

int net_join_wait_ui(char *text, size_t size, uint64_t *generation, int *remaining_ms, int *duration_ms)
{
	pthread_mutex_lock(&ui_lock);
	int visible = ui.active;
	if (visible) {
		snprintf(text, size, "%s", ui.text);
		*generation = ui.generation;
		*remaining_ms = ui.remaining_ms;
		*duration_ms = ui.duration_ms;
	}
	pthread_mutex_unlock(&ui_lock);
	return visible;
}

void net_join_wait_get_state(net_join_status *status, unsigned *started)
{
	*status = current;
	status->remaining_ms = deadline > now_ms() ? (uint32_t) (deadline - now_ms()) : 0;
	*started = presentations;
}

void net_join_wait_note_objects(void)
{
	++object_packets;
}
unsigned net_join_wait_object_packets(void)
{
	return object_packets;
}
unsigned net_join_wait_retries(void)
{
	return retries;
}
