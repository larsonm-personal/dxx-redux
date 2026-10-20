/* Exercise the real D1 selection caller without graphics, sockets or game assets */
#include <stdio.h>
#include <string.h>

#include "inferno.h"
#include "multi.h"
#include "newmenu.h"
#include "net_udp.h"
#include "playsave.h"
#include "text.h"

int net_udp_select_players(void);
extern UDP_sequence_packet UDP_Seq;

int __real_newmenu_do1(char *title, char *subtitle, int nitems, newmenu_item *item,
                       int (*subfunction)(newmenu *, d_event *, void *),
                       void *userdata, int citem);
void __real_multi_send_message_dialog(void);

static int active, menu_result, menu_calls, message_calls;

int __wrap_newmenu_do1(char *title, char *subtitle, int nitems, newmenu_item *item,
                       int (*subfunction)(newmenu *, d_event *, void *),
                       void *userdata, int citem)
{
	if (!active)
		return __real_newmenu_do1(title, subtitle, nitems, item, subfunction, userdata, citem);
	++menu_calls;
	/* Bound the broken timeout loop so the regression fails instead of hanging */
	return menu_calls == 1 ? menu_result : -1;
}

void __wrap_multi_send_message_dialog(void)
{
	if (!active) {
		__real_multi_send_message_dialog();
		return;
	}
	++message_calls;
	Network_message_reciever = -1;
}

static int exercise(int result)
{
	memset(&Netgame, 0, sizeof(Netgame));
	memset(&UDP_Seq, 0, sizeof(UDP_Seq));
	memset(Players, 0, sizeof(Players));
	N_players = 1;
	Player_num = Multi_master_playernum = 0;
	PlayerCfg.NoRankings = 1;
	Netgame.numplayers = 1;
	Netgame.max_numplayers = 3;
	Netgame.gamemode = NETGAME_ANARCHY;
	Network_status = NETSTAT_STARTING;
	menu_result = result;
	menu_calls = message_calls = 0;
	active = 1;
	const int selected = net_udp_select_players();
	active = 0;
	if (menu_calls != 1 || message_calls != 0 || selected != (result >= 0) ||
	    Network_status != (result < 0 ? NETSTAT_MENU : NETSTAT_STARTING) ||
	    (result >= 0 && (N_players != 1 || Players[0].connected != CONNECT_PLAYING))) {
		fprintf(stderr, "Selection return %d: menus=%d messages=%d selected=%d status=%d players=%d\n",
		        result, menu_calls, message_calls, selected, Network_status, N_players);
		return 1;
	}
	return 0;
}

int test_net_udp_selection_menu(void)
{
	/* These strings only label the stubbed menu; no text data file is loaded */
	for (int i = 0; i < N_TEXT_STRINGS; ++i)
		Text_string[i] = "test";
	int failed = 0;
	failed |= exercise(-2); /* Forced launcher timeout, formerly the disabled message sentinel */
	failed |= exercise(MAX_PLAYERS); /* A fresh start after timeout */
	failed |= exercise(-1); /* Normal Escape/cancel */
	failed |= exercise(-3); /* Other forced negative menu returns */
	failed |= exercise(0); /* Normal selected-item start */
	if (!failed)
		puts("PASS: D1 timeout/cancel returns bypass message dispatch and preserve normal start");
	return failed;
}

int __real_main(int argc, char **argv);

int __wrap_main(int argc, char **argv)
{
	if (argc == 2 && !strcmp(argv[1], "--udp-selection-menu"))
		return test_net_udp_selection_menu();
	return __real_main(argc, argv);
}
