#include "multi_save_transfer_policy.h"

#include <stdio.h>

#define CHECK(condition)                                                     \
	do {                                                                     \
		if (!(condition)) {                                                  \
			fprintf(stderr, "%s:%d: check failed: %s\n", __FILE__, __LINE__, \
			        #condition);                                             \
			return 1;                                                        \
		}                                                                    \
	} while (0)

int main(void)
{
	multi_save_transfer_budget budget = { 0 };
	unsigned sent = 0;
	/* A stalled receiver cannot make the sender fill the reliable queue.
	 * ACK progress reopens the window, including per-recipient rollback copies */
	CHECK(multi_save_transfer_window_chunks(128, 1, 1512) == 0);
	CHECK(multi_save_transfer_window_chunks(127, 7, 1512) == 0);
	CHECK(multi_save_transfer_window_chunks(0, 7, 1512) == 18);
	CHECK(multi_save_transfer_window_chunks(0, 0, 1512) == 0);
	{
		unsigned pending = 0, delivered = 0, peak = 0;
		for (uint64_t now = 0; delivered < 1512 && now < 120000; now += 100) {
			if (now && now % 1000 == 0) {
				unsigned acked = pending < 20 ? pending : 20;
				pending -= acked;
				delivered += acked;
			}
			unsigned allowed = multi_save_transfer_window_chunks(pending, 1, 1512 - sent);
			unsigned chunks = multi_save_transfer_chunk_budget(&budget, now, allowed);
			sent += chunks;
			pending += chunks;
			if (pending > peak) peak = pending;
			CHECK(pending <= MULTI_SAVE_TRANSFER_RELIABLE_WINDOW);
		}
		CHECK(delivered == 1512 && sent == 1512 && peak == MULTI_SAVE_TRANSFER_RELIABLE_WINDOW);
	}
	budget = (multi_save_transfer_budget) { 0 };
	sent = 0;
	/* The observed 864-chunk restore must finish even at two rendered fps */
	for (unsigned frame = 0; frame <= 18 && sent < 864; ++frame) {
		unsigned chunks = multi_save_transfer_chunk_budget(&budget, frame * 500, 864 - sent);
		CHECK(chunks <= MULTI_SAVE_TRANSFER_CHUNKS_BURST);
		sent += chunks;
	}
	CHECK(sent == 864);
	budget = (multi_save_transfer_budget) { 0 };
	sent = 0;
	for (unsigned ms = 0; ms <= 1000; ++ms)
		sent += multi_save_transfer_chunk_budget(&budget, ms, 10000);
	CHECK(sent == 8 + MULTI_SAVE_TRANSFER_CHUNKS_SECOND);
	CHECK(multi_save_transfer_chunk_budget(&budget, 1000, 10000) == 0);
	CHECK(multi_save_transfer_chunk_budget(&budget, 999, 10000) == 0);
	CHECK(multi_save_transfer_chunk_budget(&budget, 300000, 10000) == MULTI_SAVE_TRANSFER_CHUNKS_BURST);
	CHECK(multi_save_transfer_chunk_budget(&budget, 300000, 10000) == 0);
	CHECK(multi_save_transfer_chunk_budget(&budget, 300500, 3) == 3);
	CHECK(multi_save_transfer_chunk_budget(&budget, 300501, 0) == 0);

	/* Exercise the maximum campaign envelope at two fps. The old fixed
	 * 60-second limit expired partway through this otherwise progressing send */
	{
		const unsigned payload = 432; /* Both games' MULTI_REWIND_SAVE_CHUNK_PAYLOAD */
		const unsigned total = (MULTI_SAVE_TRANSFER_MAX_BYTES + payload - 1) / payload;
		uint64_t now = 0, progress = 0;
		CHECK(MULTI_SAVE_TRANSFER_MAX_BYTES >= COOP_CAMPAIGN_MAX_BYTES + COOP_CAMPAIGN_MAX_WORLD_BYTES + 1448u);
		CHECK(total > 32767 && total <= UINT16_MAX);
		budget = (multi_save_transfer_budget) { 0 };
		sent = 0;
		while (sent < total) {
			unsigned chunks;
			CHECK(!multi_save_transfer_expired(now, 0, progress, total));
			chunks = multi_save_transfer_chunk_budget(&budget, now, total - sent);
			if (chunks) progress = now;
			sent += chunks;
			now += 500;
		}
		CHECK(now > 60000 && now < multi_save_transfer_limit_ms(total));
		CHECK(!multi_save_transfer_expired(progress + 59999, 0, progress, total));
		CHECK(multi_save_transfer_expired(progress + 60000, 0, progress, total));
		/* Endless trickling cannot extend the absolute deadline */
		now = multi_save_transfer_limit_ms(total);
		CHECK(multi_save_transfer_expired(now, 0, now, total));
		CHECK(multi_save_transfer_expired(60000, 0, 0, total));
		CHECK(!multi_save_transfer_expired(999, 1000, 1000, total));
	}

	CHECK(multi_save_transfer_host_action_for_rewind(0) ==
	      MULTI_SAVE_TRANSFER_HOST_APPLY_NOW);
	CHECK(multi_save_transfer_host_action_for_rewind(1) ==
	      MULTI_SAVE_TRANSFER_HOST_WAIT_FOR_CLIENTS);

	CHECK(multi_save_transfer_client_apply_action_for_context(
	          0, 1, 1, 964, 964) == MULTI_SAVE_TRANSFER_CLIENT_WAIT);
	CHECK(multi_save_transfer_client_apply_action_for_context(
	          1, 1, 1, 963, 964) == MULTI_SAVE_TRANSFER_CLIENT_WAIT);
	CHECK(multi_save_transfer_client_apply_action_for_context(
	          1, 1, 0, 964, 964) == MULTI_SAVE_TRANSFER_CLIENT_WAIT);
	CHECK(multi_save_transfer_client_apply_action_for_context(
	          1, 1, 1, 964, 964) == MULTI_SAVE_TRANSFER_CLIENT_APPLY);
	puts("multi save transfer policy tests passed");
	return 0;
}
