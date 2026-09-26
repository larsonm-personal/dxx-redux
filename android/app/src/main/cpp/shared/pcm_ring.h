#ifndef PCM_RING_H
#define PCM_RING_H

#include <string.h>

/* Fixed SPSC buffer: 131072 stereo frames, about 2.7 seconds at 48 kHz */
#define PCM_RING_SAMPLES (1 << 18)

struct pcm_ring {
	short samples[PCM_RING_SAMPLES];
	volatile int write_position; /* Monotonic write position */
	volatile int read_position;  /* Monotonic read position */
};

/* Both producer and consumer must be quiescent during reset */
static inline void pcm_ring_reset(struct pcm_ring *ring)
{
	__atomic_store_n(&ring->write_position, 0, __ATOMIC_SEQ_CST);
	__atomic_store_n(&ring->read_position, 0, __ATOMIC_SEQ_CST);
}

/* Reader-side discard leaves the producer's monotonic cursor intact */
static inline void pcm_ring_discard(struct pcm_ring *ring)
{
	int write_position = __atomic_load_n(&ring->write_position, __ATOMIC_ACQUIRE);
	__atomic_store_n(&ring->read_position, write_position, __ATOMIC_RELEASE);
}

/* The sole producer must check space with available() before writing count samples */
static inline void pcm_ring_write(struct pcm_ring *ring, const short *data, int count)
{
	unsigned int wpos = (unsigned int) __atomic_load_n(&ring->write_position, __ATOMIC_RELAXED);
	unsigned int idx = wpos & (PCM_RING_SAMPLES - 1);
	int first = PCM_RING_SAMPLES - (int) idx;
	if (first > count) first = count;
	memcpy(&ring->samples[idx], data, first * sizeof(short));
	if (count > first)
		memcpy(&ring->samples[0], data + first, (count - first) * sizeof(short));
	__atomic_store_n(&ring->write_position, (int) (wpos + (unsigned int) count), __ATOMIC_RELEASE);
}

/* The sole consumer receives at most count samples; an empty ring returns zero */
static inline int pcm_ring_read(struct pcm_ring *ring, short *out, int count)
{
	unsigned int wpos = (unsigned int) __atomic_load_n(&ring->write_position, __ATOMIC_ACQUIRE);
	unsigned int rpos = (unsigned int) __atomic_load_n(&ring->read_position, __ATOMIC_RELAXED);
	unsigned int avail = wpos - rpos;
	if ((int) avail < count) count = (int) avail;
	if (count <= 0) return 0;
	unsigned int idx = rpos & (PCM_RING_SAMPLES - 1);
	int first = PCM_RING_SAMPLES - (int) idx;
	if (first > count) first = count;
	memcpy(out, &ring->samples[idx], first * sizeof(short));
	if (count > first)
		memcpy(out + first, &ring->samples[0], (count - first) * sizeof(short));
	__atomic_store_n(&ring->read_position, (int) (rpos + (unsigned int) count), __ATOMIC_RELEASE);
	return count;
}

/* Producer/consumer callers use their own cursor and acquire the other cursor */
static inline unsigned int pcm_ring_available(const struct pcm_ring *ring)
{
	return (unsigned int) __atomic_load_n(&ring->write_position, __ATOMIC_ACQUIRE) -
	       (unsigned int) __atomic_load_n(&ring->read_position, __ATOMIC_ACQUIRE);
}

#endif
