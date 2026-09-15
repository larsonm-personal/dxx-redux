#include "coop_world_visit.h"

static uint32_t session_id;
static uint64_t active_visit, high_water;

void coop_world_visit_session(uint32_t session)
{
	if (session == session_id) return;
	session_id = session;
	active_visit = high_water = 0;
}

uint64_t coop_world_visit_current(void)
{
	return active_visit;
}
uint64_t coop_world_visit_high_water(void)
{
	return high_water;
}

uint64_t coop_world_visit_reserve(void)
{
	if (!session_id || high_water == UINT64_MAX) return 0;
	return ++high_water;
}

void coop_world_visit_observe(uint64_t visit)
{
	if (session_id && visit > high_water) high_water = visit;
}

int coop_world_visit_activate(uint64_t visit)
{
	if (!session_id || !visit || visit <= active_visit || visit > high_water) return 0;
	active_visit = visit;
	return 1;
}

int coop_world_visit_sync(uint64_t visit)
{
	if (!session_id || visit < active_visit) return 0;
	coop_world_visit_observe(visit);
	active_visit = visit;
	return 1;
}

void coop_world_visit_write(unsigned char bytes[8], uint64_t visit)
{
	for (unsigned i = 0; i < 8; ++i) bytes[i] = (unsigned char) (visit >> (8 * i));
}

uint64_t coop_world_visit_read(const unsigned char bytes[8])
{
	uint64_t visit = 0;
	for (unsigned i = 0; i < 8; ++i) visit |= (uint64_t) bytes[i] << (8 * i);
	return visit;
}
