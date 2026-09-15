#ifndef COOP_WORLD_VISIT_H
#define COOP_WORLD_VISIT_H
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Live transport state, deliberately excluded from saves and checkpoints.
 * Session/sync/transfer callers must authenticate authority before adopting */
void coop_world_visit_session(uint32_t session);
uint64_t coop_world_visit_current(void);
uint64_t coop_world_visit_high_water(void);
/* Reservations survive cancellation and host migration; zero means exhausted */
uint64_t coop_world_visit_reserve(void);
void coop_world_visit_observe(uint64_t visit);
/* A replacement must use a fresh reservation; repeated sync is idempotent */
int coop_world_visit_activate(uint64_t visit);
int coop_world_visit_sync(uint64_t visit);
void coop_world_visit_write(unsigned char bytes[8], uint64_t visit);
uint64_t coop_world_visit_read(const unsigned char bytes[8]);

#ifdef __cplusplus
}
#endif
#endif
