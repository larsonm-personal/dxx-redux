#ifndef COOP_CAMPAIGN_H
#define COOP_CAMPAIGN_H

#include <stddef.h>
#include <stdint.h>

/* The archive contains raw world saves, never other campaign archives */
#define COOP_CAMPAIGN_MAX_WORLDS      128
#define COOP_CAMPAIGN_MAX_WORLD_BYTES (2u * 1024u * 1024u)
#define COOP_CAMPAIGN_MAX_BYTES       (16u * 1024u * 1024u)

enum { COOP_CAMPAIGN_AVAILABLE = 1,
	   COOP_CAMPAIGN_DESTROYED = 2 };

typedef struct coop_campaign_world {
	int16_t level;
	uint8_t state;
	unsigned char *data;
	size_t size;
} coop_campaign_world;

typedef struct coop_campaign {
	char mission[9];
	int16_t active_level;
	int16_t entered_from;
	uint8_t base_returnable;
	uint64_t generation;
	unsigned world_count;
	coop_campaign_world worlds[COOP_CAMPAIGN_MAX_WORLDS];
} coop_campaign;

/* Values must be zero-initialized before use; the campaign owns its buffers */
void coop_campaign_clear(coop_campaign *campaign);
int coop_campaign_valid(const coop_campaign *campaign);
int coop_campaign_put_world(coop_campaign *campaign, int level, unsigned state,
                            const void *data, size_t size);
const coop_campaign_world *coop_campaign_find_world(const coop_campaign *campaign, int level);
void coop_campaign_remove_world(coop_campaign *campaign, int level);
/* Canonical, bounded, little-endian representation; caller frees encoded data */
int coop_campaign_encode(const coop_campaign *campaign, unsigned char **data, size_t *size);
/* A failed decode leaves the destination unchanged */
int coop_campaign_decode(coop_campaign *campaign, const void *data, size_t size);

enum coop_campaign_travel_action {
	COOP_CAMPAIGN_TRAVEL_NONE,
	COOP_CAMPAIGN_ENTER,
	COOP_CAMPAIGN_RETURN,
	COOP_CAMPAIGN_ADVANCE,
	COOP_CAMPAIGN_ENDGAME
};

typedef struct coop_campaign_travel {
	coop_campaign next;
	/* Empty destination data means first visit; level zero means endgame */
	coop_campaign_world destination;
	uint64_t source_generation;
	int16_t source_level;
	char mission[9];
	enum coop_campaign_travel_action action;
} coop_campaign_travel;

/* Zero-initialize before use. Preparation never changes the live campaign.
 * The caller owns exit arbitration, the frozen source and engine validation */
void coop_campaign_travel_clear(coop_campaign_travel *travel);
int coop_campaign_prepare_enter(const coop_campaign *source, int secret_level,
                                int reactor_destroyed, const void *world, size_t size,
                                coop_campaign_travel *travel);
int coop_campaign_prepare_return(const coop_campaign *source, int last_normal_level,
                                 int reactor_destroyed, const void *world, size_t size,
                                 coop_campaign_travel *travel);
/* Adopt only after destination application succeeds. A stale commit fails intact */
int coop_campaign_commit_travel(coop_campaign *live, coop_campaign_travel *travel);
/* Canonical prepared operation, including the detached destination. Decode
 * validates ownership/shape and leaves an existing operation intact on failure */
int coop_campaign_travel_encode(const coop_campaign_travel *travel, unsigned char **data, size_t *size);
int coop_campaign_travel_decode(coop_campaign_travel *travel, const void *data, size_t size);

#endif
