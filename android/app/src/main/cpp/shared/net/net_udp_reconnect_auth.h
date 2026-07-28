#ifndef DXX_ANDROID_NET_UDP_RECONNECT_AUTH_H
#define DXX_ANDROID_NET_UDP_RECONNECT_AUTH_H

#include <stddef.h>
#include <stdint.h>

#define ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX 65
#define ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX  80
#define ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE 32

#define ANDROID_NET_UDP_RECONNECT_SEQUENCE_AUTH_SIZE \
	(1 + ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX + 8 + 1 + ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX)
#define ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE \
	(1 + ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX)

#define ANDROID_NET_UDP_RECONNECT_CHALLENGE_PACKET_SIZE \
	(1 + 4 + 1 + ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE)
#define ANDROID_NET_UDP_RECONNECT_PROOF_PACKET_SIZE             \
	(1 + 4 + 1 + ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE + 1 + \
	 ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX)

typedef struct android_net_udp_reconnect_identity {
	uint8_t public_key_len;
	uint8_t public_key[ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX];
	uint64_t request_counter;
	uint8_t request_signature_len;
	uint8_t request_signature[ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX];
} android_net_udp_reconnect_identity;

size_t android_net_udp_reconnect_build_request_message(
    uint32_t game_token,
    const android_net_udp_reconnect_identity *identity,
    const uint8_t *request_payload, size_t request_payload_size,
    uint8_t *message, size_t message_size);
size_t android_net_udp_reconnect_build_challenge_message(
    uint32_t game_token, uint8_t player_num,
    const uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE],
    uint8_t *message, size_t message_size);
int android_net_udp_reconnect_identity_valid(
    const android_net_udp_reconnect_identity *identity);
int android_net_udp_reconnect_public_key_equal(
    const android_net_udp_reconnect_identity *a,
    const android_net_udp_reconnect_identity *b);
int android_net_udp_reconnect_find_public_key(
    const android_net_udp_reconnect_identity *identities,
    size_t identity_count,
    const android_net_udp_reconnect_identity *candidate);
int android_net_udp_reconnect_counter_is_newer(uint64_t candidate,
                                               uint64_t previous);
int android_net_udp_reconnect_bytes_equal(const uint8_t *a,
                                          const uint8_t *b, size_t size);

#endif /* DXX_ANDROID_NET_UDP_RECONNECT_AUTH_H */
