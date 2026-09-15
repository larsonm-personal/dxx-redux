#include "coop_world_visit.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(c) do { if (!(c)) { fprintf(stderr, "%s:%d: %s\n", __FILE__, __LINE__, #c); exit(1); } } while (0)

int main(void)
{
	CHECK(!coop_world_visit_reserve());
	coop_world_visit_session(7);
	CHECK(coop_world_visit_reserve() == 1);
	CHECK(coop_world_visit_activate(1));
	CHECK(coop_world_visit_reserve() == 2); /* Prepared replacement, not applied */
	CHECK(coop_world_visit_current() == 1);
	CHECK(coop_world_visit_reserve() == 3); /* Cancelled reservation is never reused */
	CHECK(coop_world_visit_activate(3));
	CHECK(coop_world_visit_reserve() == 4); /* Rollback also replaces the world */
	CHECK(coop_world_visit_activate(4));
	CHECK(!coop_world_visit_activate(4));
	CHECK(!coop_world_visit_sync(1)); /* Historical saved campaign cannot rewind wire state */
	CHECK(coop_world_visit_current() == 4);
	coop_world_visit_session(7);
	CHECK(coop_world_visit_current() == 4);
	CHECK(coop_world_visit_sync(4));
	coop_world_visit_observe(12); /* Pending old-host transfer learned before migration */
	CHECK(coop_world_visit_current() == 4);
	CHECK(coop_world_visit_reserve() == 13);
	CHECK(coop_world_visit_activate(13));
	CHECK(!coop_world_visit_activate(14));
	coop_world_visit_session(8); /* Cold join into another authenticated session */
	CHECK(coop_world_visit_current() == 0);
	CHECK(coop_world_visit_sync(UINT64_C(0x100000001)));
	CHECK(coop_world_visit_reserve() == UINT64_C(0x100000002));
	unsigned char bytes[8];
	coop_world_visit_write(bytes, UINT64_C(0x8877665544332211));
	CHECK(bytes[0] == 0x11 && bytes[7] == 0x88);
	CHECK(coop_world_visit_read(bytes) == UINT64_C(0x8877665544332211));
	coop_world_visit_observe(UINT64_MAX);
	CHECK(!coop_world_visit_reserve());
	CHECK(coop_world_visit_current() == UINT64_C(0x100000001));
	CHECK(coop_world_visit_high_water() == UINT64_MAX);
	return 0;
}
