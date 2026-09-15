/* Exercise delayed/relayed gameplay using both engines' actual message IDs */
#include "wall.h"
#include "coop_gameplay_fence.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

#define COMMAND_LENGTH(name, size) size,
static const int lengths[] = { for_each_multiplayer_command(, COMMAND_LENGTH, ) };

static void test_delayed_world_mutations(void)
{
	const unsigned mutations[] = { MULTI_SCORE, MULTI_SHIP_STATUS, MULTI_DAMAGE, MULTI_REPAIR, MULTI_COOP_PEER_STATUS,
		MULTI_ROBOT_POSITION, MULTI_REMOVE_OBJECT, MULTI_DOOR_OPEN, MULTI_TRIGGER,
		MULTI_PLAYER_EXPLODE, MULTI_SAVE_GAME, MULTI_RESTORE_GAME, MULTI_REWIND_REQUEST };
	coop_gameplay_stamp base = { 17, 8, 0 }, secret = { 18, -2, 0 }, returned = { 19, 8, 0 };
	coop_gameplay_stamp rollback = { 20, 8, 0 }, temporary = { 20, 8, 1 };
	for (size_t i = 0; i < sizeof(mutations) / sizeof(*mutations); ++i) {
		unsigned type = mutations[i];
		CHECK(coop_gameplay_message_allowed(type, &base, &base, 0));
		CHECK(!coop_gameplay_message_allowed(type, &base, &base, 1));
		CHECK(!coop_gameplay_message_allowed(type, &base, &secret, 0));
		CHECK(!coop_gameplay_message_allowed(type, &base, &returned, 0));
		CHECK(!coop_gameplay_message_allowed(type, &returned, &rollback, 0));
		CHECK(!coop_gameplay_message_allowed(type, &temporary, &rollback, 0));
		CHECK(coop_gameplay_message_allowed(type, &rollback, &rollback, 0));
	}
	coop_gameplay_stamp neutral = { 0, 0, 0 }, unassigned = { 0, 8, 0 };
	CHECK(!coop_gameplay_message_allowed(MULTI_SCORE, &neutral, &neutral, 0));
	CHECK(!coop_gameplay_message_allowed(MULTI_SCORE, &unassigned, &unassigned, 0));
	CHECK(coop_gameplay_message_allowed(MULTI_COOP_TRAVEL, &neutral, &base, 1));
	uint64_t next = 7;
	CHECK(coop_gameplay_next_visit(UINT64_C(0xffffffff), &next) && next == UINT64_C(0x100000000));
	CHECK(!coop_gameplay_next_visit(UINT64_MAX, &next) && next == UINT64_C(0x100000000));
}

static void test_mixed_retry_keeps_control_and_original_visit(void)
{
	/* A reliable datagram combines a score and a load ACK. The host relays
	 * the same stored body/stamp after returning to the same numeric mine */
	unsigned char queued[64] = { MULTI_SCORE, 1, 0, 0, 0, 0,
		MULTI_REWIND_SAVE_READY, 9, 1, 2, 3, 0, 0, 0, 1, 0, 0, 0 };
	size_t body_size = (size_t) lengths[MULTI_SCORE] + lengths[MULTI_REWIND_SAVE_READY];
	CHECK(body_size == 18);
	coop_gameplay_stamp source = { UINT64_C(0x100000003), -2, 0 }, received;
	CHECK(coop_gameplay_stamp_write(queued + body_size, sizeof(queued) - body_size, &source));
	unsigned char retry[64];
	memcpy(retry, queued, sizeof(queued));
	coop_gameplay_stamp destination = { UINT64_C(0x100000005), -2, 0 };
	CHECK(coop_gameplay_stamp_read(&received, retry + body_size, COOP_GAMEPLAY_STAMP_BYTES));
	CHECK(received.visit == source.visit && received.level == source.level);
	unsigned applied = 0, skipped = 0;
	for (size_t offset = 0; offset < body_size;) {
		unsigned type = retry[offset];
		CHECK(type < sizeof(lengths) / sizeof(*lengths));
		CHECK(lengths[type] > 0 && (size_t) lengths[type] <= body_size - offset);
		if (coop_gameplay_message_allowed(type, &received, &destination, 1)) {
			CHECK(type == MULTI_REWIND_SAVE_READY);
			++applied;
		} else { CHECK(type == MULTI_SCORE); ++skipped; }
		offset += lengths[type];
	}
	CHECK(applied == 1 && skipped == 1 && !memcmp(queued, retry, sizeof(queued)));
	CHECK(coop_gameplay_message_allowed(MULTI_COOP_TRAVEL, &source, &destination, 1));
	CHECK(!coop_gameplay_message_allowed(MULTI_COOP_RECOVERY, &source, &destination, 1));
	CHECK(!coop_gameplay_message_allowed(MULTI_COOP_POWERUP_COLLECTED, &source, &destination, 0));
}

static void test_recovery_stays_in_its_visit(void)
{
	coop_gameplay_stamp current = { 20, 8, 0 }, frozen = { 20, 8, 1 };
	coop_gameplay_stamp old = { 19, 8, 0 }, old_frozen = { 19, 8, 1 };
	coop_gameplay_stamp other_level = { 20, -2, 0 }, unassigned = { 0, 8, 0 };
	/* Freeze settlement is valid on either side of this visit's release */
	CHECK(coop_gameplay_message_allowed(MULTI_COOP_RECOVERY, &current, &frozen, 1));
	CHECK(coop_gameplay_message_allowed(MULTI_COOP_RECOVERY, &frozen, &frozen, 1));
	CHECK(coop_gameplay_message_allowed(MULTI_COOP_RECOVERY, &frozen, &current, 0));
	CHECK(!coop_gameplay_message_allowed(MULTI_COOP_RECOVERY, &old, &current, 0));
	CHECK(!coop_gameplay_message_allowed(MULTI_COOP_RECOVERY, &old_frozen, &frozen, 1));
	CHECK(!coop_gameplay_message_allowed(MULTI_COOP_RECOVERY, &other_level, &current, 0));
	CHECK(!coop_gameplay_message_allowed(MULTI_COOP_RECOVERY, &unassigned, &unassigned, 0));
}

static void test_exit_status_keeps_normal_waiting_live(void)
{
	coop_gameplay_stamp normal_wait = { 52, 8, 0 }, secret_warning = { 52, 8, 1 };
	coop_gameplay_stamp returned = { 54, 8, 0 }, next_mine = { 53, 9, 0 };
	/* End-level status uses the exit mutation policy, not session control */
	CHECK(!coop_gameplay_session_control(MULTI_ENDLEVEL_START));
	CHECK(coop_gameplay_message_allowed(MULTI_ENDLEVEL_START, &normal_wait, &normal_wait, 0));
	CHECK(!coop_gameplay_message_allowed(MULTI_ENDLEVEL_START, &normal_wait, &secret_warning, 1));
	CHECK(!coop_gameplay_message_allowed(MULTI_ENDLEVEL_START, &secret_warning, &normal_wait, 0));
	CHECK(!coop_gameplay_message_allowed(MULTI_ENDLEVEL_START, &normal_wait, &returned, 0));
	CHECK(!coop_gameplay_message_allowed(MULTI_ENDLEVEL_START, &normal_wait, &next_mine, 0));
	CHECK(coop_gameplay_message_allowed(MULTI_ENDLEVEL_START, &next_mine, &next_mine, 0));
}

static void test_terminal_scores_keep_departed_world_closed(void)
{
	coop_gameplay_stamp secret = { 52, -2, 0 }, frozen_secret = { 52, -2, 1 };
	coop_gameplay_stamp terminal = { 53, -2, 1 }, live_terminal = { 53, -2, 0 };
	coop_gameplay_stamp other_level = { 53, 8, 1 }, unassigned = { 0, -2, 1 };
	CHECK(coop_gameplay_endlevel_stamp_allowed(&secret, &secret, 0));
	CHECK(!coop_gameplay_endlevel_stamp_allowed(&secret, &frozen_secret, 0));
	CHECK(!coop_gameplay_endlevel_stamp_allowed(&secret, &terminal, 1));
	CHECK(!coop_gameplay_endlevel_stamp_allowed(&frozen_secret, &terminal, 1));
	CHECK(!coop_gameplay_endlevel_stamp_allowed(&other_level, &terminal, 1));
	CHECK(!coop_gameplay_endlevel_stamp_allowed(&live_terminal, &terminal, 1));
	CHECK(!coop_gameplay_endlevel_stamp_allowed(&terminal, &live_terminal, 1));
	CHECK(!coop_gameplay_endlevel_stamp_allowed(&terminal, &terminal, 0));
	CHECK(!coop_gameplay_endlevel_stamp_allowed(&unassigned, &unassigned, 1));
	CHECK(!coop_gameplay_endlevel_stamp_allowed(NULL, &terminal, 1));
	CHECK(coop_gameplay_endlevel_stamp_allowed(&terminal, &terminal, 1));
	/* This exception does not reopen gameplay or state-changing requests */
	CHECK(!coop_gameplay_message_allowed(MULTI_SCORE, &terminal, &terminal, 1));
	CHECK(!coop_gameplay_message_allowed(MULTI_SAVE_GAME, &terminal, &terminal, 1));
	CHECK(!coop_gameplay_message_allowed(MULTI_REWIND_REQUEST, &terminal, &terminal, 1));
}

static void test_pending_inventory_keeps_received_visit(void)
{
	coop_gameplay_stamp received = { 41, -2, 1 }, before_sync = { 0, 0, 0 };
	coop_gameplay_stamp loading = { 41, -2, 1 }, ready = { 41, -2, 0 };
	coop_gameplay_stamp rollback = { 42, -2, 0 }, other_level = { 41, 8, 0 };
	CHECK(coop_gameplay_inventory_action(&received, &before_sync, 0) == COOP_INVENTORY_WAIT);
	CHECK(coop_gameplay_inventory_action(&received, &loading, 1) == COOP_INVENTORY_WAIT);
	CHECK(coop_gameplay_inventory_action(&received, &ready, 0) == COOP_INVENTORY_WAIT);
	CHECK(coop_gameplay_inventory_action(&received, &ready, 1) == COOP_INVENTORY_APPLY);
	/* A cached packet cannot follow the same numeric level into a newer visit */
	CHECK(coop_gameplay_inventory_action(&received, &rollback, 0) == COOP_INVENTORY_DISCARD);
	CHECK(coop_gameplay_inventory_action(&received, &rollback, 1) == COOP_INVENTORY_DISCARD);
	CHECK(coop_gameplay_inventory_action(&received, &other_level, 1) == COOP_INVENTORY_DISCARD);
	CHECK(coop_gameplay_inventory_action(&rollback, &ready, 1) == COOP_INVENTORY_DISCARD);
	CHECK(coop_gameplay_inventory_action(&before_sync, &ready, 1) == COOP_INVENTORY_DISCARD);
	CHECK(coop_gameplay_inventory_action(NULL, &ready, 1) == COOP_INVENTORY_DISCARD);
	CHECK(received.visit == 41 && received.level == -2 && received.frozen);
}

static void test_malformed_stamp_is_atomic(void)
{
	unsigned char bytes[COOP_GAMEPLAY_STAMP_BYTES], bad[COOP_GAMEPLAY_STAMP_BYTES];
	coop_gameplay_stamp source = { UINT64_C(0x8877665544332211), -127, 1 }, decoded = { 42, 8, 0 };
	CHECK(coop_gameplay_stamp_write(bytes, sizeof(bytes), &source));
	CHECK(bytes[0] == 0xd7 && bytes[1] == 0x11 && bytes[2] == 0x81 && bytes[3] == 0xff);
	CHECK(bytes[4] == 0x11 && bytes[11] == 0x88);
	for (size_t size = 0; size < sizeof(bytes); ++size) {
		CHECK(!coop_gameplay_stamp_read(&decoded, bytes, size));
		CHECK(decoded.visit == 42 && decoded.level == 8 && !decoded.frozen);
	}
	for (unsigned index = 0; index < 4; ++index) {
		memcpy(bad, bytes, sizeof(bad));
		if (index == 0) bad[0] ^= 1;
		if (index == 1) bad[1] |= 2;
		if (index == 2) bad[2] = 0x80; /* -128 is not an authored level */
		if (index == 3) bad[2] = bad[3] = 0; /* Neutral stamp cannot carry a visit */
		CHECK(!coop_gameplay_stamp_read(&decoded, bad, sizeof(bad)));
		CHECK(decoded.visit == 42 && decoded.level == 8 && !decoded.frozen);
	}
	CHECK(coop_gameplay_stamp_read(&decoded, bytes, sizeof(bytes)));
	CHECK(decoded.visit == source.visit && decoded.level == source.level && decoded.frozen == 1);
	memset(bad, 0xa5, sizeof(bad));
	source.level = 128;
	CHECK(!coop_gameplay_stamp_write(bad, sizeof(bad), &source));
	for (size_t i = 0; i < sizeof(bad); ++i) CHECK(bad[i] == 0xa5);
}

int main(void)
{
	test_delayed_world_mutations();
	test_terminal_scores_keep_departed_world_closed();
	test_mixed_retry_keeps_control_and_original_visit();
	test_recovery_stays_in_its_visit();
	test_exit_status_keeps_normal_waiting_live();
	test_pending_inventory_keeps_received_visit();
	test_malformed_stamp_is_atomic();
	return 0;
}
