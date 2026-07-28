#include "net_udp_android.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef __ANDROID__
#include "android_crash_handler.h"
#include "android_log.h"
#include "net_udp_reconnect_auth.h"
#include "net_udp_reconnect_jni.h"
#endif

#include "strutil.h"
#include "multi.h"
#include "player.h"

#ifdef __ANDROID__
#define ANDROID_NET_UDP_RECONNECT_PENDING_SECONDS 10

typedef struct android_net_udp_pending_reconnect {
	int active;
	int is_proxy;
	fix64 expires;
	struct _sockaddr route_addr;
	UDP_sequence_packet request;
	ubyte challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE];
} android_net_udp_pending_reconnect;

static android_net_udp_reconnect_identity
    android_net_udp_player_identities[MAX_PLAYERS + 4];
static android_net_udp_reconnect_identity android_net_udp_local_identity;
static android_net_udp_pending_reconnect
    android_net_udp_pending_reconnects[MAX_PLAYERS];

static size_t android_net_udp_auth_build_request_message(
    const UDP_sequence_packet *request, unsigned int game_token,
    ubyte *message, size_t message_size)
{
	ubyte payload[CALLSIGN_LEN + 1 + 5];
	size_t offset = 0;

	if (!request)
		return 0;
	memcpy(payload + offset, request->player.callsign, CALLSIGN_LEN + 1);
	offset += CALLSIGN_LEN + 1;
	payload[offset++] = request->player.connected;
	payload[offset++] = request->player.rank;
	payload[offset++] = request->player.color;
	payload[offset++] = request->player.missilecolor;
	payload[offset++] = request->player.observer;
	return android_net_udp_reconnect_build_request_message(
	    game_token, &request->reconnect_identity, payload, offset,
	    message, message_size);
}

uint64_t android_net_udp_read_u64_le(const ubyte *source)
{
	uint64_t value = 0;
	int i;

	for (i = 0; i < 8; i++)
		value |= (uint64_t) source[i] << (i * 8);
	return value;
}

void android_net_udp_write_u64_le(ubyte *destination,
                                  uint64_t value)
{
	int i;

	for (i = 0; i < 8; i++)
		destination[i] = (ubyte) (value >> (i * 8));
}

void android_net_udp_auth_reset(int local_player_num)
{
	ubyte random_counter[8];
	int key_len;

	memset(android_net_udp_player_identities, 0,
	       sizeof(android_net_udp_player_identities));
	memset(android_net_udp_pending_reconnects, 0,
	       sizeof(android_net_udp_pending_reconnects));
	memset(&android_net_udp_local_identity, 0,
	       sizeof(android_net_udp_local_identity));

	key_len = android_net_udp_reconnect_get_public_key(
	    android_net_udp_local_identity.public_key,
	    sizeof(android_net_udp_local_identity.public_key));
	if (key_len <= 0 ||
	    key_len > ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX)
		return;
	android_net_udp_local_identity.public_key_len = (ubyte) key_len;

	if (android_net_udp_reconnect_random(random_counter,
	                                     sizeof(random_counter))) {
		random_counter[sizeof(random_counter) - 1] &= 0x7f;
		android_net_udp_local_identity.request_counter =
		    android_net_udp_read_u64_le(random_counter);
	}
	if (!android_net_udp_local_identity.request_counter)
		android_net_udp_local_identity.request_counter = 1;

	if (local_player_num >= 0 && local_player_num < MAX_PLAYERS)
		android_net_udp_player_identities[local_player_num] =
		    android_net_udp_local_identity;
}

int android_net_udp_auth_prepare_request(UDP_sequence_packet *request,
                                         unsigned int game_token)
{
	ubyte message[160];
	size_t message_len;
	int signature_len;

	if (!request || !android_net_udp_local_identity.public_key_len)
		return 0;
	if (android_net_udp_local_identity.request_counter == UINT64_MAX)
		return 0;

	android_net_udp_local_identity.request_counter++;
	request->reconnect_identity = android_net_udp_local_identity;
	request->reconnect_identity.request_signature_len = 0;
	memset(request->reconnect_identity.request_signature, 0,
	       sizeof(request->reconnect_identity.request_signature));
	message_len = android_net_udp_auth_build_request_message(
	    request, game_token, message, sizeof(message));
	if (!message_len)
		return 0;
	signature_len = android_net_udp_reconnect_sign(
	    message, message_len,
	    request->reconnect_identity.request_signature,
	    sizeof(request->reconnect_identity.request_signature));
	if (signature_len <= 0 ||
	    signature_len > ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX)
		return 0;
	request->reconnect_identity.request_signature_len =
	    (ubyte) signature_len;
	android_net_udp_local_identity.request_counter =
	    request->reconnect_identity.request_counter;
	return 1;
}

int android_net_udp_auth_validate_request(
    const UDP_sequence_packet *request, unsigned int game_token,
    int player_count)
{
	ubyte message[160];
	size_t message_len;
	int i;

	if (!request || request->token != game_token ||
	    !android_net_udp_reconnect_identity_valid(
	        &request->reconnect_identity))
		return ANDROID_NET_UDP_AUTH_INVALID;
	message_len = android_net_udp_auth_build_request_message(
	    request, game_token, message, sizeof(message));
	if (!message_len ||
	    !android_net_udp_reconnect_verify(
	        request->reconnect_identity.public_key,
	        request->reconnect_identity.public_key_len,
	        message, message_len,
	        request->reconnect_identity.request_signature,
	        request->reconnect_identity.request_signature_len))
		return ANDROID_NET_UDP_AUTH_INVALID;

	if (player_count > MAX_PLAYERS)
		player_count = MAX_PLAYERS;
	i = android_net_udp_reconnect_find_public_key(
	    android_net_udp_player_identities, (size_t) player_count,
	    &request->reconnect_identity);
	if (i >= 0) {
		if (!android_net_udp_reconnect_counter_is_newer(
		        request->reconnect_identity.request_counter,
		        android_net_udp_player_identities[i].request_counter))
			return ANDROID_NET_UDP_AUTH_INVALID;
		android_net_udp_player_identities[i].request_counter =
		    request->reconnect_identity.request_counter;
		return i;
	}
	return -1;
}

void android_net_udp_auth_store_player(
    int player_num, const UDP_sequence_packet *request)
{
	if (!request || player_num < 0 || player_num >= MAX_PLAYERS + 4)
		return;
	android_net_udp_player_identities[player_num] =
	    request->reconnect_identity;
	android_net_udp_player_identities[player_num].request_signature_len = 0;
	memset(android_net_udp_player_identities[player_num].request_signature,
	       0,
	       sizeof(android_net_udp_player_identities[player_num]
	                  .request_signature));
}

void android_net_udp_auth_move_player(int destination_player_num,
                                      int source_player_num)
{
	if (destination_player_num < 0 ||
	    destination_player_num >= MAX_PLAYERS + 4 ||
	    source_player_num < 0 ||
	    source_player_num >= MAX_PLAYERS + 4 ||
	    destination_player_num == source_player_num)
		return;
	android_net_udp_player_identities[destination_player_num] =
	    android_net_udp_player_identities[source_player_num];
	memset(&android_net_udp_player_identities[source_player_num], 0,
	       sizeof(android_net_udp_player_identities[0]));
}

void android_net_udp_auth_remove_player(int player_num, int player_count)
{
	int i;

	if (player_num < 0 || player_num >= player_count ||
	    player_count > MAX_PLAYERS + 4)
		return;
	for (i = player_num; i < player_count - 1; i++)
		android_net_udp_player_identities[i] =
		    android_net_udp_player_identities[i + 1];
	memset(&android_net_udp_player_identities[player_count - 1], 0,
	       sizeof(android_net_udp_player_identities[0]));
}

int android_net_udp_auth_write_player(unsigned char *destination,
                                      int player_num)
{
	const android_net_udp_reconnect_identity *identity;

	if (!destination || player_num < 0 || player_num >= MAX_PLAYERS + 4)
		return 0;
	identity = &android_net_udp_player_identities[player_num];
	destination[0] = identity->public_key_len;
	memcpy(destination + 1, identity->public_key,
	       sizeof(identity->public_key));
	return ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE;
}

int android_net_udp_auth_read_player(const unsigned char *source,
                                     int player_num)
{
	android_net_udp_reconnect_identity *identity;

	if (!source || player_num < 0 || player_num >= MAX_PLAYERS + 4)
		return 0;
	identity = &android_net_udp_player_identities[player_num];
	memset(identity, 0, sizeof(*identity));
	identity->public_key_len = source[0];
	if (identity->public_key_len >
	    ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX) {
		identity->public_key_len = 0;
		return ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE;
	}
	memcpy(identity->public_key, source + 1,
	       sizeof(identity->public_key));
	return ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE;
}

int android_net_udp_auth_begin_challenge(
    int player_num, const UDP_sequence_packet *request,
    unsigned int game_token, const struct _sockaddr *route_addr,
    int is_proxy, fix64 now, unsigned char *packet, int packet_size)
{
	android_net_udp_pending_reconnect *pending;

	if (!request || !route_addr || !packet ||
	    player_num < 0 || player_num >= MAX_PLAYERS ||
	    packet_size < ANDROID_NET_UDP_RECONNECT_CHALLENGE_PACKET_SIZE)
		return 0;
	pending = &android_net_udp_pending_reconnects[player_num];
	if (!android_net_udp_reconnect_random(
	        pending->challenge, sizeof(pending->challenge)))
		return 0;
	pending->active = 1;
	pending->is_proxy = is_proxy;
	pending->expires =
	    now + F1_0 * ANDROID_NET_UDP_RECONNECT_PENDING_SECONDS;
	pending->route_addr = *route_addr;
	pending->request = *request;

	memset(packet, 0,
	       ANDROID_NET_UDP_RECONNECT_CHALLENGE_PACKET_SIZE);
	packet[0] = UPID_RECONNECT_CHALLENGE;
	PUT_INTEL_INT(packet + 1, game_token);
	packet[5] = (ubyte) player_num;
	memcpy(packet + 6, pending->challenge,
	       sizeof(pending->challenge));
	return ANDROID_NET_UDP_RECONNECT_CHALLENGE_PACKET_SIZE;
}

int android_net_udp_auth_answer_challenge(
    const unsigned char *data, int data_len, unsigned int game_token,
    int local_player_num, unsigned char *packet, int packet_size)
{
	ubyte message[80];
	size_t message_len;
	int signature_len;

	if (!data || !packet ||
	    data_len != ANDROID_NET_UDP_RECONNECT_CHALLENGE_PACKET_SIZE ||
	    packet_size < ANDROID_NET_UDP_RECONNECT_PROOF_PACKET_SIZE ||
	    GET_INTEL_INT(data + 1) != game_token ||
	    data[5] != local_player_num)
		return 0;
	message_len = android_net_udp_reconnect_build_challenge_message(
	    game_token, data[5], data + 6, message, sizeof(message));
	if (!message_len)
		return 0;

	memset(packet, 0, ANDROID_NET_UDP_RECONNECT_PROOF_PACKET_SIZE);
	packet[0] = UPID_RECONNECT_PROOF;
	PUT_INTEL_INT(packet + 1, game_token);
	packet[5] = data[5];
	memcpy(packet + 6, data + 6,
	       ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE);
	signature_len = android_net_udp_reconnect_sign(
	    message, message_len, packet + 39,
	    ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX);
	if (signature_len <= 0 ||
	    signature_len > ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX)
		return 0;
	packet[38] = (ubyte) signature_len;
	return ANDROID_NET_UDP_RECONNECT_PROOF_PACKET_SIZE;
}

int android_net_udp_auth_finish_challenge(
    const unsigned char *data, int data_len, unsigned int game_token,
    const struct _sockaddr *route_addr, int is_proxy, fix64 now,
    UDP_sequence_packet *request, int *player_num)
{
	android_net_udp_pending_reconnect *pending;
	ubyte message[80];
	size_t message_len;
	int slot;
	int signature_len;

	if (!data || !route_addr || !request || !player_num ||
	    data_len != ANDROID_NET_UDP_RECONNECT_PROOF_PACKET_SIZE ||
	    GET_INTEL_INT(data + 1) != game_token)
		return 0;
	slot = data[5];
	if (slot < 0 || slot >= MAX_PLAYERS)
		return 0;
	pending = &android_net_udp_pending_reconnects[slot];
	signature_len = data[38];
	if (!pending->active || now > pending->expires ||
	    pending->is_proxy != is_proxy ||
	    !android_net_udp_sockaddr_equal(&pending->route_addr,
	                                    route_addr) ||
	    signature_len <= 0 ||
	    signature_len > ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX ||
	    !android_net_udp_reconnect_bytes_equal(
	        pending->challenge, data + 6,
	        ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE))
		return 0;
	message_len = android_net_udp_reconnect_build_challenge_message(
	    game_token, (ubyte) slot, pending->challenge,
	    message, sizeof(message));
	if (!message_len ||
	    !android_net_udp_reconnect_verify(
	        android_net_udp_player_identities[slot].public_key,
	        android_net_udp_player_identities[slot].public_key_len,
	        message, message_len, data + 39, signature_len))
		return 0;

	*request = pending->request;
	*player_num = slot;
	memset(pending, 0, sizeof(*pending));
	return 1;
}
#endif

#ifdef __ANDROID__
void android_net_udp_mpdiag_pkt_dump(const char *label, const ubyte *buf, int len)
{
	static char msg[4096];
	int pos;
	int i;

	crash_breadcrumb_v("pktdump: %s len=%d", label, len);
	pos = snprintf(msg, sizeof(msg), "%s len=%d ", label, len);
	for (i = 0; i < len && pos + 2 < (int) sizeof(msg); i++)
		pos += snprintf(msg + pos, sizeof(msg) - pos, "%02x", buf[i]);
	crash_breadcrumb("pktdump: hex done, calling debug_log");
	debug_log(DLOG_NETWORK, "[PKTDUMP] %s", msg);
	crash_breadcrumb("pktdump: done");
}
#endif

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
		    android_net_udp_sockaddr_equal(
		        addr,
		        (struct _sockaddr *) &netgame->players[i].protocol.udp.addr))
			return i;
	}
	return -1;
}

int android_net_udp_select_welcome_player_slot(int existing_player_num,
                                               int n_players,
                                               int max_numplayers,
                                               int numplayers,
                                               int game_flags,
                                               fix64 now,
                                               const struct player *players,
                                               const struct netgame_info *netgame,
                                               int *network_player_added)
{
	int i;
	int oldest_player = -1;
	int activeplayers = 0;
	fix64 oldest_time = now;

	if (network_player_added)
		*network_player_added = 0;

	if (existing_player_num != -1) {
		if (players[existing_player_num].connected == CONNECT_PLAYING)
			return ANDROID_NET_UDP_WELCOME_SLOT_ALREADY_CONNECTED;
		return existing_player_num;
	}

	if (!(game_flags & NETGAME_FLAG_CLOSED) && (n_players < max_numplayers)) {
		if (network_player_added)
			*network_player_added = 1;
		return n_players;
	}

	if (game_flags & NETGAME_FLAG_CLOSED)
		return ANDROID_NET_UDP_WELCOME_SLOT_CLOSED;

	for (i = 0; i < numplayers; i++)
		if (netgame->players[i].connected)
			activeplayers++;

	if (activeplayers == max_numplayers)
		return ANDROID_NET_UDP_WELCOME_SLOT_FULL;

	for (i = 0; i < n_players; i++) {
		if (!players[i].connected &&
		    (netgame->players[i].LastPacketTime < oldest_time)) {
			oldest_time = netgame->players[i].LastPacketTime;
			oldest_player = i;
		}
	}

	if (oldest_player == -1)
		return ANDROID_NET_UDP_WELCOME_SLOT_FULL;

	if (network_player_added)
		*network_player_added = 1;
	return oldest_player;
}

void android_net_udp_prepare_observer_join(UDP_sequence_packet *sync_player,
                                           int *udp_sync_obsnum,
                                           int *network_send_objects,
                                           int *network_send_objnum,
                                           fix64 now,
                                           struct netgame_info *netgame)
{
	int obsnum = 0;

	if (!netgame || !sync_player)
		return;

	netgame->numobservers++;
	while (netgame->observers[obsnum].connected == 1)
		obsnum++;

	sync_player->player.connected = OBSERVER_PLAYER_ID;
	if (udp_sync_obsnum)
		*udp_sync_obsnum = obsnum;
	if (network_send_objects)
		*network_send_objects = 1;
	if (network_send_objnum)
		*network_send_objnum = -1;

	netgame->observers[obsnum].LastPacketTime = now;
	netgame->observers[obsnum].connected = 0;
	netgame->observers[obsnum].protocol.udp.addr = sync_player->player.protocol.udp.addr;
	strncpy((char *) &netgame->observers[obsnum].callsign, sync_player->player.callsign, 8);
}

void android_net_udp_prepare_reconnect_player(int player_num,
                                              const struct _sockaddr *new_addr,
                                              int is_proxy,
                                              int proxy_through,
                                              int i_am_master,
                                              struct connection_status *statuses,
                                              android_net_udp_update_address_fn update_address)
{
#ifdef __ANDROID__
	if (!is_proxy && new_addr && update_address)
		update_address(player_num, *new_addr);

	if (statuses && i_am_master) {
		statuses[player_num].type =
		    is_proxy ? CONNT_PROXY : CONNT_DIRECT;
		if (is_proxy)
			statuses[player_num].proxy_through = proxy_through;
	}
#else
	if (new_addr && update_address)
		update_address(player_num, *new_addr);
	(void) is_proxy;
	(void) proxy_through;
	(void) i_am_master;
	(void) statuses;
#endif
}

void android_net_udp_begin_welcome_sync(UDP_sequence_packet *sync_player,
                                        int player_num,
                                        uint *player_tokens,
                                        int *network_send_objects,
                                        int *network_send_objnum,
                                        fix64 now,
                                        fix64 *last_packet_time)
{
	if (sync_player)
		sync_player->player.connected = player_num;
#ifdef __ANDROID__
	if (sync_player)
		android_net_udp_auth_store_player(player_num, sync_player);
#endif
	if (player_tokens)
		player_tokens[player_num] = sync_player->token;
	if (network_send_objects)
		*network_send_objects = 1;
	if (network_send_objnum)
		*network_send_objnum = -1;
	if (last_packet_time)
		*last_packet_time = now;
}

int android_net_udp_objnum_is_past(int objnum,
                                   int player_num,
                                   int object_owner_value,
                                   int network_send_objects,
                                   int network_send_object_mode,
                                   int network_send_objnum)
{
	int obj_mode = !((object_owner_value == -1) || (object_owner_value == player_num));

	if (!network_send_objects)
		return 0;

	if (obj_mode > network_send_object_mode)
		return 0;
	else if (obj_mode < network_send_object_mode)
		return 1;
	else if (objnum < network_send_objnum)
		return 1;
	else
		return 0;
}

int android_net_udp_rebind_for_hosting(int *udp_socket, int *udp_bind_loopback,
                                       const char *udp_my_port, unsigned int netgame_token,
                                       void (*close_socket)(int), int (*open_socket)(int, int),
                                       android_net_udp_log_message_fn log_message)
{
	char logbuf[128];
	int port;

	if (udp_socket[0] == -1)
		return -1;
	if (!*udp_bind_loopback)
		return 0;

	port = atoi(udp_my_port);
	snprintf(logbuf, sizeof(logbuf), "rebind_for_hosting: closing loopback socket, reopening on 0.0.0.0:%d", port);
	log_message(logbuf);
	close_socket(0);
	*udp_bind_loopback = 0;
	if (open_socket(0, port) != 0) {
		snprintf(logbuf, sizeof(logbuf), "rebind_for_hosting: FAILED to reopen socket on port %d", port);
		log_message(logbuf);
		return -1;
	}
	snprintf(logbuf, sizeof(logbuf), "rebind_for_hosting: socket=%d now on 0.0.0.0:%d token=%u",
	         udp_socket[0], port, netgame_token);
	log_message(logbuf);
	return 0;
}

void android_net_udp_send_p2p_reattempt_direct(unsigned int netgame_token,
                                               int player_num,
                                               const struct _sockaddr *connect_to_addr,
                                               int to_player,
                                               android_net_udp_send_direct_fn send_direct)
{
	ubyte buf[UPID_REATTEMPT_DIRECT_SIZE];
	int len = 0;

	if (!connect_to_addr || !send_direct)
		return;

	memset(buf, 0, sizeof(buf));
	buf[len] = UPID_REATTEMPT_DIRECT;
	len++;
	PUT_INTEL_INT(buf + len, netgame_token);
	len += 4;
	buf[len] = player_num;
	len++;
	memcpy(buf + len, connect_to_addr, sizeof(*connect_to_addr));

	send_direct(buf, sizeof(buf), to_player);
}

void android_net_udp_reattempt_direct(int pnum,
                                      int master_player_num,
                                      int i_am_master,
                                      struct connection_status *status,
                                      fix64 now)
{
	if (!status)
		return;
	if (pnum == master_player_num)
		return;
	if (i_am_master)
		return;

	status->holepunch_attempts = 0;
	status->last_direct_pong = now;
}

void android_net_udp_process_p2p_reattempt_direct(const ubyte *data,
                                                  int player_num,
                                                  int master_player_num,
                                                  int i_am_master,
                                                  int max_players,
                                                  fix64 now,
                                                  struct connection_status *statuses,
                                                  android_net_udp_log_message_fn log_message,
                                                  android_net_udp_update_address_fn update_address)
{
	struct _sockaddr new_address;
	int len = 0;
	int pnum;

	len++;
	len += 4;
	pnum = data[len];
	len++;

	if (pnum == master_player_num) {
		if (log_message)
			log_message("Attempting reconnect to master, illegal.");
		return;
	}

	if (pnum == player_num) {
		if (log_message)
			log_message("Attempting reconnect to self, illegal.");
		return;
	}

	if (pnum >= max_players) {
		if (log_message)
			log_message("Attempting connection to illegal player num.");
		return;
	}

	memcpy(&new_address, data + len, sizeof(new_address));
	if (update_address)
		update_address(pnum, new_address);
	android_net_udp_reattempt_direct(pnum, master_player_num, i_am_master,
	                                 statuses ? &statuses[pnum] : NULL, now);
}

void android_net_udp_reset_proxy(int pnum,
                                 int master_player_num,
                                 int i_am_master,
                                 struct connection_status *status,
                                 android_net_udp_log_connection_status_fn log_connection_status)
{
	if (!status)
		return;
	if (pnum == master_player_num)
		return;
	if (i_am_master)
		return;
	if (log_connection_status)
		log_connection_status(pnum, status->type);
	status->type = CONNT_PROXY;
	status->proxy_through = master_player_num;
}
