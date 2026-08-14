#ifndef DXX_ANDROID_NET_UDP_RECONNECT_AUTH_H
#define DXX_ANDROID_NET_UDP_RECONNECT_AUTH_H

#include <stddef.h>
#include <stdint.h>

#define ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX   65
#define ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX    80
#define ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE   32
#define ANDROID_NET_UDP_RECONNECT_GENERATION_SIZE  16
#define ANDROID_NET_UDP_RECONNECT_ROUTE_MAX        16
#define ANDROID_NET_UDP_RECONNECT_PROTOCOL_VERSION 2

#define ANDROID_NET_UDP_RECONNECT_GAME_D1 1
#define ANDROID_NET_UDP_RECONNECT_GAME_D2 2

#define ANDROID_NET_UDP_RECONNECT_SOURCE_KEY_SIZE        4
#define ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_MAX   32
#define ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST 4
#define ANDROID_NET_UDP_RECONNECT_ADMISSION_GLOBAL_BURST 16

#define ANDROID_NET_UDP_RECONNECT_SEQUENCE_AUTH_SIZE     \
	(2 + ANDROID_NET_UDP_RECONNECT_GENERATION_SIZE + 1 + \
	 ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX + 8 + 1 +  \
	 ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX)
#define ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE       \
	(2 + ANDROID_NET_UDP_RECONNECT_GENERATION_SIZE + 1 + \
	 ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX + 8)

#define ANDROID_NET_UDP_RECONNECT_CHALLENGE_PACKET_SIZE \
	(1 + 4 + 1 + ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE)
#define ANDROID_NET_UDP_RECONNECT_PROOF_PACKET_SIZE             \
	(1 + 4 + 1 + ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE + 1 + \
	 ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX)

typedef struct android_net_udp_reconnect_identity {
	uint8_t protocol_version;
	uint8_t game_kind;
	uint8_t generation_nonce[ANDROID_NET_UDP_RECONNECT_GENERATION_SIZE];
	uint8_t public_key_len;
	uint8_t public_key[ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX];
	uint64_t request_counter;
	uint8_t request_signature_len;
	uint8_t request_signature[ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX];
} android_net_udp_reconnect_identity;

typedef struct android_net_udp_reconnect_route_proof {
	int active;
	int is_proxy;
	int context;
	int64_t expires;
	uint64_t request_counter;
	size_t route_size;
	uint8_t route[ANDROID_NET_UDP_RECONNECT_ROUTE_MAX];
	uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE];
} android_net_udp_reconnect_route_proof;

typedef struct android_net_udp_reconnect_admission_source {
	int active;
	int64_t window_started;
	int64_t last_seen;
	unsigned int attempts;
	uint8_t key[ANDROID_NET_UDP_RECONNECT_SOURCE_KEY_SIZE];
} android_net_udp_reconnect_admission_source;

typedef struct android_net_udp_reconnect_admission {
	int64_t window_ticks;
	int64_t global_window_started;
	unsigned int global_attempts;
	android_net_udp_reconnect_admission_source
	    sources[ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_MAX];
} android_net_udp_reconnect_admission;

typedef int (*android_net_udp_reconnect_verify_fn)(void *context);

size_t android_net_udp_reconnect_build_request_message(
    uint32_t game_token,
    const android_net_udp_reconnect_identity *identity,
    const uint8_t *request_payload, size_t request_payload_size,
    uint8_t *message, size_t message_size);
size_t android_net_udp_reconnect_build_challenge_message(
    uint32_t game_token, uint8_t player_num,
    const android_net_udp_reconnect_identity *identity,
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
int android_net_udp_reconnect_context_valid(
    const android_net_udp_reconnect_identity *identity);
int android_net_udp_reconnect_context_equal(
    const android_net_udp_reconnect_identity *a,
    const android_net_udp_reconnect_identity *b);
int android_net_udp_reconnect_write_sequence_identity(
    uint8_t *destination, size_t destination_size,
    const android_net_udp_reconnect_identity *identity);
int android_net_udp_reconnect_read_sequence_identity(
    const uint8_t *source, size_t source_size,
    android_net_udp_reconnect_identity *identity);
int android_net_udp_reconnect_write_player_identity(
    uint8_t *destination, size_t destination_size,
    const android_net_udp_reconnect_identity *identity);
int android_net_udp_reconnect_read_player_identity(
    const uint8_t *source, size_t source_size,
    android_net_udp_reconnect_identity *identity);
int android_net_udp_reconnect_stage_route_proof(
    android_net_udp_reconnect_route_proof *proof,
    uint64_t accepted_counter, uint64_t request_counter,
    const uint8_t *route, size_t route_size, int is_proxy,
    int context, int64_t expires,
    const uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE]);
int android_net_udp_reconnect_route_claim_matches(
    const android_net_udp_reconnect_route_proof *proof,
    const uint8_t *route, size_t route_size, int is_proxy,
    int context, int64_t now,
    const uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE]);
int android_net_udp_reconnect_commit_route_proof(
    android_net_udp_reconnect_route_proof *proof,
    uint64_t *accepted_counter);
void android_net_udp_reconnect_admission_reset(
    android_net_udp_reconnect_admission *admission,
    int64_t window_ticks);
int android_net_udp_reconnect_verify_admitted(
    android_net_udp_reconnect_admission *admission,
    const uint8_t *route, size_t route_size, int64_t now,
    android_net_udp_reconnect_verify_fn verify, void *context);
int android_net_udp_reconnect_bytes_equal(const uint8_t *a,
                                          const uint8_t *b, size_t size);

#endif /* DXX_ANDROID_NET_UDP_RECONNECT_AUTH_H */
