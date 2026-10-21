#ifndef DXX_COOP_PHASE_TRANSMIT_H
#define DXX_COOP_PHASE_TRANSMIT_H

#include <stdint.h>

/* A new barrier should not wait behind the unchanged-state recovery timer */
typedef struct coop_phase_transmit {
	uint64_t generation;
	uint64_t last_sent_ms;
	unsigned phase;
} coop_phase_transmit;

/* Consume one send opportunity; repeated polls still recover lost packets at 250 ms */
static inline int coop_phase_transmit_poll(coop_phase_transmit *sent, uint64_t now_ms,
                                           uint64_t generation, unsigned phase)
{
	if (sent->generation == generation && sent->phase == phase && now_ms < sent->last_sent_ms + 250)
		return 0;
	sent->generation = generation;
	sent->phase = phase;
	sent->last_sent_ms = now_ms;
	return 1;
}

#endif
