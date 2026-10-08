/* Actual Redbook sector reader, resampler, producer, callback and public
 * lifecycle. Test-local barriers control real ring writes and stdio reads
 */
#include "pcm_ring.h"
#include <pthread.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cond = PTHREAD_COND_INITIALIZER;
static int gate_waiting, gate_release, gate_on_read, gate_write, writes;
static size_t controlled_fread(void *, size_t, size_t, FILE *);
static void controlled_ring_write(struct pcm_ring *, const short *, int);
#define fread          controlled_fread
#define pcm_ring_write controlled_ring_write
#include "../../app/src/main/cpp/shared/rbaudio_bin.c"
#undef pcm_ring_write
#undef fread

static void require(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static void block_publication(void)
{
	pthread_mutex_lock(&gate_mutex);
	gate_waiting = 1;
	pthread_cond_broadcast(&gate_cond);
	while (!gate_release) pthread_cond_wait(&gate_cond, &gate_mutex);
	pthread_mutex_unlock(&gate_mutex);
}

static size_t controlled_fread(void *out, size_t size, size_t count, FILE *file)
{
	size_t got = fread(out, size, count, file);
	if (gate_on_read) {
		gate_on_read = 0;
		block_publication();
	}
	return got;
}

static void controlled_ring_write(struct pcm_ring *ring, const short *samples, int count)
{
	if (++writes == gate_write) block_publication();
	pcm_ring_write(ring, samples, count);
}

static void wait_for_gate(void)
{
	struct timespec deadline;
	clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += 5;
	pthread_mutex_lock(&gate_mutex);
	while (!gate_waiting)
		require(pthread_cond_timedwait(&gate_cond, &gate_mutex, &deadline) == 0,
		        "producer did not reach controlled boundary");
	pthread_mutex_unlock(&gate_mutex);
}

static void release_gate(void)
{
	pthread_mutex_lock(&gate_mutex);
	gate_release = 1;
	pthread_cond_broadcast(&gate_cond);
	pthread_mutex_unlock(&gate_mutex);
}

/* Advance only the game-thread poll clock, without sleeping for two seconds */
fix64 timer_query(void)
{
	static fix64 ticks;
	ticks += F2_0;
	return ticks;
}

static int hooks, replacement_hooks;
static void finished(void)
{
	++hooks;
}
static void replacement_finished(void)
{
	++replacement_hooks;
}

static int consume(int count, int expected)
{
	short out[4096];
	require(count <= 4096, "callback bounds");
	memset(out, 0x55, sizeof(out));
	rba_music_callback(NULL, (Uint8 *) out, count * (int) sizeof(short));
	for (int i = 0; i < count; ++i)
		require(out[i] == (i < expected ? 1234 : 0), "exact CD tail and zero padding");
	require(hooks == 0 && replacement_hooks == 0, "callback does not execute game hook");
	return expected;
}

static void poll_without_completion(void)
{
	RBACheckFinishedHook();
	require(hooks == 0 && replacement_hooks == 0, "no completion before final delivered sample");
}

static void prepare(int sectors, int rate, int range)
{
	gate_waiting = gate_release = writes = 0;
	hooks = replacement_hooks = 0;
	s_initialised = 1;
	s_output_rate = rate;
	s_volume = 1.0f / AUDIO_GAMEPLAY_HEADROOM_SCALE;
	s_num_tracks = range ? 2 : 1;
	s_num_sources = 1;
	s_sources[0].num_bins = 1;
	FILE *file = fopen("redbook-tail.bin", "w+b");
	require(file != NULL, "create ordinary CD audio fixture");
	short sector[SECTOR_SIZE / sizeof(short)];
	for (int i = 0; i < SECTOR_SIZE / (int) sizeof(short); ++i) sector[i] = 1234;
	for (int i = 0; i < sectors; ++i)
		require(fwrite(sector, sizeof(sector), 1, file) == 1, "write complete audio sector");
	require(fflush(file) == 0, "flush fixture");
	s_sources[0].bin_files[0].sf = file;
	memset(s_tracks, 0, sizeof(s_tracks));
	s_tracks[0].type = 1;
	s_tracks[0].num_sectors = range ? 1 : sectors;
	if (range) {
		s_tracks[1].type = 1;
		s_tracks[1].start_sector = 1;
		s_tracks[1].num_sectors = sectors - 1;
	}
	require(RBAPlayTracks(1, s_num_tracks, finished), "public Redbook play request");
}

static void cleanup(void)
{
	RBAStop();
	render_thread_stop();
	bh_close(&s_sources[0].bin_files[0]);
	require(remove("redbook-tail.bin") == 0, "remove fixture");
}

static int drain(int expected, int dispatch)
{
	int delivered = 0;
	unsigned int started = SDL_GetTicks();
	/* Retain the final stereo frame even when EOF is already visible */
	while (delivered < expected - 2) {
		unsigned int available = pcm_ring_available(&s_rb);
		int count = expected - 2 - delivered;
		if (count > 4096) count = 4096;
		if ((unsigned int) count > available) count = (int) available;
		if (count) delivered += consume(count, count);
		poll_without_completion();
		require(SDL_GetTicks() - started < 10000, "all queued CD samples must remain drainable");
		if (!count) SDL_Delay(1);
	}
	while (pcm_ring_available(&s_rb) != 2) {
		poll_without_completion();
		require(SDL_GetTicks() - started < 10000, "final stereo frame must be published");
		SDL_Delay(1);
	}
	poll_without_completion();
	delivered += consume(4, 2);
	/* Exact full chunks may publish EOF on the next producer pass */
	while (RBAPeekPlayStatus() != 0) {
		consume(4, 0);
		require(SDL_GetTicks() - started < 10000, "audible completion after tail");
		SDL_Delay(1);
	}
	if (dispatch) {
		RBACheckFinishedHook();
		RBACheckFinishedHook();
	}
	require(pcm_ring_available(&s_rb) == 0, "no stranded CD tail");
	return delivered;
}

int run_redbook_completion(const char *trace_path)
{
	static const char *names[] = { "short", "full-chunk", "equal-ring", "longer-ring", "pause", "stop", "replace-blocked", "replace-complete", "range", "stop-complete" };
	FILE *trace = fopen(trace_path, "w");
	require(trace != NULL, "open trace");
	fputs("[\n", trace);
	for (int scenario = 0; scenario < 10; ++scenario) {
		int sectors = scenario == 1 ? 4 : scenario == 2 ? 300
		                              : scenario == 3   ? 301
		                              : scenario == 8   ? 2
		                                                : 1;
		int rate = scenario == 1 ? 38400 : scenario == 2 || scenario == 3 ? 32768
		                                                                  : 44100;
		int expected = (int) ceil((sectors * FRAMES_PER_SECTOR - 1) * (double) rate / CD_SAMPLE_RATE) * 2;
		int delivered = 0;
		gate_on_read = scenario == 5 || scenario == 6;
		gate_write = scenario <= 1 || scenario == 4 ? 1 : 0;
		prepare(sectors, rate, scenario == 8);
		if (gate_write || scenario == 5 || scenario == 6) {
			wait_for_gate();
			consume(4, 0);
			poll_without_completion();
			if (scenario == 5) {
				RBAStop();
				release_gate();
				render_thread_stop();
				consume(4, 0);
				poll_without_completion();
				require(RBAPeekPlayStatus() == 0, "explicit stop remains non-completing");
			} else if (scenario == 6) {
				require(RBAPlayTracks(1, 1, replacement_finished), "replace blocked source request");
				release_gate();
			} else {
				release_gate();
			}
		}
		if (scenario == 4) {
			RBAPause();
			unsigned int queued = pcm_ring_available(&s_rb);
			consume(4, 0);
			poll_without_completion();
			require(pcm_ring_available(&s_rb) == queued, "pause preserves queued tail");
			require(RBAResume() == 1, "resume draining source");
		}
		if (scenario != 5) delivered = drain(expected, scenario != 7 && scenario != 9);
		if (scenario == 7) {
			/* A completed but unpolled old hook must not survive replacement */
			require(RBAPlayTracks(1, 1, replacement_finished), "replace completed request");
			poll_without_completion();
			delivered += drain(expected, 1);
		}
		if (scenario == 9) {
			RBAStop();
			consume(4, 0);
			poll_without_completion();
		}
		require(hooks == (scenario == 5 || scenario == 6 || scenario == 7 || scenario == 9 ? 0 : 1), "one-shot CD hook");
		require(replacement_hooks == (scenario == 6 || scenario == 7 ? 1 : 0), "generation-isolated CD hook");
		fprintf(trace, "%s  {\"scenario\":\"%s\",\"samples\":%d,\"hooks\":%d,\"replacement_hooks\":%d}",
		        scenario ? ",\n" : "", names[scenario], delivered, hooks, replacement_hooks);
		cleanup();
	}
	fputs("\n]\n", trace);
	require(fclose(trace) == 0, "save trace");
	puts("PASS: 10 actual production Redbook completion cases");
	return 0;
}
