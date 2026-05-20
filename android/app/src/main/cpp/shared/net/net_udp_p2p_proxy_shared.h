#ifndef DXX_ANDROID_SHARED_NET_UDP_P2P_PROXY_SHARED_H
#define DXX_ANDROID_SHARED_NET_UDP_P2P_PROXY_SHARED_H

void net_udp_send_p2p_pong(int to_player, fix64 time, int direct_ping);
void net_udp_process_p2p_pong(ubyte *data, struct _sockaddr sender_addr, int data_len);
void update_address_for_player(int pnum, struct _sockaddr new_addr);
void resetProxy(int pnum);
void reattemptDirect(int pnum);
void net_udp_send_to_player(ubyte *data, int len, int to_player);
void net_udp_send_to_player_direct(ubyte *data, int len, int to_player);
void net_udp_send_to_player_proxy(ubyte *data, int data_len, int to_player, int through_player);
void net_udp_process_proxy(ubyte *data, struct _sockaddr sender_addr, int data_len);
void net_udp_p2p_ping_frame(fix64 time);
void net_udp_process_ping(ubyte *data, int data_len, struct _sockaddr sender_addr);

#endif /* DXX_ANDROID_SHARED_NET_UDP_P2P_PROXY_SHARED_H */