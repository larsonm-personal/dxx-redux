/*
 * Auto-join/auto-host support for Android matchmaking integration.
 * The Kotlin launcher sets these globals via JNI before the game reaches
 * the main menu. menu.c checks them on EVENT_WINDOW_ACTIVATED and
 * triggers the appropriate network action without user interaction.
 */

#ifndef DXX_ANDROID_SHARED_NET_AUTO_NET_H
#define DXX_ANDROID_SHARED_NET_AUTO_NET_H

#ifdef __ANDROID__

/* Maximum length for a host address string (IP or hostname). */
#define AUTO_NET_ADDR_LEN 128

/* --- globals (defined in auto_net.c, set by JNI) --- */

/* Non-zero when the launcher wants an automatic join. */
extern int auto_join_pending;
/* Host address to connect to (e.g. "127.0.0.1"). */
extern char auto_join_host_addr[AUTO_NET_ADDR_LEN];
/* Host port to connect to (e.g. 42430). */
extern int auto_join_host_port;
/* Local port for our socket (e.g. 42424). */
extern int auto_join_my_port;

/* Non-zero when the launcher wants to auto-host. */
extern int auto_host_pending;
/* Local port for the host socket. */
extern int auto_host_my_port;
/* Mission filename (e.g. "descent2"). */
extern char auto_host_mission[64];
/* Game mode (NETGAME_ANARCHY, NETGAME_COOPERATIVE, etc.). */
extern int auto_host_mode;
/* Max players (2-8). */
extern int auto_host_max_players;
/* Starting level number. */
extern int auto_host_level_num;
/* Difficulty (0-4). */
extern int auto_host_difficulty;
/* Host-side coop QoL default (0/1). */
extern int auto_host_coop_qol;
/* Host-side full death spew default (0/1) */
extern int auto_host_full_death_spew;
/* Host-side player spew persistence default (0/1) */
extern int auto_host_player_spew_no_expire;
/* Host-side client rewind request permission default (0/1). */
extern int auto_host_clients_can_request_rewind;

/* Callsign for auto-created pilot (shared by host and join paths).
 * When non-empty and no pilot exists, a pilot with this name is created
 * automatically before the network action starts. */
extern char auto_net_callsign[10];

/* Client identity UUID (36 chars + null). Set by JNI from
 * ClientIdentity.getInstallationId() or the server's player_id.
 * Used for save-file player matching across sessions. */
#define AUTO_NET_CLIENT_ID_LEN 37
extern char auto_net_client_id[AUTO_NET_CLIENT_ID_LEN];

/* Auto-create a pilot with auto_net_callsign if no pilot exists.
 * Called from main_menu_handler before check_auto_net(). */
int auto_create_pilot(void);

/* Called from main_menu_handler on EVENT_WINDOW_ACTIVATED.
 * Returns 1 if an auto action was started (caller should not proceed
 * with normal menu logic), 0 otherwise. */
int check_auto_net(void);

#endif /* __ANDROID__ */

#endif /* DXX_ANDROID_SHARED_NET_AUTO_NET_H */
