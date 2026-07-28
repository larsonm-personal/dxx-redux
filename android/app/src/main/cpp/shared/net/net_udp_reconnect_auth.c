#include "net_udp_reconnect_auth.h"

#include <limits.h>
#include <string.h>

static const uint8_t request_domain[] = "DXX-UDP-RECONNECT-REQUEST-v1";
static const uint8_t challenge_domain[] = "DXX-UDP-RECONNECT-CHALLENGE-v1";

static void write_u32_le(uint8_t *destination, uint32_t value)
{
	destination[0] = (uint8_t) value;
	destination[1] = (uint8_t) (value >> 8);
	destination[2] = (uint8_t) (value >> 16);
	destination[3] = (uint8_t) (value >> 24);
}

static void write_u16_le(uint8_t *destination, uint16_t value)
{
	destination[0] = (uint8_t) value;
	destination[1] = (uint8_t) (value >> 8);
}

static void write_u64_le(uint8_t *destination, uint64_t value)
{
	int i;
	for (i = 0; i < 8; i++)
		destination[i] = (uint8_t) (value >> (i * 8));
}

size_t android_net_udp_reconnect_build_request_message(
    uint32_t game_token,
    const android_net_udp_reconnect_identity *identity,
    const uint8_t *request_payload, size_t request_payload_size,
    uint8_t *message, size_t message_size)
{
	size_t required;
	size_t offset = 0;

	if (!message || !identity || !request_payload ||
	    request_payload_size > UINT16_MAX ||
	    identity->public_key_len == 0 ||
	    identity->public_key_len > ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX ||
	    request_payload_size > SIZE_MAX - (sizeof(request_domain) - 1 + 4 +
	                                       8 + 1 + 2 +
	                                       identity->public_key_len))
		return 0;
	required = sizeof(request_domain) - 1 + 4 + 8 + 1 +
	           identity->public_key_len + 2 + request_payload_size;
	if (message_size < required)
		return 0;

	memcpy(message + offset, request_domain, sizeof(request_domain) - 1);
	offset += sizeof(request_domain) - 1;
	write_u32_le(message + offset, game_token);
	offset += 4;
	write_u64_le(message + offset, identity->request_counter);
	offset += 8;
	message[offset++] = identity->public_key_len;
	memcpy(message + offset, identity->public_key, identity->public_key_len);
	offset += identity->public_key_len;
	write_u16_le(message + offset, (uint16_t) request_payload_size);
	offset += 2;
	memcpy(message + offset, request_payload, request_payload_size);
	return required;
}

size_t android_net_udp_reconnect_build_challenge_message(
    uint32_t game_token, uint8_t player_num,
    const uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE],
    uint8_t *message, size_t message_size)
{
	const size_t required = sizeof(challenge_domain) - 1 + 4 + 1 +
	                        ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE;
	size_t offset = 0;

	if (!message || !challenge || message_size < required)
		return 0;

	memcpy(message + offset, challenge_domain, sizeof(challenge_domain) - 1);
	offset += sizeof(challenge_domain) - 1;
	write_u32_le(message + offset, game_token);
	offset += 4;
	message[offset++] = player_num;
	memcpy(message + offset, challenge,
	       ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE);
	return required;
}

int android_net_udp_reconnect_identity_valid(
    const android_net_udp_reconnect_identity *identity)
{
	return identity && identity->public_key_len > 0 &&
	       identity->public_key_len <= ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX &&
	       identity->request_signature_len > 0 &&
	       identity->request_signature_len <= ANDROID_NET_UDP_RECONNECT_SIGNATURE_MAX;
}

int android_net_udp_reconnect_public_key_equal(
    const android_net_udp_reconnect_identity *a,
    const android_net_udp_reconnect_identity *b)
{
	return a && b && a->public_key_len > 0 &&
	       a->public_key_len == b->public_key_len &&
	       android_net_udp_reconnect_bytes_equal(a->public_key, b->public_key,
	                                             a->public_key_len);
}

int android_net_udp_reconnect_find_public_key(
    const android_net_udp_reconnect_identity *identities,
    size_t identity_count,
    const android_net_udp_reconnect_identity *candidate)
{
	size_t i;

	if (!identities || !candidate || identity_count > INT_MAX)
		return -1;
	for (i = 0; i < identity_count; i++)
		if (android_net_udp_reconnect_public_key_equal(
		        &identities[i], candidate))
			return (int) i;
	return -1;
}

int android_net_udp_reconnect_counter_is_newer(uint64_t candidate,
                                               uint64_t previous)
{
	return candidate > previous;
}

int android_net_udp_reconnect_bytes_equal(const uint8_t *a,
                                          const uint8_t *b, size_t size)
{
	uint8_t difference = 0;
	size_t i;

	if (!a || !b)
		return 0;
	for (i = 0; i < size; i++)
		difference |= a[i] ^ b[i];
	return difference == 0;
}
