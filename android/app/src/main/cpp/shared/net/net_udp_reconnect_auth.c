#include "net_udp_reconnect_auth.h"

#include <limits.h>
#include <string.h>

static const uint8_t transcript_domain[] = "DXX-UDP-RECONNECT";

#define ANDROID_NET_UDP_RECONNECT_ROLE_REQUEST   1
#define ANDROID_NET_UDP_RECONNECT_ROLE_CHALLENGE 2

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

static uint64_t read_u64_le(const uint8_t *source)
{
	uint64_t value = 0;
	int i;

	for (i = 0; i < 8; i++)
		value |= (uint64_t) source[i] << (i * 8);
	return value;
}

static size_t write_transcript_prefix(
    uint8_t role, const android_net_udp_reconnect_identity *identity,
    uint8_t *message, size_t message_size)
{
	const size_t required = sizeof(transcript_domain) - 1 + 3 +
	                        ANDROID_NET_UDP_RECONNECT_GENERATION_SIZE;
	size_t offset = 0;

	if (!message || message_size < required ||
	    !android_net_udp_reconnect_context_valid(identity))
		return 0;
	memcpy(message + offset, transcript_domain,
	       sizeof(transcript_domain) - 1);
	offset += sizeof(transcript_domain) - 1;
	message[offset++] = identity->protocol_version;
	message[offset++] = identity->game_kind;
	message[offset++] = role;
	memcpy(message + offset, identity->generation_nonce,
	       sizeof(identity->generation_nonce));
	return required;
}

size_t android_net_udp_reconnect_build_request_message(
    uint32_t game_token,
    const android_net_udp_reconnect_identity *identity,
    const uint8_t *request_payload, size_t request_payload_size,
    uint8_t *message, size_t message_size)
{
	size_t required;
	size_t offset;
	const size_t prefix_size = sizeof(transcript_domain) - 1 + 3 +
	                           ANDROID_NET_UDP_RECONNECT_GENERATION_SIZE;

	if (!message || !identity || !request_payload ||
	    request_payload_size > UINT16_MAX ||
	    !android_net_udp_reconnect_context_valid(identity) ||
	    identity->public_key_len == 0 ||
	    identity->public_key_len > ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX ||
	    request_payload_size > SIZE_MAX - (prefix_size + 4 + 8 + 1 + 2 +
	                                       identity->public_key_len))
		return 0;
	required = prefix_size + 4 + 8 + 1 +
	           identity->public_key_len + 2 + request_payload_size;
	if (message_size < required)
		return 0;

	offset = write_transcript_prefix(
	    ANDROID_NET_UDP_RECONNECT_ROLE_REQUEST, identity,
	    message, message_size);
	if (!offset)
		return 0;
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
    const android_net_udp_reconnect_identity *identity,
    const uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE],
    uint8_t *message, size_t message_size)
{
	const size_t required = sizeof(transcript_domain) - 1 + 3 +
	                        ANDROID_NET_UDP_RECONNECT_GENERATION_SIZE + 4 + 1 +
	                        ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE;
	size_t offset;

	if (!message || !challenge || message_size < required ||
	    !android_net_udp_reconnect_context_valid(identity))
		return 0;

	offset = write_transcript_prefix(
	    ANDROID_NET_UDP_RECONNECT_ROLE_CHALLENGE, identity,
	    message, message_size);
	if (!offset)
		return 0;
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
	return android_net_udp_reconnect_context_valid(identity) &&
	       identity->public_key_len > 0 &&
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

int android_net_udp_reconnect_context_valid(
    const android_net_udp_reconnect_identity *identity)
{
	uint8_t nonce_difference = 0;
	size_t i;

	if (!identity ||
	    identity->protocol_version !=
	        ANDROID_NET_UDP_RECONNECT_PROTOCOL_VERSION ||
	    (identity->game_kind != ANDROID_NET_UDP_RECONNECT_GAME_D1 &&
	     identity->game_kind != ANDROID_NET_UDP_RECONNECT_GAME_D2))
		return 0;
	for (i = 0; i < sizeof(identity->generation_nonce); i++)
		nonce_difference |= identity->generation_nonce[i];
	return nonce_difference != 0;
}

int android_net_udp_reconnect_context_equal(
    const android_net_udp_reconnect_identity *a,
    const android_net_udp_reconnect_identity *b)
{
	return android_net_udp_reconnect_context_valid(a) &&
	       android_net_udp_reconnect_context_valid(b) &&
	       a->protocol_version == b->protocol_version &&
	       a->game_kind == b->game_kind &&
	       android_net_udp_reconnect_bytes_equal(
	           a->generation_nonce, b->generation_nonce,
	           sizeof(a->generation_nonce));
}

int android_net_udp_reconnect_write_sequence_identity(
    uint8_t *destination, size_t destination_size,
    const android_net_udp_reconnect_identity *identity)
{
	size_t offset = 0;

	if (!destination || !identity ||
	    destination_size < ANDROID_NET_UDP_RECONNECT_SEQUENCE_AUTH_SIZE)
		return 0;
	destination[offset++] = identity->protocol_version;
	destination[offset++] = identity->game_kind;
	memcpy(destination + offset, identity->generation_nonce,
	       sizeof(identity->generation_nonce));
	offset += sizeof(identity->generation_nonce);
	destination[offset++] = identity->public_key_len;
	memcpy(destination + offset, identity->public_key,
	       sizeof(identity->public_key));
	offset += sizeof(identity->public_key);
	write_u64_le(destination + offset, identity->request_counter);
	offset += 8;
	destination[offset++] = identity->request_signature_len;
	memcpy(destination + offset, identity->request_signature,
	       sizeof(identity->request_signature));
	return ANDROID_NET_UDP_RECONNECT_SEQUENCE_AUTH_SIZE;
}

int android_net_udp_reconnect_read_sequence_identity(
    const uint8_t *source, size_t source_size,
    android_net_udp_reconnect_identity *identity)
{
	size_t offset = 0;

	if (!source || !identity ||
	    source_size < ANDROID_NET_UDP_RECONNECT_SEQUENCE_AUTH_SIZE)
		return 0;
	memset(identity, 0, sizeof(*identity));
	identity->protocol_version = source[offset++];
	identity->game_kind = source[offset++];
	memcpy(identity->generation_nonce, source + offset,
	       sizeof(identity->generation_nonce));
	offset += sizeof(identity->generation_nonce);
	identity->public_key_len = source[offset++];
	memcpy(identity->public_key, source + offset,
	       sizeof(identity->public_key));
	offset += sizeof(identity->public_key);
	identity->request_counter = read_u64_le(source + offset);
	offset += 8;
	identity->request_signature_len = source[offset++];
	memcpy(identity->request_signature, source + offset,
	       sizeof(identity->request_signature));
	return ANDROID_NET_UDP_RECONNECT_SEQUENCE_AUTH_SIZE;
}

int android_net_udp_reconnect_write_player_identity(
    uint8_t *destination, size_t destination_size,
    const android_net_udp_reconnect_identity *identity)
{
	if (!destination || !identity ||
	    destination_size < ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE ||
	    identity->public_key_len > ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX)
		return 0;
	destination[0] = identity->protocol_version;
	destination[1] = identity->game_kind;
	memcpy(destination + 2, identity->generation_nonce,
	       sizeof(identity->generation_nonce));
	destination[2 + sizeof(identity->generation_nonce)] =
	    identity->public_key_len;
	memcpy(destination + 3 + sizeof(identity->generation_nonce),
	       identity->public_key,
	       sizeof(identity->public_key));
	write_u64_le(destination + 3 + sizeof(identity->generation_nonce) +
	                 sizeof(identity->public_key),
	             identity->request_counter);
	return ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE;
}

int android_net_udp_reconnect_read_player_identity(
    const uint8_t *source, size_t source_size,
    android_net_udp_reconnect_identity *identity)
{
	if (!source || !identity ||
	    source_size < ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE)
		return 0;
	memset(identity, 0, sizeof(*identity));
	identity->protocol_version = source[0];
	identity->game_kind = source[1];
	memcpy(identity->generation_nonce, source + 2,
	       sizeof(identity->generation_nonce));
	identity->public_key_len =
	    source[2 + sizeof(identity->generation_nonce)];
	if (identity->public_key_len >
	    ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX) {
		identity->public_key_len = 0;
		return ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE;
	}
	memcpy(identity->public_key,
	       source + 3 + sizeof(identity->generation_nonce),
	       sizeof(identity->public_key));
	identity->request_counter = read_u64_le(
	    source + 3 + sizeof(identity->generation_nonce) +
	    sizeof(identity->public_key));
	return ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE;
}

int android_net_udp_reconnect_stage_route_proof(
    android_net_udp_reconnect_route_proof *proof,
    uint64_t accepted_counter, uint64_t request_counter,
    const uint8_t *route, size_t route_size, int is_proxy,
    int context, int64_t expires,
    const uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE])
{
	if (!proof || !route || !challenge || route_size == 0 ||
	    route_size > sizeof(proof->route) ||
	    !android_net_udp_reconnect_counter_is_newer(
	        request_counter, accepted_counter))
		return 0;
	memset(proof, 0, sizeof(*proof));
	proof->active = 1;
	proof->is_proxy = !!is_proxy;
	proof->context = context;
	proof->expires = expires;
	proof->request_counter = request_counter;
	proof->route_size = route_size;
	memcpy(proof->route, route, route_size);
	memcpy(proof->challenge, challenge, sizeof(proof->challenge));
	return 1;
}

int android_net_udp_reconnect_route_claim_matches(
    const android_net_udp_reconnect_route_proof *proof,
    const uint8_t *route, size_t route_size, int is_proxy,
    int context, int64_t now,
    const uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE])
{
	return proof && proof->active && route && challenge &&
	       now <= proof->expires && proof->is_proxy == !!is_proxy &&
	       proof->context == context && proof->route_size == route_size &&
	       android_net_udp_reconnect_bytes_equal(
	           proof->route, route, route_size) &&
	       android_net_udp_reconnect_bytes_equal(
	           proof->challenge, challenge, sizeof(proof->challenge));
}

int android_net_udp_reconnect_commit_route_proof(
    android_net_udp_reconnect_route_proof *proof,
    uint64_t *accepted_counter)
{
	if (!proof || !proof->active || !accepted_counter ||
	    !android_net_udp_reconnect_counter_is_newer(
	        proof->request_counter, *accepted_counter))
		return 0;
	*accepted_counter = proof->request_counter;
	memset(proof, 0, sizeof(*proof));
	return 1;
}

void android_net_udp_reconnect_admission_reset(
    android_net_udp_reconnect_admission *admission,
    int64_t window_ticks)
{
	if (!admission)
		return;
	memset(admission, 0, sizeof(*admission));
	admission->window_ticks = window_ticks;
}

static int admission_window_expired(int64_t started, int64_t now,
                                    int64_t window_ticks)
{
	return now < started || now - started >= window_ticks;
}

static android_net_udp_reconnect_admission_source *admission_find_source(
    android_net_udp_reconnect_admission *admission,
    const uint8_t *route, int64_t now)
{
	android_net_udp_reconnect_admission_source *oldest = NULL;
	size_t i;

	for (i = 0; i < ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_MAX; i++) {
		android_net_udp_reconnect_admission_source *source =
		    &admission->sources[i];
		if (source->active && android_net_udp_reconnect_bytes_equal(
		                          source->key, route,
		                          sizeof(source->key)))
			return source;
		if (!source->active)
			oldest = source;
		else if (!oldest || source->last_seen < oldest->last_seen)
			oldest = source;
	}

	memset(oldest, 0, sizeof(*oldest));
	oldest->active = 1;
	oldest->window_started = now;
	oldest->last_seen = now;
	memcpy(oldest->key, route, sizeof(oldest->key));
	return oldest;
}

int android_net_udp_reconnect_verify_admitted(
    android_net_udp_reconnect_admission *admission,
    const uint8_t *route, size_t route_size, int64_t now,
    android_net_udp_reconnect_verify_fn verify, void *context)
{
	android_net_udp_reconnect_admission_source *source;

	if (!admission || !route ||
	    route_size < ANDROID_NET_UDP_RECONNECT_SOURCE_KEY_SIZE ||
	    admission->window_ticks <= 0 || !verify)
		return 0;
	if (admission_window_expired(admission->global_window_started, now,
	                             admission->window_ticks)) {
		admission->global_window_started = now;
		admission->global_attempts = 0;
	}
	if (admission->global_attempts >=
	    ANDROID_NET_UDP_RECONNECT_ADMISSION_GLOBAL_BURST)
		return 0;

	source = admission_find_source(admission, route, now);
	if (admission_window_expired(source->window_started, now,
	                             admission->window_ticks)) {
		source->window_started = now;
		source->attempts = 0;
	}
	source->last_seen = now;
	if (source->attempts >=
	    ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST)
		return 0;

	source->attempts++;
	admission->global_attempts++;
	return verify(context);
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
