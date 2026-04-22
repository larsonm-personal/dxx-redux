#ifndef DXX_ANDROID_SHARED_NET_UDP_ANDROID_H
#define DXX_ANDROID_SHARED_NET_UDP_ANDROID_H

#include "multi.h"

struct player;
struct netgame_info;

int android_net_udp_sockaddr_equal(const struct _sockaddr *a,
                                   const struct _sockaddr *b);
int android_net_udp_find_player_by_identity(const char *callsign,
                                            struct _sockaddr *addr, int player_count, const struct player *players,
                                            const struct netgame_info *netgame);

#endif /* DXX_ANDROID_SHARED_NET_UDP_ANDROID_H */