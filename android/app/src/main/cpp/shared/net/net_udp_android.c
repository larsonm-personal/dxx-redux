#include "net_udp_android.h"

#include <string.h>

#include "strutil.h"
#include "player.h"
#include "multi.h"
#include "net_udp.h"

static int android_net_udp_sockaddr_ip_equal(const struct _sockaddr *a,
                                             const struct _sockaddr *b)
{
#ifdef IPv6
	return memcmp(&a->sin6_addr, &b->sin6_addr, sizeof(a->sin6_addr)) == 0;
#else
	return a->sin_addr.s_addr == b->sin_addr.s_addr;
#endif
}

int android_net_udp_sockaddr_equal(const struct _sockaddr *a,
                                   const struct _sockaddr *b)
{
	const struct sockaddr_in *sa = (const struct sockaddr_in *) a;
	const struct sockaddr_in *sb = (const struct sockaddr_in *) b;

	return sa->sin_addr.s_addr == sb->sin_addr.s_addr &&
	       sa->sin_port == sb->sin_port;
}

int android_net_udp_find_player_by_identity(const char *callsign,
                                            struct _sockaddr *addr, int player_count, const struct player *players,
                                            const struct netgame_info *netgame)
{
	int i;

	for (i = 0; i < player_count; i++) {
		if (!d_stricmp(players[i].callsign, callsign) &&
		    android_net_udp_sockaddr_ip_equal(addr,
		                                      (struct _sockaddr *) &netgame->players[i].protocol.udp.addr))
			return i;
	}
#ifdef __ANDROID__
	for (i = 0; i < player_count; i++) {
		if (!players[i].connected &&
		    !d_stricmp(players[i].callsign, callsign))
			return i;
	}
#endif
	return -1;
}