#ifndef DXX_ANDROID_SHARED_NET_UDP_ANDROID_H
#define DXX_ANDROID_SHARED_NET_UDP_ANDROID_H

#include "multi.h"

struct player;
struct netgame_info;
typedef void (*android_net_udp_log_message_fn)(const char *message);

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

#endif /* DXX_ANDROID_SHARED_NET_UDP_ANDROID_H */