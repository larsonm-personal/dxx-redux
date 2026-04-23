#ifndef DXX_ANDROID_SHARED_NET_UDP_ANDROID_H
#define DXX_ANDROID_SHARED_NET_UDP_ANDROID_H

#include "multi.h"

struct player;
struct netgame_info;
struct connection_status;
typedef void (*android_net_udp_log_message_fn)(const char *message);
typedef void (*android_net_udp_send_direct_fn)(ubyte *data, int len, int to_player);
typedef void (*android_net_udp_update_address_fn)(int pnum, struct _sockaddr new_addr);
typedef void (*android_net_udp_log_connection_status_fn)(int pnum, int old_type);

#ifdef __ANDROID__
void android_net_udp_mpdiag_pkt_dump(const char *label, const ubyte *buf, int len);
#endif

int android_net_udp_sockaddr_equal(const struct _sockaddr *a,
                                   const struct _sockaddr *b);
int android_net_udp_find_player_by_identity(const char *callsign,
                                            struct _sockaddr *addr, int player_count, const struct player *players,
                                            const struct netgame_info *netgame);
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