#include <string.h>

#include "byteswap.h"
#include "cntrlcen.h"
#include "fuelcen.h"
#include "game.h"
#include "hudmsg.h"
#include "multi.h"
#include "multi_gameplay_options.h"

#define REACTOR_PAUSE_REQUEST 0
#define REACTOR_PAUSE_STATE   1

static void multi_send_reactor_pause_state(void)
{
	multibuf[0] = (char) MULTI_REACTOR_PAUSE;
	multibuf[1] = REACTOR_PAUSE_STATE;
	multibuf[2] = Reactor_countdown_paused ? 1 : 0;
	PUT_INTEL_INT(multibuf + 3, Countdown_timer);
	multi_send_data(multibuf, 7, 2);
}

void multi_request_reactor_pause_toggle(void)
{
	if (!(Game_mode & GM_MULTI_COOP) || is_observer() ||
	    !reactor_countdown_is_active())
		return;
	if (!multi_i_am_master()) {
		multibuf[0] = (char) MULTI_REACTOR_PAUSE;
		multibuf[1] = REACTOR_PAUSE_REQUEST;
		multibuf[2] = 0;
		PUT_INTEL_INT(multibuf + 3, 0);
		multi_send_data_direct(multibuf, 7, multi_who_is_master(), 2);
		return;
	}
	if (reactor_countdown_set_paused(!Reactor_countdown_paused, Countdown_timer)) {
		HUD_init_message_literal(HM_MULTI, Reactor_countdown_paused ? "Reactor countdown paused" : "Reactor countdown resumed");
		multi_send_reactor_pause_state();
	}
}

void multi_do_reactor_pause(const ubyte *buf, int authenticated_sender)
{
	fix remaining_time;

	if (!(Game_mode & GM_MULTI_COOP) || !reactor_countdown_is_active())
		return;
	if (buf[1] == REACTOR_PAUSE_REQUEST) {
		if (!multi_i_am_master() || authenticated_sender < 0 ||
		    authenticated_sender >= N_players ||
		    Players[authenticated_sender].connected != CONNECT_PLAYING)
			return;
		if (reactor_countdown_set_paused(!Reactor_countdown_paused, Countdown_timer)) {
			HUD_init_message(HM_MULTI, "%s %s the reactor countdown",
			                 Players[authenticated_sender].callsign,
			                 Reactor_countdown_paused ? "paused" : "resumed");
			multi_send_reactor_pause_state();
		}
		return;
	}
	if (buf[1] != REACTOR_PAUSE_STATE || multi_i_am_master() ||
	    authenticated_sender != multi_who_is_master() || buf[2] > 1)
		return;
	remaining_time = GET_INTEL_INT(buf + 3);
	if (remaining_time <= 0 || remaining_time > i2f(60 * 60))
		return;
	if (reactor_countdown_set_paused(buf[2], remaining_time))
		HUD_init_message_literal(HM_MULTI, Reactor_countdown_paused ? "Reactor countdown paused" : "Reactor countdown resumed");
}

#define MATCEN_MODE_REQUEST 0
#define MATCEN_MODE_STATE   1

static const char *multi_matcen_mode_name(int mode)
{
	switch (mode) {
		case MATCEN_MODE_ONE_ROUND: return "1 round limit";
		case MATCEN_MODE_PAUSED: return "paused";
		default: return "default";
	}
}

static void multi_build_matcen_mode_state(void)
{
	multibuf[0] = (char) MULTI_MATCEN_MODE;
	multibuf[1] = MATCEN_MODE_STATE;
	multibuf[2] = (ubyte) matcen_mode_get();
	matcen_mode_get_activation_counts(multibuf + 3, MATCEN_MODE_MAX_CENTERS);
}

static void multi_send_matcen_mode_state(void)
{
	multi_build_matcen_mode_state();
	multi_send_data(multibuf, 3 + MATCEN_MODE_MAX_CENTERS, 2);
}

void multi_send_matcen_mode_state_to_player(int pnum)
{
	if (!(Game_mode & GM_MULTI) || !multi_i_am_master() || pnum < 0 ||
	    pnum >= N_players)
		return;
	multi_build_matcen_mode_state();
	multi_send_data_direct(multibuf, 3 + MATCEN_MODE_MAX_CENTERS, pnum, 2);
}

void multi_request_matcen_mode(int mode)
{
	if (!(Game_mode & GM_MULTI) || is_observer() || mode < MATCEN_MODE_DEFAULT ||
	    mode >= MATCEN_MODE_COUNT)
		return;
	if (!multi_i_am_master()) {
		memset(multibuf, 0, 3 + MATCEN_MODE_MAX_CENTERS);
		multibuf[0] = (char) MULTI_MATCEN_MODE;
		multibuf[1] = MATCEN_MODE_REQUEST;
		multibuf[2] = (ubyte) mode;
		multi_send_data_direct(multibuf, 3 + MATCEN_MODE_MAX_CENTERS,
		                       multi_who_is_master(), 2);
		return;
	}
	matcen_set_mode(mode);
	HUD_init_message(HM_MULTI, "Matcens: %s", multi_matcen_mode_name(mode));
	multi_send_matcen_mode_state();
}

void multi_do_matcen_mode(const ubyte *buf, int authenticated_sender)
{
	int mode = buf[2];

	if (!(Game_mode & GM_MULTI) || mode < MATCEN_MODE_DEFAULT ||
	    mode >= MATCEN_MODE_COUNT)
		return;
	if (buf[1] == MATCEN_MODE_REQUEST) {
		if (!multi_i_am_master() || authenticated_sender < 0 ||
		    authenticated_sender >= N_players ||
		    Players[authenticated_sender].connected != CONNECT_PLAYING)
			return;
		matcen_set_mode(mode);
		HUD_init_message(HM_MULTI, "%s set matcens to %s",
		                 Players[authenticated_sender].callsign,
		                 multi_matcen_mode_name(mode));
		multi_send_matcen_mode_state();
		return;
	}
	if (buf[1] != MATCEN_MODE_STATE || multi_i_am_master() ||
	    authenticated_sender != multi_who_is_master())
		return;
	if (matcen_restore_mode(mode, buf + 3, MATCEN_MODE_MAX_CENTERS))
		HUD_init_message(HM_MULTI, "Matcens: %s", multi_matcen_mode_name(mode));
}
