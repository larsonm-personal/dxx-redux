#include "coop_host_migration.h"

#include <stdint.h>
#include <stdio.h>

#include <physfs.h>

#include "android_crash_handler.h"
#include "android_rewind.h"
#include "console.h"
#include "coop_host_migration_policy.h"
#include "coop_level_restart.h"
#include "game.h"
#include "gameseq.h"
#include "hudmsg.h"
#include "multi.h"

static void coop_host_migration_write_metadata(void)
{
	PHYSFS_file *mfp = PHYSFS_openWrite("host_migration.json");

	if (mfp) {
		char mbuf[512];
		int mlen = snprintf(mbuf, sizeof(mbuf),
		                    "{\n"
		                    "  \"callsign\": \"%s\",\n"
#ifdef DXX_BUILD_DESCENT_II
		                    "  \"game\": \"d2\",\n"
#else
		                    "  \"game\": \"d1\",\n"
#endif
		                    "  \"mission\": \"%s\",\n"
		                    "  \"mode\": \"coop\",\n"
		                    "  \"difficulty\": %d,\n"
		                    "  \"level_num\": %d,\n"
		                    "  \"max_players\": %d,\n"
		                    "  \"coop_qol\": %s,\n"
		                    "  \"duplicate_energy_shields\": %s,\n"
		                    "  \"full_death_spew\": %s,\n"
		                    "  \"player_spew_no_expire\": %s\n"
		                    "}\n",
		                    Players[Player_num].callsign,
		                    Netgame.mission_name,
		                    Netgame.difficulty,
		                    Current_level_num,
		                    Netgame.max_numplayers,
		                    (Netgame.game_flags & NETGAME_FLAG_COOP_QOL) ? "true" : "false",
		                    Netgame.DuplicateEnergyShields ? "true" : "false",
		                    Netgame.FullDeathSpew ? "true" : "false",
		                    Netgame.PlayerSpewNoExpire ? "true" : "false");

		if (mlen >= (int) sizeof(mbuf))
			mlen = (int) sizeof(mbuf) - 1;
		if (mlen > 0)
			PHYSFS_write(mfp, mbuf, mlen, 1);
		PHYSFS_close(mfp);
	}
}

int coop_host_migration_handle_disconnect(int disconnected_player)
{
	coop_host_migration_decision decision;
	int8_t connection_states[MAX_PLAYERS];
	int player_count = N_players;
	int player;

	if (!(Game_mode & GM_MULTI_COOP))
		return 0;
	if (player_count > MAX_PLAYERS)
		player_count = MAX_PLAYERS;
	for (player = 0; player < player_count; player++)
		connection_states[player] = Players[player].connected;

	decision = coop_host_migration_decide(Multi_master_playernum,
	                                      disconnected_player,
	                                      Player_num,
	                                      connection_states,
	                                      player_count,
	                                      CONNECT_PLAYING);
	if (decision.action != COOP_HOST_MIGRATION_LOCAL_HOST &&
	    decision.action != COOP_HOST_MIGRATION_REMOTE_HOST)
		return 0;

	Multi_master_playernum = decision.new_master;
	android_rewind_reset_level();
	coop_level_restart_clear();
	android_rewind_set_clients_can_request(0);
	con_printf(CON_NORMAL, "host migration: player %d is now master\n", decision.new_master);
	if (decision.action == COOP_HOST_MIGRATION_LOCAL_HOST) {
		HUD_init_message_literal(HM_MULTI, "You are now the game host");
		coop_host_migration_reset_object_owners((int8_t *) object_owner, MAX_OBJECTS);
		multi_powcap_count_powerups_in_mine();
		coop_host_migration_write_metadata();
		/* Kotlin replaces the client proxy with a host-mode loopback proxy. */
		android_notify_host_migration();
	}
	return 1;
}
