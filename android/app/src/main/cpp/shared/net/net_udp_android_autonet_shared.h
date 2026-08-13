#ifndef DXX_ANDROID_SHARED_NET_UDP_ANDROID_AUTONET_SHARED_H
#define DXX_ANDROID_SHARED_NET_UDP_ANDROID_AUTONET_SHARED_H

int net_udp_auto_join(const char *host_addr, int host_port, int my_port);
int net_udp_auto_host(int my_port, const char *mission, int mode,
                      int difficulty, int max_players, int level_num,
                      int coop_qol, int duplicate_energy_shields,
                      int full_death_spew,
                      int player_spew_no_expire);

#endif /* DXX_ANDROID_SHARED_NET_UDP_AUTONET_SHARED_H */
