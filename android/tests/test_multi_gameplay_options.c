#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "byteswap.h"
#include "cntrlcen.h"
#include "fuelcen.h"
#include "game.h"
#include "hudmsg.h"
#include "multi.h"
#include "multi_gameplay_options.h"

int Game_mode;
int Player_num;
int N_players;
#ifdef DXX_BUILD_DESCENT_II
player Players[MAX_PLAYERS + 4];
#else
player Players[MAX_PLAYERS];
#endif
ubyte multibuf[MAX_MULTI_MESSAGE_LEN + 4];
fix Countdown_timer;
int Reactor_countdown_paused;

static int failures;
static int active;
static int observer;
static int sent_count;
static int sent_length;
static int sent_recipient;
static int sent_priority;
static ubyte sent[MAX_MULTI_MESSAGE_LEN + 4];
static char message[256];

#define CHECK(condition)                                                 \
	do {                                                                 \
		if (!(condition)) {                                              \
			fprintf(stderr, "FAIL line %d: %s\n", __LINE__, #condition); \
			failures++;                                                  \
		}                                                                \
	} while (0)

bool is_observer(void)
{
	return observer != 0;
}
int multi_i_am_master(void)
{
	return Player_num == 0;
}
int multi_who_is_master(void)
{
	return 0;
}
int reactor_countdown_is_active(void)
{
	return active;
}

int reactor_countdown_set_paused(int paused, fix remaining_time)
{
	int changed = Reactor_countdown_paused != paused;
	Reactor_countdown_paused = paused;
	Countdown_timer = remaining_time;
	return changed;
}

int matcen_set_mode(int mode)
{
	return matcen_mode_set(mode);
}
int matcen_restore_mode(int mode, const ubyte *counts, int count)
{
	return matcen_mode_restore(mode, counts, count);
}

static void record_packet(const ubyte *buf, int len, int recipient, int priority)
{
	CHECK(len <= (int) sizeof(sent));
	if (len > (int) sizeof(sent)) return;
	memcpy(sent, buf, len);
	sent_length = len;
	sent_recipient = recipient;
	sent_priority = priority;
	sent_count++;
}

void multi_send_data(ubyte *buf, int len, int priority)
{
	record_packet(buf, len, -1, priority);
}

#ifdef DXX_BUILD_DESCENT_II
void multi_send_data_direct(ubyte *buf, int len, int pnum, int priority)
#else
void multi_send_data_direct(const ubyte *buf, int len, int pnum, int priority)
#endif
{
	record_packet(buf, len, pnum, priority);
}

int HUD_init_message(int class_flag, const char *format, ...)
{
	va_list args;
	CHECK(class_flag == HM_MULTI);
	va_start(args, format);
	vsnprintf(message, sizeof(message), format, args);
	va_end(args);
	return 1;
}

int HUD_init_message_literal(int class_flag, const char *text)
{
	return HUD_init_message(class_flag, "%s", text);
}

static void reset(void)
{
	Game_mode = GM_MULTI | GM_MULTI_COOP;
	Player_num = 0;
	N_players = 3;
	memset(Players, 0, sizeof(Players));
	strcpy(Players[1].callsign, "Peer");
	Players[1].connected = CONNECT_PLAYING;
	active = 1;
	observer = 0;
	Countdown_timer = i2f(45);
	Reactor_countdown_paused = 0;
	matcen_mode_reset_game();
	sent_count = 0;
	message[0] = 0;
	memset(multibuf, 0xa5, sizeof(multibuf));
}

static void check_reactor_round_trip(void)
{
	ubyte request[7], state[7];
	const ubyte expected_request[7] = { MULTI_REACTOR_PAUSE, 0, 0, 0, 0, 0, 0 };
	reset();
	Player_num = 1;
	multi_request_reactor_pause_toggle();
	CHECK(sent_count == 1 && sent_length == 7 && sent_recipient == 0 && sent_priority == 2);
	CHECK(memcmp(sent, expected_request, 7) == 0);
	CHECK(Reactor_countdown_paused == 0);
	memcpy(request, sent, sizeof(request));
	Player_num = 0;
	multi_do_reactor_pause(request, 1);
	CHECK(sent_count == 2 && sent_recipient == -1 && sent_priority == 2);
	CHECK(sent[0] == MULTI_REACTOR_PAUSE && sent[1] == 1 && sent[2] == 1);
	CHECK(GET_INTEL_INT(sent + 3) == i2f(45));
	CHECK(strcmp(message, "Peer paused the reactor countdown") == 0);
	memcpy(state, sent, sizeof(state));
	Player_num = 1;
	Reactor_countdown_paused = 0;
	Countdown_timer = i2f(42);
	multi_do_reactor_pause(state, 0);
	CHECK(Reactor_countdown_paused == 1 && Countdown_timer == i2f(45));
	CHECK(sent_count == 2);
	CHECK(strcmp(message, "Reactor countdown paused") == 0);
	Player_num = 0;
	multi_request_reactor_pause_toggle();
	CHECK(Reactor_countdown_paused == 0 && sent[2] == 0 && sent_count == 3);
	CHECK(strcmp(message, "Reactor countdown resumed") == 0);
}

static void check_matcen_round_trip_and_join(void)
{
	ubyte request[3 + MATCEN_MODE_MAX_CENTERS], state[sizeof(request)], counts[MATCEN_MODE_MAX_CENTERS];
	int i;
	reset();
	Player_num = 1;
	multi_request_matcen_mode(MATCEN_MODE_ONE_ROUND);
	CHECK(sent_count == 1 && sent_length == (int) sizeof(request) && sent_recipient == 0 && sent_priority == 2);
	CHECK(sent[0] == MULTI_MATCEN_MODE && sent[1] == 0 && sent[2] == MATCEN_MODE_ONE_ROUND);
	for (i = 3; i < sent_length; i++) CHECK(sent[i] == 0);
	CHECK(matcen_mode_get() == MATCEN_MODE_DEFAULT);
	memcpy(request, sent, sizeof(request));
	Player_num = 0;
	matcen_mode_record_activation(0);
	matcen_mode_record_activation(MATCEN_MODE_MAX_CENTERS - 1);
	multi_do_matcen_mode(request, 1);
	CHECK(sent_count == 2 && sent_recipient == -1 && sent_priority == 2);
	CHECK(sent[0] == MULTI_MATCEN_MODE && sent[1] == 1 && sent[2] == MATCEN_MODE_ONE_ROUND);
	CHECK(sent[3] == 1 && sent[sizeof(request) - 1] == 1);
	CHECK(strcmp(message, "Peer set matcens to 1 round limit") == 0);
	memcpy(state, sent, sizeof(state));
	Player_num = 1;
	matcen_mode_reset_game();
	multi_do_matcen_mode(state, 0);
	matcen_mode_get_activation_counts(counts, sizeof(counts));
	CHECK(matcen_mode_get() == MATCEN_MODE_ONE_ROUND);
	CHECK(memcmp(counts, state + 3, sizeof(counts)) == 0 && sent_count == 2);
	CHECK(strcmp(message, "Matcens: 1 round limit") == 0);
	Player_num = 0;
	multi_send_matcen_mode_state_to_player(2);
	CHECK(sent_count == 3 && sent_recipient == 2 && sent_priority == 2);
	CHECK(sent_length == (int) sizeof(state) && memcmp(sent, state, sizeof(state)) == 0);
	multi_request_matcen_mode(MATCEN_MODE_PAUSED);
	CHECK(matcen_mode_get() == MATCEN_MODE_PAUSED && sent_count == 4);
	CHECK(strcmp(message, "Matcens: paused") == 0);
	multi_request_matcen_mode(MATCEN_MODE_DEFAULT);
	CHECK(matcen_mode_get() == MATCEN_MODE_DEFAULT && sent_count == 5);
}

static void check_ordinary_inactive_states(void)
{
	reset();
	observer = 1;
	multi_request_reactor_pause_toggle();
	multi_request_matcen_mode(MATCEN_MODE_PAUSED);
	CHECK(sent_count == 0);
	observer = 0;
	active = 0;
	multi_request_reactor_pause_toggle();
	CHECK(sent_count == 0);
	Game_mode = 0;
	multi_request_matcen_mode(MATCEN_MODE_ONE_ROUND);
	multi_send_matcen_mode_state_to_player(1);
	CHECK(sent_count == 0);
}

int main(void)
{
	check_reactor_round_trip();
	check_matcen_round_trip_and_join();
	check_ordinary_inactive_states();
	if (failures) return 1;
	puts("PASS: gameplay option packets, host/client state and join catchup");
	return 0;
}
