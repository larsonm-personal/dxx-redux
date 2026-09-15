#include "coop_campaign.h"
#include "coop_save_format.h"
#include <stdlib.h>
#include <string.h>

enum { HEADER_BYTES = 40,
	   WORLD_BYTES = 16,
	   ARCHIVE_VERSION = 1 };
#define ARCHIVE_TAG 0x504d4143u /* CAMP */

static void put32(unsigned char *p, uint32_t value)
{
	unsigned i;
	for (i = 0; i < 4; ++i) p[i] = (unsigned char) (value >> (8 * i));
}

static uint32_t get32(const unsigned char *p)
{
	return (uint32_t) p[0] | (uint32_t) p[1] << 8 |
	       (uint32_t) p[2] << 16 | (uint32_t) p[3] << 24;
}

static int level_valid(int level)
{
	return level && level >= -127 && level <= 127;
}

void coop_campaign_travel_clear(coop_campaign_travel *travel)
{
	if (!travel) return;
	coop_campaign_clear(&travel->next);
	free(travel->destination.data);
	memset(travel, 0, sizeof(*travel));
}

static int copy_campaign(const coop_campaign *source, coop_campaign *copy)
{
	*copy = *source;
	memset(copy->worlds, 0, sizeof(copy->worlds));
	copy->world_count = 0;
	for (unsigned i = 0; i < source->world_count; ++i) {
		const coop_campaign_world *world = &source->worlds[i];
		if (!coop_campaign_put_world(copy, world->level, world->state, world->data, world->size)) return 0;
	}
	return 1;
}

static void take_destination(coop_campaign *campaign, int level, coop_campaign_world *destination)
{
	const coop_campaign_world *world = coop_campaign_find_world(campaign, level);
	destination->level = (int16_t) level;
	destination->state = COOP_CAMPAIGN_AVAILABLE;
	if (!world) return;
	*destination = *world;
	size_t index = (size_t) (world - campaign->worlds);
	--campaign->world_count;
	memmove(campaign->worlds + index, campaign->worlds + index + 1,
	        (campaign->world_count - index) * sizeof(*world));
	memset(campaign->worlds + campaign->world_count, 0, sizeof(*world));
}

static int prepare_travel(const coop_campaign *source, int destination, int destroyed,
                          const void *world, size_t size,
                          enum coop_campaign_travel_action action, coop_campaign_travel *travel)
{
	coop_campaign_travel prepared = { 0 };
	prepared.source_generation = source->generation;
	prepared.source_level = source->active_level;
	memcpy(prepared.mission, source->mission, sizeof(prepared.mission));
	prepared.action = action;
	if (action != COOP_CAMPAIGN_ENDGAME) {
		if ((destroyed && (world || size)) || (!destroyed && (!world || !size)) ||
		    !copy_campaign(source, &prepared.next)) goto fail;
		take_destination(&prepared.next, destination, &prepared.destination);
		prepared.next.active_level = (int16_t) destination;
		prepared.next.entered_from = action == COOP_CAMPAIGN_ENTER ? source->active_level : 0;
		prepared.next.base_returnable = action == COOP_CAMPAIGN_ENTER && !destroyed;
		++prepared.next.generation;
		/* A destroyed base is unavailable; a destroyed secret remains recorded */
		if ((!destroyed || source->active_level < 0) &&
		    !coop_campaign_put_world(&prepared.next, source->active_level,
		                             destroyed ? COOP_CAMPAIGN_DESTROYED : COOP_CAMPAIGN_AVAILABLE,
		                             world, size)) goto fail;
		if (!coop_campaign_valid(&prepared.next)) goto fail;
	}
	coop_campaign_travel_clear(travel);
	*travel = prepared;
	return 1;
fail:
	coop_campaign_travel_clear(&prepared);
	return 0;
}

int coop_campaign_prepare_enter(const coop_campaign *source, int secret_level,
                                int reactor_destroyed, const void *world, size_t size,
                                coop_campaign_travel *travel)
{
	const coop_campaign_world *destination;
	if (!source || !travel || source == &travel->next || !coop_campaign_valid(source) ||
	    source->generation == UINT64_MAX || source->active_level < 1 ||
	    secret_level >= 0 || !level_valid(secret_level) ||
	    (reactor_destroyed != 0 && reactor_destroyed != 1)) return 0;
	destination = coop_campaign_find_world(source, secret_level);
	if (destination && destination->state == COOP_CAMPAIGN_DESTROYED) return 0;
	return prepare_travel(source, secret_level, reactor_destroyed, world, size,
	                      COOP_CAMPAIGN_ENTER, travel);
}

int coop_campaign_prepare_return(const coop_campaign *source, int last_normal_level,
                                 int reactor_destroyed, const void *world, size_t size,
                                 coop_campaign_travel *travel)
{
	int destination;
	enum coop_campaign_travel_action action;
	if (!source || !travel || source == &travel->next || !coop_campaign_valid(source) ||
	    source->generation == UINT64_MAX || source->active_level >= 0 ||
	    last_normal_level < 1 || last_normal_level > 127 || source->entered_from > last_normal_level ||
	    (reactor_destroyed != 0 && reactor_destroyed != 1)) return 0;
	if (source->base_returnable) {
		destination = source->entered_from;
		action = COOP_CAMPAIGN_RETURN;
	} else if (source->entered_from == last_normal_level) {
		destination = 0;
		action = COOP_CAMPAIGN_ENDGAME;
	} else {
		destination = source->entered_from + 1;
		action = COOP_CAMPAIGN_ADVANCE;
	}
	return prepare_travel(source, destination, reactor_destroyed, world, size, action, travel);
}

int coop_campaign_commit_travel(coop_campaign *live, coop_campaign_travel *travel)
{
	if (!live || !travel || live == &travel->next ||
	    travel->action < COOP_CAMPAIGN_ENTER || travel->action > COOP_CAMPAIGN_ENDGAME ||
	    live->generation != travel->source_generation || live->active_level != travel->source_level ||
	    memcmp(live->mission, travel->mission, sizeof(live->mission)) ||
	    !coop_campaign_valid(live)) return 0;
	if (travel->action != COOP_CAMPAIGN_ENDGAME &&
	    (!coop_campaign_valid(&travel->next) || travel->next.generation != live->generation + 1 ||
	     travel->next.active_level != travel->destination.level)) return 0;
	coop_campaign_clear(live);
	*live = travel->next;
	memset(&travel->next, 0, sizeof(travel->next));
	coop_campaign_travel_clear(travel);
	return 1;
}

void coop_campaign_clear(coop_campaign *campaign)
{
	unsigned i;
	if (!campaign) return;
	for (i = 0; i < campaign->world_count && i < COOP_CAMPAIGN_MAX_WORLDS; ++i)
		free(campaign->worlds[i].data);
	memset(campaign, 0, sizeof(*campaign));
}

const coop_campaign_world *coop_campaign_find_world(const coop_campaign *campaign, int level)
{
	unsigned i;
	if (!campaign || campaign->world_count > COOP_CAMPAIGN_MAX_WORLDS) return NULL;
	for (i = 0; i < campaign->world_count; ++i)
		if (campaign->worlds[i].level == level) return &campaign->worlds[i];
	return NULL;
}

void coop_campaign_remove_world(coop_campaign *campaign, int level)
{
	const coop_campaign_world *world = coop_campaign_find_world(campaign, level);
	size_t index;
	if (!world) return;
	index = (size_t) (world - campaign->worlds);
	free(campaign->worlds[index].data);
	--campaign->world_count;
	memmove(campaign->worlds + index, campaign->worlds + index + 1,
	        (campaign->world_count - index) * sizeof(*world));
	memset(campaign->worlds + campaign->world_count, 0, sizeof(*world));
}

int coop_campaign_put_world(coop_campaign *campaign, int level, unsigned state,
                            const void *data, size_t size)
{
	const coop_campaign_world *old;
	unsigned char *copy = NULL;
	size_t total = HEADER_BYTES, index;
	unsigned i;
	if (!campaign || !level_valid(level) ||
	    (state != COOP_CAMPAIGN_AVAILABLE && state != COOP_CAMPAIGN_DESTROYED) ||
	    (state == COOP_CAMPAIGN_DESTROYED && (level > 0 || size)) ||
	    !size != !data || size > COOP_CAMPAIGN_MAX_WORLD_BYTES ||
	    campaign->world_count > COOP_CAMPAIGN_MAX_WORLDS) return 0;
	old = coop_campaign_find_world(campaign, level);
	if (!old && campaign->world_count == COOP_CAMPAIGN_MAX_WORLDS) return 0;
	for (i = 0; i < campaign->world_count; ++i) {
		if (campaign->worlds[i].size > COOP_CAMPAIGN_MAX_WORLD_BYTES) return 0;
		total += WORLD_BYTES + campaign->worlds[i].size;
	}
	total = total - (old ? old->size : 0) + size + (old ? 0 : WORLD_BYTES);
	if (total > COOP_CAMPAIGN_MAX_BYTES) return 0;
	if (size) {
		copy = (unsigned char *) malloc(size);
		if (!copy) return 0;
		memcpy(copy, data, size);
	}
	index = old ? (size_t) (old - campaign->worlds) : campaign->world_count++;
	free(campaign->worlds[index].data);
	campaign->worlds[index].level = (int16_t) level;
	campaign->worlds[index].state = (uint8_t) state;
	campaign->worlds[index].data = copy;
	campaign->worlds[index].size = size;
	return 1;
}

int coop_campaign_valid(const coop_campaign *campaign)
{
	unsigned i, j;
	size_t total = HEADER_BYTES;
	if (!campaign || !campaign->mission[0] || !memchr(campaign->mission, 0, 9) ||
	    !level_valid(campaign->active_level) || !campaign->generation ||
	    campaign->base_returnable > 1 || campaign->world_count > COOP_CAMPAIGN_MAX_WORLDS)
		return 0;
	if (campaign->active_level > 0) {
		if (campaign->entered_from || campaign->base_returnable) return 0;
	} else if (campaign->entered_from < 1 || campaign->entered_from > 127) return 0;
	for (i = 0; i < campaign->world_count; ++i) {
		const coop_campaign_world *world = &campaign->worlds[i];
		if (!level_valid(world->level) || world->level == campaign->active_level ||
		    world->size > COOP_CAMPAIGN_MAX_WORLD_BYTES || !world->size != !world->data)
			return 0;
		if (world->state == COOP_CAMPAIGN_DESTROYED) {
			if (world->level > 0 || world->size) return 0;
		} else if (world->state != COOP_CAMPAIGN_AVAILABLE || !world->size) return 0;
		if (world->level > 0 && (!campaign->base_returnable || world->level != campaign->entered_from))
			return 0;
		for (j = 0; j < i; ++j)
			if (campaign->worlds[j].level == world->level) return 0;
		total += WORLD_BYTES + world->size;
	}
	if (campaign->base_returnable && !coop_campaign_find_world(campaign, campaign->entered_from)) return 0;
	return total <= COOP_CAMPAIGN_MAX_BYTES;
}

int coop_campaign_encode(const coop_campaign *campaign, unsigned char **data, size_t *size)
{
	unsigned i;
	int level;
	size_t bytes = HEADER_BYTES, offset = HEADER_BYTES;
	unsigned char *encoded;
	if (!data || !size || !coop_campaign_valid(campaign)) return 0;
	for (i = 0; i < campaign->world_count; ++i) bytes += WORLD_BYTES + campaign->worlds[i].size;
	encoded = (unsigned char *) calloc(1, bytes);
	if (!encoded) return 0;
	put32(encoded, ARCHIVE_TAG);
	put32(encoded + 4, ARCHIVE_VERSION);
	put32(encoded + 8, campaign->world_count);
	encoded[12] = (unsigned char) campaign->active_level;
	encoded[13] = (unsigned char) campaign->entered_from;
	encoded[14] = campaign->base_returnable;
	put32(encoded + 16, (uint32_t) campaign->generation);
	put32(encoded + 20, (uint32_t) (campaign->generation >> 32));
	memcpy(encoded + 24, campaign->mission, strlen(campaign->mission));
	/* Stable ordering makes captures comparable regardless of insertion order */
	for (level = -127; level <= 127; ++level) {
		const coop_campaign_world *world = coop_campaign_find_world(campaign, level);
		if (!world) continue;
		encoded[offset] = (unsigned char) world->level;
		encoded[offset + 1] = world->state;
		put32(encoded + offset + 4, (uint32_t) world->size);
		put32(encoded + offset + 8, coop_save_checksum(world->data, world->size, 2166136261u));
		if (world->size) memcpy(encoded + offset + WORLD_BYTES, world->data, world->size);
		offset += WORLD_BYTES + world->size;
	}
	*data = encoded;
	*size = bytes;
	return 1;
}

static int signed_level(unsigned char value)
{
	return value < 128 ? value : (int) value - 256;
}

int coop_campaign_decode(coop_campaign *campaign, const void *data, size_t size)
{
	const unsigned char *bytes = (const unsigned char *) data;
	coop_campaign next = { 0 };
	size_t offset = HEADER_BYTES;
	unsigned i, count;
	int previous = -128;
	if (!campaign || !data || size < HEADER_BYTES || size > COOP_CAMPAIGN_MAX_BYTES ||
	    get32(bytes) != ARCHIVE_TAG || get32(bytes + 4) != ARCHIVE_VERSION ||
	    get32(bytes + 8) > COOP_CAMPAIGN_MAX_WORLDS || bytes[15]) return 0;
	for (i = 33; i < HEADER_BYTES; ++i)
		if (bytes[i]) return 0;
	count = get32(bytes + 8);
	next.active_level = (int16_t) signed_level(bytes[12]);
	next.entered_from = bytes[13];
	next.base_returnable = bytes[14];
	next.generation = get32(bytes + 16) | (uint64_t) get32(bytes + 20) << 32;
	memcpy(next.mission, bytes + 24, 9);
	for (i = 0; i < count; ++i) {
		size_t world_size;
		int level;
		if (size - offset < WORLD_BYTES) goto fail;
		level = signed_level(bytes[offset]);
		world_size = get32(bytes + offset + 4);
		if (level <= previous || bytes[offset + 2] || bytes[offset + 3] ||
		    get32(bytes + offset + 12) || world_size > size - offset - WORLD_BYTES ||
		    world_size > COOP_CAMPAIGN_MAX_WORLD_BYTES ||
		    get32(bytes + offset + 8) != coop_save_checksum(bytes + offset + WORLD_BYTES, world_size, 2166136261u) ||
		    !coop_campaign_put_world(&next, level, bytes[offset + 1],
		                             world_size ? bytes + offset + WORLD_BYTES : NULL, world_size)) goto fail;
		previous = level;
		offset += WORLD_BYTES + world_size;
	}
	if (offset != size || !coop_campaign_valid(&next)) goto fail;
	coop_campaign_clear(campaign);
	*campaign = next;
	return 1;
fail:
	coop_campaign_clear(&next);
	return 0;
}

enum { TRAVEL_HEADER_BYTES = 48 };
#define TRAVEL_TAG 0x4c565254u /* TRVL */

static int travel_valid(const coop_campaign_travel *travel)
{
	if (!travel || !level_valid(travel->source_level) || !travel->source_generation ||
	    travel->source_generation == UINT64_MAX || !travel->mission[0] || !memchr(travel->mission, 0, 9) ||
	    travel->action < COOP_CAMPAIGN_ENTER || travel->action > COOP_CAMPAIGN_ENDGAME ||
	    travel->destination.size > COOP_CAMPAIGN_MAX_WORLD_BYTES ||
	    !travel->destination.size != !travel->destination.data) return 0;
	if (travel->action == COOP_CAMPAIGN_ENDGAME)
		return travel->source_level < 0 && !travel->destination.level && !travel->destination.state &&
		       !travel->destination.size && !travel->next.active_level && !travel->next.world_count;
	if (!coop_campaign_valid(&travel->next) || travel->next.generation != travel->source_generation + 1 ||
	    strcmp(travel->mission, travel->next.mission) || travel->next.active_level != travel->destination.level ||
	    travel->destination.state != COOP_CAMPAIGN_AVAILABLE) return 0;
	if (travel->action == COOP_CAMPAIGN_ENTER)
		return travel->source_level > 0 && travel->destination.level < 0 && travel->next.entered_from == travel->source_level;
	return travel->source_level < 0 && travel->destination.level > 0 &&
	       coop_campaign_find_world(&travel->next, travel->source_level) &&
	       (travel->action == COOP_CAMPAIGN_RETURN ? travel->destination.size != 0 : travel->destination.size == 0);
}

int coop_campaign_travel_encode(const coop_campaign_travel *travel, unsigned char **data, size_t *size)
{
	unsigned char *archive = NULL, *bytes;
	size_t archive_size = 0, total;
	uint32_t checksum;
	if (!data || !size || !travel_valid(travel)) return 0;
	if (travel->action != COOP_CAMPAIGN_ENDGAME && !coop_campaign_encode(&travel->next, &archive, &archive_size)) return 0;
	total = TRAVEL_HEADER_BYTES + archive_size + travel->destination.size;
	bytes = (unsigned char *) calloc(1, total);
	if (!bytes) {
		free(archive);
		return 0;
	}
	put32(bytes, TRAVEL_TAG);
	put32(bytes + 4, 1);
	bytes[8] = (unsigned char) travel->action;
	bytes[9] = (unsigned char) travel->source_level;
	bytes[10] = (unsigned char) travel->destination.level;
	bytes[11] = travel->destination.state;
	put32(bytes + 16, (uint32_t) travel->source_generation);
	put32(bytes + 20, (uint32_t) (travel->source_generation >> 32));
	memcpy(bytes + 24, travel->mission, strlen(travel->mission));
	put32(bytes + 36, (uint32_t) archive_size);
	put32(bytes + 40, (uint32_t) travel->destination.size);
	if (archive_size) memcpy(bytes + TRAVEL_HEADER_BYTES, archive, archive_size);
	if (travel->destination.size) memcpy(bytes + TRAVEL_HEADER_BYTES + archive_size, travel->destination.data, travel->destination.size);
	checksum = coop_save_checksum(bytes, 44, 2166136261u);
	put32(bytes + 44, coop_save_checksum(bytes + TRAVEL_HEADER_BYTES, total - TRAVEL_HEADER_BYTES, checksum));
	free(archive);
	*data = bytes;
	*size = total;
	return 1;
}

int coop_campaign_travel_decode(coop_campaign_travel *travel, const void *data, size_t size)
{
	const unsigned char *bytes = (const unsigned char *) data;
	coop_campaign_travel next = { 0 };
	size_t archive_size, destination_size;
	uint32_t checksum;
	if (!travel || !bytes || size < TRAVEL_HEADER_BYTES ||
	    size > TRAVEL_HEADER_BYTES + COOP_CAMPAIGN_MAX_BYTES + COOP_CAMPAIGN_MAX_WORLD_BYTES ||
	    get32(bytes) != TRAVEL_TAG || get32(bytes + 4) != 1 || get32(bytes + 12) ||
	    bytes[33] || bytes[34] || bytes[35]) return 0;
	archive_size = get32(bytes + 36);
	destination_size = get32(bytes + 40);
	if (archive_size > COOP_CAMPAIGN_MAX_BYTES || destination_size > COOP_CAMPAIGN_MAX_WORLD_BYTES ||
	    archive_size + destination_size != size - TRAVEL_HEADER_BYTES) return 0;
	checksum = coop_save_checksum(bytes, 44, 2166136261u);
	if (get32(bytes + 44) != coop_save_checksum(bytes + TRAVEL_HEADER_BYTES, size - TRAVEL_HEADER_BYTES, checksum)) return 0;
	next.action = (enum coop_campaign_travel_action) bytes[8];
	next.source_level = (int16_t) signed_level(bytes[9]);
	next.destination.level = (int16_t) signed_level(bytes[10]);
	next.destination.state = bytes[11];
	next.source_generation = get32(bytes + 16) | (uint64_t) get32(bytes + 20) << 32;
	memcpy(next.mission, bytes + 24, 9);
	if (archive_size && !coop_campaign_decode(&next.next, bytes + TRAVEL_HEADER_BYTES, archive_size)) goto fail;
	if (destination_size) {
		next.destination.data = (unsigned char *) malloc(destination_size);
		if (!next.destination.data) goto fail;
		memcpy(next.destination.data, bytes + TRAVEL_HEADER_BYTES + archive_size, destination_size);
		next.destination.size = destination_size;
	}
	if (!travel_valid(&next)) goto fail;
	coop_campaign_travel_clear(travel);
	*travel = next;
	return 1;
fail:
	coop_campaign_travel_clear(&next);
	return 0;
}
