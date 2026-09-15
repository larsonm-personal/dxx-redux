#ifdef NDEBUG
#undef NDEBUG
#endif
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "coop_campaign.h"

static coop_campaign normal_campaign(int level)
{
	coop_campaign campaign = { 0 };
	memcpy(campaign.mission, "d2", 3);
	campaign.active_level = (int16_t) level;
	campaign.generation = UINT64_C(0x100000007);
	return campaign;
}

static void roundtrip(const coop_campaign *source, coop_campaign *restored)
{
	unsigned char *first = NULL, *second = NULL;
	size_t first_size = 0, second_size = 0;
	assert(coop_campaign_encode(source, &first, &first_size));
	assert(coop_campaign_decode(restored, first, first_size));
	assert(coop_campaign_encode(restored, &second, &second_size));
	assert(first_size == second_size && !memcmp(first, second, first_size));
	free(first);
	free(second);
}

static void campaign_visits(void)
{
	const char base[] = "base: reactor intact, door opened, robot dead";
	const char secret[] = "secret: key collected, missile pickup consumed";
	coop_campaign campaign = normal_campaign(10), restored = { 0 };
	const coop_campaign_world *world;
	roundtrip(&campaign, &restored);
	assert(restored.active_level == 10 && restored.generation == campaign.generation);
	/* Enter from an intact base; active world is supplied by the outer save */
	assert(coop_campaign_put_world(&campaign, 10, COOP_CAMPAIGN_AVAILABLE, base, sizeof(base)));
	campaign.active_level = -2;
	campaign.entered_from = 10;
	campaign.base_returnable = 1;
	++campaign.generation;
	roundtrip(&campaign, &restored);
	world = coop_campaign_find_world(&restored, 10);
	assert(world && world->size == sizeof(base) && !memcmp(world->data, base, sizeof(base)));
	/* Return, then reach the same secret from a later base */
	assert(coop_campaign_put_world(&campaign, -2, COOP_CAMPAIGN_AVAILABLE, secret, sizeof(secret)));
	coop_campaign_remove_world(&campaign, 10);
	campaign.active_level = 11;
	campaign.entered_from = 0;
	campaign.base_returnable = 0;
	++campaign.generation;
	roundtrip(&campaign, &restored);
	world = coop_campaign_find_world(&restored, -2);
	assert(world && !memcmp(world->data, secret, sizeof(secret)));
	/* Destroyed secret remains explicitly unavailable with no stale snapshot */
	assert(coop_campaign_put_world(&campaign, -2, COOP_CAMPAIGN_DESTROYED, NULL, 0));
	roundtrip(&campaign, &restored);
	assert(coop_campaign_find_world(&restored, -2)->state == COOP_CAMPAIGN_DESTROYED);
	/* A destroyed base needs no snapshot and cannot be made returnable */
	campaign.active_level = -3;
	campaign.entered_from = 11;
	roundtrip(&campaign, &restored);
	campaign.base_returnable = 1;
	assert(!coop_campaign_valid(&campaign));
	coop_campaign_clear(&campaign);
	coop_campaign_clear(&restored);
}

static void malformed_archives(void)
{
	const unsigned char world[] = { 1, 2, 3, 4, 5 };
	coop_campaign source = normal_campaign(3), destination = normal_campaign(9);
	unsigned char *bytes = NULL;
	size_t size = 0, i;
	assert(coop_campaign_put_world(&source, -1, COOP_CAMPAIGN_AVAILABLE, world, sizeof(world)));
	assert(coop_campaign_encode(&source, &bytes, &size));
	/* No truncated input may replace a live campaign */
	for (i = 0; i < size; ++i) {
		assert(!coop_campaign_decode(&destination, bytes, i));
		assert(destination.active_level == 9 && !destination.world_count);
	}
	bytes[size - 1] ^= 1;
	assert(!coop_campaign_decode(&destination, bytes, size));
	bytes[size - 1] ^= 1;
	bytes[44] = 0xff;
	assert(!coop_campaign_decode(&destination, bytes, size));
	bytes[44] = sizeof(world);
	bytes[8] = 129;
	assert(!coop_campaign_decode(&destination, bytes, size));
	bytes[8] = 1;
	bytes[12] = 0;
	assert(!coop_campaign_decode(&destination, bytes, size));
	bytes[12] = 3;
	memset(bytes + 24, 'x', 9);
	assert(!coop_campaign_decode(&destination, bytes, size));
	assert(destination.active_level == 9 && !destination.world_count);
	free(bytes);
	coop_campaign_clear(&source);
	coop_campaign_clear(&destination);
}

static void travel_roundtrip(coop_campaign_travel *travel)
{
	unsigned char *first = NULL, *second = NULL;
	size_t size = 0, second_size = 0;
	uint64_t generation = travel->source_generation;
	int level = travel->destination.level;
	assert(coop_campaign_travel_encode(travel, &first, &size));
	for (size_t i = 0; i < size; ++i) {
		assert(!coop_campaign_travel_decode(travel, first, i));
		assert(travel->source_generation == generation && travel->destination.level == level);
	}
	first[size - 1] ^= 1;
	assert(!coop_campaign_travel_decode(travel, first, size));
	first[size - 1] ^= 1;
	assert(coop_campaign_travel_decode(travel, first, size));
	assert(coop_campaign_travel_encode(travel, &second, &second_size));
	assert(size == second_size && !memcmp(first, second, size));
	free(first);
	free(second);
}

static void prepared_travel(void)
{
	const char base[] = "base before departure";
	const char secret[] = "secret after pickup";
	coop_campaign live = normal_campaign(10), restored = { 0 };
	coop_campaign_travel travel = { 0 };
	const coop_campaign_world *world;
	uint64_t generation = live.generation;
	/* Preparing and cancelling an entry must retain the playable source */
	assert(coop_campaign_prepare_enter(&live, -2, 0, base, sizeof(base), &travel));
	assert(live.active_level == 10 && live.generation == generation && !live.world_count);
	assert(travel.action == COOP_CAMPAIGN_ENTER && travel.destination.level == -2);
	assert(!travel.destination.data && travel.next.base_returnable);
	travel_roundtrip(&travel);
	coop_campaign_travel_clear(&travel);
	assert(coop_campaign_valid(&live) && !live.world_count);
	assert(coop_campaign_prepare_enter(&live, -2, 0, base, sizeof(base), &travel));
	/* Invalid replacement requests preserve an already prepared operation */
	assert(!coop_campaign_prepare_enter(&live, 2, 0, base, sizeof(base), &travel));
	assert(travel.action == COOP_CAMPAIGN_ENTER && travel.destination.level == -2);
	assert(coop_campaign_commit_travel(&live, &travel));
	assert(!coop_campaign_commit_travel(&live, &travel));
	assert(live.active_level == -2 && live.entered_from == 10 && live.base_returnable);
	assert(live.generation == generation + 1);
	roundtrip(&live, &restored);
	/* Return restores the exact dormant base and stores the visited secret */
	assert(coop_campaign_prepare_return(&live, 24, 0, secret, sizeof(secret), &travel));
	assert(travel.action == COOP_CAMPAIGN_RETURN && travel.destination.level == 10);
	assert(travel.destination.size == sizeof(base) && !memcmp(travel.destination.data, base, sizeof(base)));
	travel_roundtrip(&travel);
	assert(live.active_level == -2 && coop_campaign_find_world(&live, 10));
	assert(coop_campaign_commit_travel(&live, &travel));
	assert(live.active_level == 10 && !live.entered_from && !live.base_returnable);
	world = coop_campaign_find_world(&live, -2);
	assert(world && world->size == sizeof(secret) && !memcmp(world->data, secret, sizeof(secret)));
	/* A revisit owns its destination independently of the live archive */
	assert(coop_campaign_prepare_enter(&live, -2, 0, base, sizeof(base), &travel));
	assert(travel.destination.data != world->data);
	assert(!memcmp(travel.destination.data, secret, sizeof(secret)));
	travel_roundtrip(&travel);
	++live.generation;
	assert(!coop_campaign_commit_travel(&live, &travel));
	assert(live.active_level == 10 && world == coop_campaign_find_world(&live, -2));
	coop_campaign_travel_clear(&travel);
	assert(coop_campaign_prepare_enter(&live, -2, 0, base, sizeof(base), &travel));
	assert(coop_campaign_commit_travel(&live, &travel));
	assert(coop_campaign_prepare_return(&live, 24, 1, NULL, 0, &travel));
	travel_roundtrip(&travel);
	assert(coop_campaign_commit_travel(&live, &travel));
	assert(coop_campaign_find_world(&live, -2)->state == COOP_CAMPAIGN_DESTROYED);
	assert(!coop_campaign_prepare_enter(&live, -2, 0, base, sizeof(base), &travel));
	roundtrip(&live, &restored);
	coop_campaign_clear(&live);
	coop_campaign_clear(&restored);
	/* Entering from a destroyed base cannot create a return path */
	live = normal_campaign(10);
	assert(!coop_campaign_prepare_enter(&live, -2, 1, base, sizeof(base), &travel));
	assert(coop_campaign_prepare_enter(&live, -2, 1, NULL, 0, &travel));
	assert(coop_campaign_commit_travel(&live, &travel));
	assert(!live.base_returnable && !live.world_count);
	assert(coop_campaign_prepare_return(&live, 24, 0, secret, sizeof(secret), &travel));
	assert(travel.action == COOP_CAMPAIGN_ADVANCE && travel.destination.level == 11);
	assert(!travel.destination.data);
	travel_roundtrip(&travel);
	assert(coop_campaign_commit_travel(&live, &travel));
	assert(live.active_level == 11 && coop_campaign_find_world(&live, -2));
	coop_campaign_clear(&live);
	/* The last normal level routes to endgame, never a nonexistent level */
	live = normal_campaign(24);
	assert(coop_campaign_prepare_enter(&live, -6, 1, NULL, 0, &travel));
	assert(coop_campaign_commit_travel(&live, &travel));
	assert(coop_campaign_prepare_return(&live, 24, 1, NULL, 0, &travel));
	assert(travel.action == COOP_CAMPAIGN_ENDGAME && !travel.destination.level);
	travel_roundtrip(&travel);
	assert(live.active_level == -6);
	assert(coop_campaign_commit_travel(&live, &travel));
	assert(!live.active_level && !live.world_count);
	live = normal_campaign(10);
	live.generation = UINT64_MAX;
	assert(!coop_campaign_prepare_enter(&live, -2, 0, base, sizeof(base), &travel));
	coop_campaign_clear(&live);
}

static void canonical_and_bounded(void)
{
	coop_campaign first = normal_campaign(4), second = normal_campaign(4), restored = { 0 };
	unsigned char *a = NULL, *b = NULL, *large;
	size_t a_size = 0, b_size = 0;
	int level;
	for (level = -1; level >= -127; --level)
		assert(coop_campaign_put_world(&first, level, COOP_CAMPAIGN_DESTROYED, NULL, 0));
	for (level = -127; level < 0; ++level)
		assert(coop_campaign_put_world(&second, level, COOP_CAMPAIGN_DESTROYED, NULL, 0));
	assert(coop_campaign_encode(&first, &a, &a_size));
	assert(coop_campaign_encode(&second, &b, &b_size));
	assert(a_size == b_size && !memcmp(a, b, a_size));
	roundtrip(&first, &restored);
	assert(restored.world_count == 127);
	/* Duplicate records, unbounded lengths, and active-world copies are invalid */
	a[56] = a[40];
	assert(!coop_campaign_decode(&restored, a, a_size));
	assert(restored.world_count == 127);
	assert(!coop_campaign_put_world(&first, -128, COOP_CAMPAIGN_DESTROYED, NULL, 0));
	assert(!coop_campaign_put_world(&first, 0, COOP_CAMPAIGN_AVAILABLE, "x", 2));
	assert(coop_campaign_put_world(&first, 4, COOP_CAMPAIGN_AVAILABLE, "x", 2));
	assert(!coop_campaign_valid(&first));
	coop_campaign_clear(&first);
	first = normal_campaign(1);
	large = (unsigned char *) calloc(1, COOP_CAMPAIGN_MAX_WORLD_BYTES);
	assert(large);
	for (level = -1; level >= -7; --level)
		assert(coop_campaign_put_world(&first, level, COOP_CAMPAIGN_AVAILABLE, large, COOP_CAMPAIGN_MAX_WORLD_BYTES));
	assert(!coop_campaign_put_world(&first, -8, COOP_CAMPAIGN_AVAILABLE, large, COOP_CAMPAIGN_MAX_WORLD_BYTES));
	assert(!coop_campaign_put_world(&first, -1, COOP_CAMPAIGN_AVAILABLE, large, COOP_CAMPAIGN_MAX_WORLD_BYTES + 1));
	assert(first.world_count == 7 && coop_campaign_valid(&first));
	/* Multi-world archives exceed the old transport bound and cross the high
	 * bit of the 16-bit chunk index. Check actual bytes through both codecs */
	roundtrip(&first, &restored);
	{
		coop_campaign_travel prepared = { 0 }, decoded = { 0 };
		unsigned char *encoded = NULL;
		size_t encoded_size = 0;
		assert(coop_campaign_prepare_enter(&first, -1, 0, large, COOP_CAMPAIGN_MAX_WORLD_BYTES, &prepared));
		assert(coop_campaign_travel_encode(&prepared, &encoded, &encoded_size));
		assert(encoded_size > 16u * 1024u * 1024u);
		assert(coop_campaign_travel_decode(&decoded, encoded, encoded_size));
		assert(decoded.next.world_count == 7 && decoded.destination.level == -1);
		assert(decoded.destination.size == COOP_CAMPAIGN_MAX_WORLD_BYTES);
		assert(!memcmp(decoded.destination.data, large, COOP_CAMPAIGN_MAX_WORLD_BYTES));
		assert(coop_campaign_commit_travel(&restored, &decoded));
		assert(restored.active_level == -1 && restored.entered_from == 1);
		free(encoded);
		coop_campaign_travel_clear(&prepared);
		coop_campaign_travel_clear(&decoded);
	}
	free(large);
	free(a);
	free(b);
	coop_campaign_clear(&first);
	coop_campaign_clear(&second);
	coop_campaign_clear(&restored);
}

int main(void)
{
	campaign_visits();
	prepared_travel();
	malformed_archives();
	canonical_and_bounded();
	puts("Co-op campaign archive tests passed");
	return 0;
}
