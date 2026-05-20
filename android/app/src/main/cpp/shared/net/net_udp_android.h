#ifndef DXX_ANDROID_SHARED_NET_UDP_ANDROID_H
#define DXX_ANDROID_SHARED_NET_UDP_ANDROID_H

#include "net_udp.h"

struct player;
typedef void (*android_net_udp_log_message_fn)(const char *message);
typedef void (*android_net_udp_send_direct_fn)(ubyte *data, int len, int to_player);
typedef void (*android_net_udp_update_address_fn)(int pnum, struct _sockaddr new_addr);
typedef void (*android_net_udp_log_connection_status_fn)(int pnum, int old_type);

#define ANDROID_NET_UDP_WELCOME_SLOT_CLOSED            -1
#define ANDROID_NET_UDP_WELCOME_SLOT_FULL              -2
#define ANDROID_NET_UDP_WELCOME_SLOT_ALREADY_CONNECTED -3

#ifdef __ANDROID__
void android_net_udp_mpdiag_pkt_dump(const char *label, const ubyte *buf, int len);
#endif

int android_net_udp_sockaddr_equal(const struct _sockaddr *a,
                                   const struct _sockaddr *b);
int android_net_udp_find_player_by_identity(const char *callsign,
                                            struct _sockaddr *addr, int player_count, const struct player *players,
                                            const struct netgame_info *netgame);
int android_net_udp_select_welcome_player_slot(int existing_player_num,
                                               int n_players,
                                               int max_numplayers,
                                               int numplayers,
                                               int game_flags,
                                               fix64 now,
                                               const struct player *players,
                                               const struct netgame_info *netgame,
                                               int *network_player_added);
void android_net_udp_prepare_observer_join(UDP_sequence_packet *sync_player,
                                           int *udp_sync_obsnum,
                                           int *network_send_objects,
                                           int *network_send_objnum,
                                           fix64 now,
                                           struct netgame_info *netgame);
void android_net_udp_prepare_reconnect_player(int player_num,
                                              const struct _sockaddr *new_addr,
                                              int i_am_master,
                                              struct connection_status *statuses,
                                              android_net_udp_update_address_fn update_address);
void android_net_udp_begin_welcome_sync(UDP_sequence_packet *sync_player,
                                        int player_num,
                                        uint *player_tokens,
                                        int *network_send_objects,
                                        int *network_send_objnum,
                                        fix64 now,
                                        fix64 *last_packet_time);
int android_net_udp_objnum_is_past(int objnum,
                                   int player_num,
                                   int object_owner_value,
                                   int network_send_objects,
                                   int network_send_object_mode,
                                   int network_send_objnum);
int android_net_udp_rebind_for_hosting(int *udp_socket, int *udp_bind_loopback,
                                       const char *udp_my_port, unsigned int netgame_token,
                                       void (*close_socket)(int), int (*open_socket)(int, int),
                                       android_net_udp_log_message_fn log_message);
void android_net_udp_send_p2p_reattempt_direct(unsigned int netgame_token,
                                               int player_num,
                                               const struct _sockaddr *connect_to_addr,
                                               int to_player,
                                               android_net_udp_send_direct_fn send_direct);
void android_net_udp_process_p2p_reattempt_direct(const ubyte *data,
                                                  int player_num,
                                                  int master_player_num,
                                                  int i_am_master,
                                                  int max_players,
                                                  fix64 now,
                                                  struct connection_status *statuses,
                                                  android_net_udp_log_message_fn log_message,
                                                  android_net_udp_update_address_fn update_address);
void android_net_udp_reset_proxy(int pnum,
                                 int master_player_num,
                                 int i_am_master,
                                 struct connection_status *status,
                                 android_net_udp_log_connection_status_fn log_connection_status);
void android_net_udp_reattempt_direct(int pnum,
                                      int master_player_num,
                                      int i_am_master,
                                      struct connection_status *status,
                                      fix64 now);

#endif /* DXX_ANDROID_SHARED_NET_UDP_ANDROID_H */