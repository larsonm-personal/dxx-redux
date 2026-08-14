#include "net_udp_reconnect_auth.h"

#include <stdio.h>
#include <string.h>

#define CHECK(condition)                                                     \
	do {                                                                     \
		if (!(condition)) {                                                  \
			fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
			        #condition);                                             \
			return 0;                                                        \
		}                                                                    \
	} while (0)

static android_net_udp_reconnect_identity make_identity(uint8_t seed,
                                                        uint64_t counter)
{
	android_net_udp_reconnect_identity identity;
	size_t i;

	memset(&identity, 0, sizeof(identity));
	identity.protocol_version = ANDROID_NET_UDP_RECONNECT_PROTOCOL_VERSION;
	identity.game_kind = ANDROID_NET_UDP_RECONNECT_GAME_D1;
	for (i = 0; i < sizeof(identity.generation_nonce); i++)
		identity.generation_nonce[i] = (uint8_t) (seed + 0x40 + i);
	identity.public_key_len = ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX;
	for (i = 0; i < sizeof(identity.public_key); i++)
		identity.public_key[i] = (uint8_t) (seed + i);
	identity.request_counter = counter;
	identity.request_signature_len = 71;
	memset(identity.request_signature, seed,
	       identity.request_signature_len);
	return identity;
}

typedef struct verify_probe {
	int calls;
	int result;
} verify_probe;

static int count_verification(void *context)
{
	verify_probe *probe = (verify_probe *) context;

	probe->calls++;
	return probe->result;
}

static int test_request_binds_generation_counter_key_and_player_payload(void)
{
	android_net_udp_reconnect_identity identity = make_identity(3, 41);
	android_net_udp_reconnect_identity changed_identity = identity;
	const uint8_t player_payload[] = {
		'a',
		'c',
		'e',
		0,
		0,
		0,
		0,
		0,
		0,
		4,
		2,
		3,
		5,
		0,
	};
	uint8_t changed_payload[sizeof(player_payload)];
	uint8_t baseline[192];
	uint8_t changed[192];
	size_t baseline_size;
	size_t changed_size;

	baseline_size = android_net_udp_reconnect_build_request_message(
	    0x12345678u, &identity, player_payload, sizeof(player_payload),
	    baseline, sizeof(baseline));
	CHECK(baseline_size > 0);

	memcpy(changed_payload, player_payload, sizeof(changed_payload));
	changed_payload[0] = 'b';
	changed_size = android_net_udp_reconnect_build_request_message(
	    0x12345678u, &identity, changed_payload, sizeof(changed_payload),
	    changed, sizeof(changed));
	CHECK(changed_size == baseline_size);
	CHECK(memcmp(baseline, changed, baseline_size) != 0);

	changed_size = android_net_udp_reconnect_build_request_message(
	    0x12345679u, &identity, player_payload, sizeof(player_payload),
	    changed, sizeof(changed));
	CHECK(changed_size == baseline_size);
	CHECK(memcmp(baseline, changed, baseline_size) != 0);

	changed_identity.request_counter++;
	changed_size = android_net_udp_reconnect_build_request_message(
	    0x12345678u, &changed_identity, player_payload,
	    sizeof(player_payload), changed, sizeof(changed));
	CHECK(changed_size == baseline_size);
	CHECK(memcmp(baseline, changed, baseline_size) != 0);

	changed_identity = identity;
	changed_identity.public_key[10] ^= 0x55;
	changed_size = android_net_udp_reconnect_build_request_message(
	    0x12345678u, &changed_identity, player_payload,
	    sizeof(player_payload), changed, sizeof(changed));
	CHECK(changed_size == baseline_size);
	CHECK(memcmp(baseline, changed, baseline_size) != 0);
	return 1;
}

static int test_request_rejects_invalid_identity_and_short_output(void)
{
	android_net_udp_reconnect_identity identity = make_identity(7, 9);
	const uint8_t payload[] = { 1, 2, 3 };
	uint8_t message[192];

	CHECK(android_net_udp_reconnect_build_request_message(
	          5, &identity, payload, sizeof(payload), message,
	          sizeof(message)) > 0);
	CHECK(android_net_udp_reconnect_build_request_message(
	          5, &identity, payload, sizeof(payload), message, 2) == 0);
	identity.public_key_len = 0;
	CHECK(android_net_udp_reconnect_build_request_message(
	          5, &identity, payload, sizeof(payload), message,
	          sizeof(message)) == 0);
	identity = make_identity(7, 9);
	identity.protocol_version--;
	CHECK(android_net_udp_reconnect_build_request_message(
	          5, &identity, payload, sizeof(payload), message,
	          sizeof(message)) == 0);
	return 1;
}

static int test_transcripts_separate_title_role_generation_and_endian(void)
{
	android_net_udp_reconnect_identity d1 = make_identity(4, 7);
	android_net_udp_reconnect_identity d2 = d1;
	android_net_udp_reconnect_identity restarted = d1;
	const uint8_t payload[] = { 0xa1, 0xb2 };
	uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE];
	uint8_t request[192];
	uint8_t other[192];
	uint8_t proof[96];
	size_t request_size;
	size_t other_size;
	size_t proof_size;
	const size_t version_offset = sizeof("DXX-UDP-RECONNECT") - 1;
	const size_t token_offset = version_offset + 3 +
	                            ANDROID_NET_UDP_RECONNECT_GENERATION_SIZE;

	memset(challenge, 0x2c, sizeof(challenge));
	request_size = android_net_udp_reconnect_build_request_message(
	    0x12345678u, &d1, payload, sizeof(payload), request,
	    sizeof(request));
	CHECK(request_size > 0);
	CHECK(memcmp(request, "DXX-UDP-RECONNECT", version_offset) == 0);
	CHECK(request[version_offset] ==
	      ANDROID_NET_UDP_RECONNECT_PROTOCOL_VERSION);
	CHECK(request[version_offset + 1] ==
	      ANDROID_NET_UDP_RECONNECT_GAME_D1);
	CHECK(request[version_offset + 2] == 1);
	CHECK(request[token_offset] == 0x78);
	CHECK(request[token_offset + 1] == 0x56);
	CHECK(request[token_offset + 2] == 0x34);
	CHECK(request[token_offset + 3] == 0x12);

	d2.game_kind = ANDROID_NET_UDP_RECONNECT_GAME_D2;
	other_size = android_net_udp_reconnect_build_request_message(
	    0x12345678u, &d2, payload, sizeof(payload), other,
	    sizeof(other));
	CHECK(other_size == request_size);
	CHECK(memcmp(request, other, request_size) != 0);
	CHECK(!android_net_udp_reconnect_context_equal(&d1, &d2));

	restarted.generation_nonce[3] ^= 0x80;
	other_size = android_net_udp_reconnect_build_request_message(
	    0x12345678u, &restarted, payload, sizeof(payload), other,
	    sizeof(other));
	CHECK(other_size == request_size);
	CHECK(memcmp(request, other, request_size) != 0);
	CHECK(!android_net_udp_reconnect_context_equal(&d1, &restarted));

	proof_size = android_net_udp_reconnect_build_challenge_message(
	    0x12345678u, 2, &d1, challenge, proof, sizeof(proof));
	CHECK(proof_size > 0);
	CHECK(proof[version_offset + 2] == 2);
	CHECK(memcmp(request, proof, version_offset + 2) == 0);
	CHECK(memcmp(request, proof, version_offset + 3) != 0);
	return 1;
}

static int test_public_key_identity_is_independent_of_display_name(void)
{
	android_net_udp_reconnect_identity first = make_identity(11, 100);
	android_net_udp_reconnect_identity same_key = first;
	android_net_udp_reconnect_identity other_key = make_identity(12, 100);
	android_net_udp_reconnect_identity players[3];

	same_key.request_counter = 101;
	same_key.request_signature[0] ^= 0xff;
	CHECK(android_net_udp_reconnect_public_key_equal(&first, &same_key));
	CHECK(!android_net_udp_reconnect_public_key_equal(&first, &other_key));

	memset(players, 0, sizeof(players));
	players[1] = first;
	players[2] = other_key;
	CHECK(android_net_udp_reconnect_find_public_key(
	          players, 3, &same_key) == 1);
	CHECK(android_net_udp_reconnect_find_public_key(
	          players, 3, &other_key) == 2);
	other_key.public_key[3] ^= 0x40;
	CHECK(android_net_udp_reconnect_find_public_key(
	          players, 3, &other_key) == -1);
	return 1;
}

static int test_request_counter_rejects_replay_and_older_values(void)
{
	CHECK(android_net_udp_reconnect_counter_is_newer(11, 10));
	CHECK(!android_net_udp_reconnect_counter_is_newer(10, 10));
	CHECK(!android_net_udp_reconnect_counter_is_newer(9, 10));
	return 1;
}

static int test_migration_identity_preserves_accepted_counter(void)
{
	android_net_udp_reconnect_identity identity =
	    make_identity(19, UINT64_C(0x123456789abcdef0));
	android_net_udp_reconnect_identity restored;
	uint8_t encoded[ANDROID_NET_UDP_RECONNECT_PLAYER_AUTH_SIZE];

	memset(&restored, 0, sizeof(restored));
	CHECK(android_net_udp_reconnect_write_player_identity(
	          encoded, sizeof(encoded), &identity) == sizeof(encoded));
	CHECK(android_net_udp_reconnect_read_player_identity(
	          encoded, sizeof(encoded), &restored) == sizeof(encoded));
	CHECK(restored.public_key_len == identity.public_key_len);
	CHECK(restored.protocol_version == identity.protocol_version);
	CHECK(restored.game_kind == identity.game_kind);
	CHECK(memcmp(restored.generation_nonce, identity.generation_nonce,
	             sizeof(identity.generation_nonce)) == 0);
	CHECK(memcmp(restored.public_key, identity.public_key,
	             sizeof(identity.public_key)) == 0);
	CHECK(restored.request_counter == identity.request_counter);
	CHECK(restored.request_signature_len == 0);
	return 1;
}

static int test_sequence_identity_rejects_downgrade_after_round_trip(void)
{
	android_net_udp_reconnect_identity identity = make_identity(23, 81);
	android_net_udp_reconnect_identity restored;
	uint8_t encoded[ANDROID_NET_UDP_RECONNECT_SEQUENCE_AUTH_SIZE];

	CHECK(android_net_udp_reconnect_write_sequence_identity(
	          encoded, sizeof(encoded), &identity) == sizeof(encoded));
	CHECK(android_net_udp_reconnect_read_sequence_identity(
	          encoded, sizeof(encoded), &restored) == sizeof(encoded));
	CHECK(android_net_udp_reconnect_identity_valid(&restored));
	CHECK(android_net_udp_reconnect_context_equal(&identity, &restored));
	encoded[0] = ANDROID_NET_UDP_RECONNECT_PROTOCOL_VERSION - 1;
	CHECK(android_net_udp_reconnect_read_sequence_identity(
	          encoded, sizeof(encoded), &restored) == sizeof(encoded));
	CHECK(!android_net_udp_reconnect_identity_valid(&restored));
	return 1;
}

static int test_route_proof_direct_proxy_nat_and_replay_matrix(void)
{
	enum {
		TEST_CONTEXT_STARTING = 1,
		TEST_CONTEXT_WAITING = 2,
		TEST_CONTEXT_PLAYING = 3,
	};
	android_net_udp_reconnect_route_proof proof;
	const uint8_t direct_route[] = { 10, 0, 0, 8, 0x12, 0x34 };
	const uint8_t rebound_route[] = { 10, 0, 0, 8, 0x56, 0x78 };
	uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE];
	uint8_t wrong_challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE];
	uint64_t accepted_counter = 40;

	memset(&proof, 0, sizeof(proof));
	memset(challenge, 0x5a, sizeof(challenge));
	memcpy(wrong_challenge, challenge, sizeof(wrong_challenge));
	wrong_challenge[4] ^= 1;

	CHECK(android_net_udp_reconnect_stage_route_proof(
	          &proof, accepted_counter, 41, direct_route,
	          sizeof(direct_route), 0, TEST_CONTEXT_STARTING, 100,
	          challenge));
	CHECK(!android_net_udp_reconnect_route_claim_matches(
	    &proof, rebound_route, sizeof(rebound_route), 0,
	    TEST_CONTEXT_STARTING, 99, challenge));
	CHECK(!android_net_udp_reconnect_route_claim_matches(
	    &proof, direct_route, sizeof(direct_route), 1,
	    TEST_CONTEXT_STARTING, 99, challenge));
	CHECK(!android_net_udp_reconnect_route_claim_matches(
	    &proof, direct_route, sizeof(direct_route), 0,
	    TEST_CONTEXT_WAITING, 99, challenge));
	CHECK(!android_net_udp_reconnect_route_claim_matches(
	    &proof, direct_route, sizeof(direct_route), 0,
	    TEST_CONTEXT_STARTING, 101, challenge));
	CHECK(!android_net_udp_reconnect_route_claim_matches(
	    &proof, direct_route, sizeof(direct_route), 0,
	    TEST_CONTEXT_STARTING, 99, wrong_challenge));
	CHECK(accepted_counter == 40);
	CHECK(android_net_udp_reconnect_route_claim_matches(
	    &proof, direct_route, sizeof(direct_route), 0,
	    TEST_CONTEXT_STARTING, 99, challenge));
	CHECK(android_net_udp_reconnect_commit_route_proof(
	    &proof, &accepted_counter));
	CHECK(accepted_counter == 41);
	CHECK(!android_net_udp_reconnect_route_claim_matches(
	    &proof, direct_route, sizeof(direct_route), 0,
	    TEST_CONTEXT_STARTING, 99, challenge));
	CHECK(!android_net_udp_reconnect_commit_route_proof(
	    &proof, &accepted_counter));

	CHECK(android_net_udp_reconnect_stage_route_proof(
	          &proof, accepted_counter, 42, rebound_route,
	          sizeof(rebound_route), 0, TEST_CONTEXT_WAITING, 200,
	          challenge));
	CHECK(!android_net_udp_reconnect_route_claim_matches(
	    &proof, direct_route, sizeof(direct_route), 0,
	    TEST_CONTEXT_WAITING, 150, challenge));
	CHECK(android_net_udp_reconnect_route_claim_matches(
	    &proof, rebound_route, sizeof(rebound_route), 0,
	    TEST_CONTEXT_WAITING, 150, challenge));
	CHECK(android_net_udp_reconnect_commit_route_proof(
	    &proof, &accepted_counter));

	CHECK(android_net_udp_reconnect_stage_route_proof(
	          &proof, accepted_counter, 43, direct_route,
	          sizeof(direct_route), 1, TEST_CONTEXT_PLAYING, 300,
	          challenge));
	CHECK(!android_net_udp_reconnect_route_claim_matches(
	    &proof, direct_route, sizeof(direct_route), 0,
	    TEST_CONTEXT_PLAYING, 250, challenge));
	CHECK(android_net_udp_reconnect_route_claim_matches(
	    &proof, direct_route, sizeof(direct_route), 1,
	    TEST_CONTEXT_PLAYING, 250, challenge));
	return 1;
}

static int test_stale_request_does_not_replace_live_challenge(void)
{
	android_net_udp_reconnect_route_proof proof;
	const uint8_t route[] = { 192, 0, 2, 4, 0x44, 0x55 };
	uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE];
	uint8_t replacement[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE];

	memset(&proof, 0, sizeof(proof));
	memset(challenge, 0x21, sizeof(challenge));
	memset(replacement, 0x84, sizeof(replacement));
	CHECK(android_net_udp_reconnect_stage_route_proof(
	          &proof, 90, 91, route, sizeof(route), 0, 3, 100,
	          challenge));
	CHECK(!android_net_udp_reconnect_stage_route_proof(
	    &proof, 90, 90, route, sizeof(route), 0, 3, 100,
	    replacement));
	CHECK(android_net_udp_reconnect_route_claim_matches(
	    &proof, route, sizeof(route), 0, 3, 50, challenge));
	return 1;
}

static int test_challenge_binds_generation_slot_and_random_value(void)
{
	android_net_udp_reconnect_identity identity = make_identity(31, 90);
	uint8_t challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE];
	uint8_t changed_challenge[ANDROID_NET_UDP_RECONNECT_CHALLENGE_SIZE];
	uint8_t baseline[96];
	uint8_t changed[96];
	size_t baseline_size;
	size_t changed_size;

	memset(challenge, 0x31, sizeof(challenge));
	memcpy(changed_challenge, challenge, sizeof(changed_challenge));
	changed_challenge[7] ^= 0x7f;
	baseline_size = android_net_udp_reconnect_build_challenge_message(
	    91, 2, &identity, challenge, baseline, sizeof(baseline));
	CHECK(baseline_size > 0);

	changed_size = android_net_udp_reconnect_build_challenge_message(
	    92, 2, &identity, challenge, changed, sizeof(changed));
	CHECK(changed_size == baseline_size);
	CHECK(memcmp(baseline, changed, baseline_size) != 0);

	changed_size = android_net_udp_reconnect_build_challenge_message(
	    91, 3, &identity, challenge, changed, sizeof(changed));
	CHECK(changed_size == baseline_size);
	CHECK(memcmp(baseline, changed, baseline_size) != 0);

	changed_size = android_net_udp_reconnect_build_challenge_message(
	    91, 2, &identity, changed_challenge, changed, sizeof(changed));
	CHECK(changed_size == baseline_size);
	CHECK(memcmp(baseline, changed, baseline_size) != 0);
	identity.generation_nonce[0] ^= 1;
	changed_size = android_net_udp_reconnect_build_challenge_message(
	    91, 2, &identity, challenge, changed, sizeof(changed));
	CHECK(changed_size == baseline_size);
	CHECK(memcmp(baseline, changed, baseline_size) != 0);
	return 1;
}

static int test_constant_time_comparison_reports_equality(void)
{
	const uint8_t first[] = { 1, 2, 3, 4 };
	const uint8_t same[] = { 1, 2, 3, 4 };
	const uint8_t changed[] = { 1, 2, 8, 4 };

	CHECK(android_net_udp_reconnect_bytes_equal(
	    first, same, sizeof(first)));
	CHECK(!android_net_udp_reconnect_bytes_equal(
	    first, changed, sizeof(first)));
	return 1;
}

static int test_verification_admission_bounds_source_before_crypto(void)
{
	android_net_udp_reconnect_admission admission;
	const uint8_t route[] = { 192, 0, 2, 1, 0x12, 0x34 };
	verify_probe probe = { 0, 1 };
	unsigned int i;

	android_net_udp_reconnect_admission_reset(&admission, 100);
	for (i = 0;
	     i < ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST; i++)
		CHECK(android_net_udp_reconnect_verify_admitted(
		    &admission, route, sizeof(route), 10,
		    count_verification, &probe));
	CHECK(probe.calls ==
	      ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST);
	CHECK(!android_net_udp_reconnect_verify_admitted(
	    &admission, route, sizeof(route), 10,
	    count_verification, &probe));
	CHECK(probe.calls ==
	      ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST);
	return 1;
}

static int test_verification_admission_ignores_source_port(void)
{
	android_net_udp_reconnect_admission admission;
	uint8_t route[] = { 198, 51, 100, 4, 0, 0 };
	verify_probe probe = { 0, 1 };
	unsigned int i;

	android_net_udp_reconnect_admission_reset(&admission, 100);
	for (i = 0;
	     i < ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST; i++) {
		route[4] = (uint8_t) i;
		route[5] = (uint8_t) (i + 20);
		CHECK(android_net_udp_reconnect_verify_admitted(
		    &admission, route, sizeof(route), 20,
		    count_verification, &probe));
	}
	route[4] = 0xfe;
	route[5] = 0xdc;
	CHECK(!android_net_udp_reconnect_verify_admitted(
	    &admission, route, sizeof(route), 20,
	    count_verification, &probe));
	CHECK(probe.calls ==
	      ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST);
	return 1;
}

static int test_verification_admission_preserves_source_fairness(void)
{
	android_net_udp_reconnect_admission admission;
	const uint8_t noisy[] = { 203, 0, 113, 1, 1, 1 };
	const uint8_t peer[] = { 203, 0, 113, 2, 2, 2 };
	verify_probe probe = { 0, 1 };
	unsigned int i;

	android_net_udp_reconnect_admission_reset(&admission, 100);
	for (i = 0;
	     i < ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST; i++)
		CHECK(android_net_udp_reconnect_verify_admitted(
		    &admission, noisy, sizeof(noisy), 30,
		    count_verification, &probe));
	CHECK(!android_net_udp_reconnect_verify_admitted(
	    &admission, noisy, sizeof(noisy), 30,
	    count_verification, &probe));
	CHECK(android_net_udp_reconnect_verify_admitted(
	    &admission, peer, sizeof(peer), 30,
	    count_verification, &probe));
	CHECK(probe.calls ==
	      ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST + 1);
	return 1;
}

static int test_verification_admission_bounds_source_rotation(void)
{
	android_net_udp_reconnect_admission admission;
	uint8_t route[] = { 10, 0, 0, 0, 0, 0 };
	verify_probe probe = { 0, 1 };
	unsigned int i;

	android_net_udp_reconnect_admission_reset(&admission, 100);
	for (i = 0;
	     i < ANDROID_NET_UDP_RECONNECT_ADMISSION_GLOBAL_BURST; i++) {
		route[3] = (uint8_t) (i + 1);
		CHECK(android_net_udp_reconnect_verify_admitted(
		    &admission, route, sizeof(route), 40,
		    count_verification, &probe));
	}
	route[3]++;
	CHECK(!android_net_udp_reconnect_verify_admitted(
	    &admission, route, sizeof(route), 40,
	    count_verification, &probe));
	CHECK(probe.calls ==
	      ANDROID_NET_UDP_RECONNECT_ADMISSION_GLOBAL_BURST);
	return 1;
}

static int test_verification_admission_expires_and_resets(void)
{
	android_net_udp_reconnect_admission admission;
	const uint8_t route[] = { 172, 16, 0, 8, 3, 4 };
	verify_probe probe = { 0, 1 };
	unsigned int i;

	android_net_udp_reconnect_admission_reset(&admission, 100);
	for (i = 0;
	     i < ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST; i++)
		CHECK(android_net_udp_reconnect_verify_admitted(
		    &admission, route, sizeof(route), 50,
		    count_verification, &probe));
	CHECK(android_net_udp_reconnect_verify_admitted(
	    &admission, route, sizeof(route), 150,
	    count_verification, &probe));
	android_net_udp_reconnect_admission_reset(&admission, 100);
	CHECK(android_net_udp_reconnect_verify_admitted(
	    &admission, route, sizeof(route), 150,
	    count_verification, &probe));
	CHECK(probe.calls ==
	      ANDROID_NET_UDP_RECONNECT_ADMISSION_SOURCE_BURST + 2);
	return 1;
}

int main(void)
{
	int passed = 1;

	passed &= test_request_binds_generation_counter_key_and_player_payload();
	passed &= test_request_rejects_invalid_identity_and_short_output();
	passed &= test_transcripts_separate_title_role_generation_and_endian();
	passed &= test_public_key_identity_is_independent_of_display_name();
	passed &= test_request_counter_rejects_replay_and_older_values();
	passed &= test_migration_identity_preserves_accepted_counter();
	passed &= test_sequence_identity_rejects_downgrade_after_round_trip();
	passed &= test_route_proof_direct_proxy_nat_and_replay_matrix();
	passed &= test_stale_request_does_not_replace_live_challenge();
	passed &= test_challenge_binds_generation_slot_and_random_value();
	passed &= test_constant_time_comparison_reports_equality();
	passed &= test_verification_admission_bounds_source_before_crypto();
	passed &= test_verification_admission_ignores_source_port();
	passed &= test_verification_admission_preserves_source_fairness();
	passed &= test_verification_admission_bounds_source_rotation();
	passed &= test_verification_admission_expires_and_resets();
	if (!passed)
		return 1;
	puts("net UDP reconnect auth tests passed");
	return 0;
}
