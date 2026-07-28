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
	identity.public_key_len = ANDROID_NET_UDP_RECONNECT_PUBLIC_KEY_MAX;
	for (i = 0; i < sizeof(identity.public_key); i++)
		identity.public_key[i] = (uint8_t) (seed + i);
	identity.request_counter = counter;
	identity.request_signature_len = 71;
	memset(identity.request_signature, seed,
	       identity.request_signature_len);
	return identity;
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

static int test_challenge_binds_generation_slot_and_random_value(void)
{
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
	    91, 2, challenge, baseline, sizeof(baseline));
	CHECK(baseline_size > 0);

	changed_size = android_net_udp_reconnect_build_challenge_message(
	    92, 2, challenge, changed, sizeof(changed));
	CHECK(changed_size == baseline_size);
	CHECK(memcmp(baseline, changed, baseline_size) != 0);

	changed_size = android_net_udp_reconnect_build_challenge_message(
	    91, 3, challenge, changed, sizeof(changed));
	CHECK(changed_size == baseline_size);
	CHECK(memcmp(baseline, changed, baseline_size) != 0);

	changed_size = android_net_udp_reconnect_build_challenge_message(
	    91, 2, changed_challenge, changed, sizeof(changed));
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

int main(void)
{
	int passed = 1;

	passed &= test_request_binds_generation_counter_key_and_player_payload();
	passed &= test_request_rejects_invalid_identity_and_short_output();
	passed &= test_public_key_identity_is_independent_of_display_name();
	passed &= test_request_counter_rejects_replay_and_older_values();
	passed &= test_challenge_binds_generation_slot_and_random_value();
	passed &= test_constant_time_comparison_reports_equality();
	if (!passed)
		return 1;
	puts("net UDP reconnect auth tests passed");
	return 0;
}
