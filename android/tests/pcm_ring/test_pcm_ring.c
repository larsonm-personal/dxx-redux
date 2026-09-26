#include "pcm_ring.h"

#include <assert.h>
#include <limits.h>
#include <pthread.h>
#include <sched.h>
#include <stdio.h>
#include <unistd.h>

static struct pcm_ring ring, independent;
static short input[PCM_RING_SAMPLES], output[PCM_RING_SAMPLES + 1];
enum { STREAM_SAMPLES = PCM_RING_SAMPLES * 32 };

static short sample(unsigned int index)
{
	return (short) ((index ^ (index >> 15) ^ ((index * 1103515245u) >> 16)) & 0x7fffu);
}

static void boundaries(void)
{
	const unsigned int starts[] = { 0, PCM_RING_SAMPLES - 3, INT_MAX - 2u, UINT_MAX - 2u };
	for (unsigned int i = 0; i < PCM_RING_SAMPLES; ++i) input[i] = sample(i);

	for (unsigned int test = 0; test < sizeof(starts) / sizeof(starts[0]); ++test) {
		pcm_ring_reset(&ring);
		ring.write_position = ring.read_position = (int) starts[test];
		output[0] = -1;
		assert(pcm_ring_read(&ring, output, 1) == 0 && output[0] == -1);
		pcm_ring_write(&ring, input, PCM_RING_SAMPLES);
		assert(pcm_ring_available(&ring) == PCM_RING_SAMPLES);
		output[PCM_RING_SAMPLES] = -1;
		assert(pcm_ring_read(&ring, output, PCM_RING_SAMPLES + 1) == PCM_RING_SAMPLES);
		assert(memcmp(input, output, sizeof(input)) == 0 && output[PCM_RING_SAMPLES] == -1);
		assert(pcm_ring_available(&ring) == 0);

		pcm_ring_write(&ring, input, 7);
		assert(pcm_ring_read(&ring, output, 3) == 3);
		assert(pcm_ring_available(&ring) == 4 && memcmp(input, output, 3 * sizeof(short)) == 0);
		output[4] = -1;
		assert(pcm_ring_read(&ring, output, 6) == 4);
		assert(memcmp(input + 3, output, 4 * sizeof(short)) == 0 && output[4] == -1);
		pcm_ring_write(&ring, input, 7);
		int published = ring.write_position;
		pcm_ring_discard(&ring);
		assert(pcm_ring_available(&ring) == 0 && ring.write_position == published);
		pcm_ring_write(&ring, input + 7, 5);
		assert(pcm_ring_read(&ring, output, 5) == 5 && memcmp(input + 7, output, 5 * sizeof(short)) == 0);
		pcm_ring_write(&ring, input, 7);
		pcm_ring_reset(&ring);
		assert(ring.write_position == 0 && ring.read_position == 0 && pcm_ring_read(&ring, output, 1) == 0);
	}
	pcm_ring_write(&independent, input, 7);
	pcm_ring_write(&ring, input + 7, 3);
	pcm_ring_reset(&ring);
	assert(pcm_ring_read(&independent, output, 7) == 7 && memcmp(input, output, 7 * sizeof(short)) == 0);
}

static void *produce(void *unused)
{
	(void) unused;
	short chunk[4093];
	for (unsigned int sent = 0; sent < STREAM_SAMPLES;) {
		unsigned int count = 1 + sent % 4093;
		if (count > STREAM_SAMPLES - sent) count = STREAM_SAMPLES - sent;
		if (PCM_RING_SAMPLES - pcm_ring_available(&ring) < count) {
			sched_yield();
			continue;
		}
		for (unsigned int i = 0; i < count; ++i) chunk[i] = sample(sent + i);
		pcm_ring_write(&ring, chunk, (int) count);
		sent += count;
	}
	return NULL;
}

static void concurrent_stream(void)
{
	pthread_t producer;
	short chunk[997];
	pcm_ring_reset(&ring);
	/* Cross both array and 32-bit cursor boundaries with a live producer */
	ring.write_position = ring.read_position = (int) (UINT_MAX - PCM_RING_SAMPLES);
	assert(pthread_create(&producer, NULL, produce, NULL) == 0);
	for (unsigned int received = 0; received < STREAM_SAMPLES;) {
		int count = pcm_ring_read(&ring, chunk, 997);
		if (!count) sched_yield();
		for (int i = 0; i < count; ++i) assert(chunk[i] == sample(received + (unsigned int) i));
		received += (unsigned int) count;
	}
	assert(pthread_join(producer, NULL) == 0);
	assert(pcm_ring_available(&ring) == 0 && pcm_ring_read(&ring, chunk, 997) == 0);
}

int main(void)
{
	alarm(55);
	boundaries();
	for (int i = 0; i < 4; ++i) concurrent_stream();
	puts("PCM ring boundaries, underruns, discard/reset, isolation and concurrent streams passed");
	return 0;
}
