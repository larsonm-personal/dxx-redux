/* Exercise the actual producer, callback and lifecycle with real SDL threads
 * and ring storage. Only synth output and the ring publication boundary are
 * controlled; no soundfont, audio device or product test hook is required
 */
#include "pcm_ring.h"
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

static pthread_mutex_t gate_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t gate_cond = PTHREAD_COND_INITIALIZER;
static int gate_waiting, gate_release, writes_until_stop;
static void controlled_ring_write(struct pcm_ring *, const short *, int);
#define pcm_ring_write controlled_ring_write
#include "../../app/src/main/cpp/shared/digi_tsf_music.c"
#undef pcm_ring_write

void music_synth_render_short(music_synth *synth, short *out, int frames, int mixing)
{
	(void) synth;
	(void) mixing;
	for (int i = 0; i < frames * 2; ++i) out[i] = 1234;
}
void music_synth_reset(music_synth *synth)
{
	(void) synth;
}
void music_synth_set_output(music_synth *synth, enum TSFOutputMode mode, int rate, float gain)
{
	(void) synth;
	(void) mode;
	(void) rate;
	(void) gain;
}
void music_synth_channel_set_presetnumber(music_synth *synth, int channel, int preset, int drums)
{
	(void) synth;
	(void) channel;
	(void) preset;
	(void) drums;
}

static void require(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static void controlled_ring_write(struct pcm_ring *ring, const short *samples, int count)
{
	pthread_mutex_lock(&gate_mutex);
	gate_waiting = 1;
	pthread_cond_broadcast(&gate_cond);
	while (!gate_release) pthread_cond_wait(&gate_cond, &gate_mutex);
	pthread_mutex_unlock(&gate_mutex);
	pcm_ring_write(ring, samples, count);
	if (writes_until_stop && --writes_until_stop == 0)
		__atomic_store_n(&g_render_running, 0, __ATOMIC_SEQ_CST);
}

static void wait_for_gate(void)
{
	struct timespec deadline;
	clock_gettime(CLOCK_REALTIME, &deadline);
	deadline.tv_sec += 5;
	pthread_mutex_lock(&gate_mutex);
	while (!gate_waiting)
		require(pthread_cond_timedwait(&gate_cond, &gate_mutex, &deadline) == 0,
		        "producer did not reach publication barrier");
	pthread_mutex_unlock(&gate_mutex);
}

static void release_gate(void)
{
	pthread_mutex_lock(&gate_mutex);
	gate_release = 1;
	pthread_cond_broadcast(&gate_cond);
	pthread_mutex_unlock(&gate_mutex);
}

static void wait_for_value(volatile int *value, int expected, const char *message)
{
	unsigned int started = SDL_GetTicks();
	while (__atomic_load_n(value, __ATOMIC_SEQ_CST) != expected) {
		require(SDL_GetTicks() - started < 5000, message);
		SDL_Delay(1);
	}
}

static int hook_count, replacement_count;
static void finished(void)
{
	++hook_count;
}
static void replacement_finished(void)
{
	++replacement_count;
}

enum source { MIDI,
	          HMP,
	          PCM };
enum scenario { TAIL,
	            FULL,
	            EMPTY,
	            LOOP,
	            PAUSE,
	            STOP,
	            REPLACE };
static const char *source_names[] = { "MIDI", "HMP", "PCM" };
static const char *scenario_names[] = { "tail", "full", "empty", "loop", "pause", "stop", "replace" };

static int prepare(enum source source, enum scenario scenario, void (*hook)(void))
{
	gate_waiting = gate_release = 0;
	writes_until_stop = scenario == LOOP ? 2 : 0;
	tsf_atomic_store_int(&g_source_finished, 0);
	tsf_atomic_store_int(&g_song_finished, 0);
	tsf_atomic_store_int(&g_playing, 1);
	tsf_atomic_store_int(&g_paused, 0);
	tsf_atomic_store_int(&g_bg_paused, 0);
	g_finished_hook = hook;
	g_output_rate = g_pcm_rate = 32000;
	g_is_pcm = source == PCM;
	g_is_hmp = source == HMP;
	g_loop = scenario == LOOP;
	g_playback_msec = g_pcm_pos = 0;
	tsf_atomic_store_float(&g_volume, 1.0f / AUDIO_GAMEPLAY_HEADROOM_SCALE /
	                                      (g_is_pcm ? MUSIC_FILE_VOLUME_SCALE : 1.0f));
	int frames = scenario == FULL ? 2048 : source == MIDI ? 256
	                                   : source == HMP    ? 64
	                                                      : 2;
	if (scenario == EMPTY) frames = 0;
	if (source == PCM) {
		g_pcm_total = (size_t) frames + 1;
		g_pcm_channels = 1;
		g_pcm_buf = malloc(g_pcm_total * sizeof(short));
		require(g_pcm_buf != NULL, "allocate ordinary PCM fixture");
		for (size_t i = 0; i < g_pcm_total; ++i) g_pcm_buf[i] = 1234;
	} else {
		if (scenario != EMPTY) {
			g_midi = calloc(1, sizeof(*g_midi));
			require(g_midi != NULL, "allocate ordinary MIDI fixture");
			g_midi->type = TML_PROGRAM_CHANGE;
			g_midi->time = scenario == FULL ? 63 : 0;
		}
		g_midi_cur = g_midi;
		if (source == HMP) {
			midi_seek_timeline_init(&g_hmp_timeline, g_midi, g_output_rate, 2,
			                        NULL, &hmp_timeline_ops);
			if (scenario != EMPTY)
				require(midi_seek_timeline_set_range(&g_hmp_timeline,
				                                     g_loop ? g_midi : NULL, 0, scenario == FULL ? 64 : 2),
				        "set valid HMP duration");
		}
	}
	require(render_thread_start(), "start actual SDL producer");
	/* Two actual passes prove cursor restart as well as absence of EOF */
	return scenario == LOOP ? (source == MIDI ? frames * 4 : 8192) : frames * 2;
}

static int callback_samples(int count, int expected)
{
	short out[8194];
	require(count <= 8194, "callback fixture bounds");
	memset(out, 0x55, sizeof(out));
	tsf_music_callback(NULL, (Uint8 *) out, count * (int) sizeof(short));
	int delivered = 0;
	for (int i = 0; i < count; ++i) {
		require(out[i] == (i < expected ? 1234 : 0), "exact ordered tail and zero padding");
		if (out[i]) ++delivered;
	}
	return delivered;
}

static void *stop_music(void *unused)
{
	(void) unused;
	mix_stop_music();
	return NULL;
}

static void stop_at_gate(void)
{
	pthread_t thread;
	require(pthread_create(&thread, NULL, stop_music, NULL) == 0, "start explicit stop");
	wait_for_value(&g_render_running, 0, "stop must request producer join");
	release_gate();
	require(pthread_join(thread, NULL) == 0, "explicit stop joins producer");
	require(!g_render_thread && !g_pcm_buf && !g_midi && !g_midi_cur,
	        "explicit stop releases source and worker");
	require(!tsf_atomic_load_int(&g_source_finished) && !tsf_atomic_load_int(&g_song_finished),
	        "explicit stop clears both completion flags");
	callback_samples(4098, 0);
	mix_poll_music();
	require(hook_count == 0, "explicit stop must not dispatch natural completion");
}

static int drain_tail(int expected, int looping)
{
	/* Leave two final samples queued to prove EOF alone is not completion */
	int delivered = callback_samples(expected - 2, expected - 2);
	mix_poll_music();
	require(hook_count == 0 && replacement_count == 0, "no completion before final sample");
	require(pcm_ring_available(&g_rb) == 2, "last stereo frame remains queued");
	delivered += callback_samples(4, 2);
	require(hook_count == 0 && replacement_count == 0, "audio callback must not invoke game hook");
	mix_poll_music();
	callback_samples(4, 0);
	mix_poll_music();
	mix_poll_music();
	require(pcm_ring_available(&g_rb) == 0, "no stranded tail");
	require(tsf_atomic_load_int(&g_playing) == looping, "playing state follows audible completion");
	return delivered;
}

int run_music_completion(const char *trace_path)
{
	FILE *trace = fopen(trace_path, "w");
	require(trace != NULL, "open trace");
	fputs("[\n", trace);
	int record = 0;
	for (enum source source = MIDI; source <= PCM; ++source) {
		for (enum scenario scenario = TAIL; scenario <= REPLACE; ++scenario) {
			hook_count = replacement_count = 0;
			int expected = prepare(source, scenario, finished);
			int delivered = 0;
			if (scenario == EMPTY) {
				wait_for_value(&g_source_finished, 1, "empty source EOF");
				render_thread_stop();
				callback_samples(4, 0);
				require(hook_count == 0, "empty completion remains on game thread");
				mix_poll_music();
				mix_poll_music();
			} else {
				wait_for_gate();
				callback_samples(4098, 0);
				mix_poll_music();
				require(hook_count == 0 && tsf_atomic_load_int(&g_playing),
				        "no premature completion while final write is blocked");
				if (scenario == STOP || scenario == REPLACE) {
					stop_at_gate();
					if (scenario == REPLACE) {
						expected = prepare(source, TAIL, replacement_finished);
						wait_for_gate();
						callback_samples(4098, 0);
						mix_poll_music();
						require(hook_count == 0 && replacement_count == 0,
						        "no stale completion in replacement generation");
					}
				}
				if (scenario != STOP) {
					if (scenario == PAUSE) mix_pause_music();
					release_gate();
					if (scenario != LOOP)
						wait_for_value(&g_source_finished, 1, "final samples precede EOF");
					else
						wait_for_value(&g_render_running, 0, "two complete looping render passes");
					render_thread_stop();
					if (scenario == PAUSE) {
						require(tsf_atomic_load_int(&g_paused), "queued pause applied by producer");
						unsigned int queued = pcm_ring_available(&g_rb);
						callback_samples(4098, 0);
						mix_poll_music();
						require(hook_count == 0 && pcm_ring_available(&g_rb) == queued,
						        "pause preserves tail without completion");
						mix_resume_music();
					}
					delivered = drain_tail(expected, scenario == LOOP);
				}
			}
			require(hook_count == (scenario == LOOP || scenario == STOP || scenario == REPLACE ? 0 : 1),
			        "exact one-shot natural completion");
			require(replacement_count == (scenario == REPLACE ? 1 : 0), "replacement hook isolation");
			fprintf(trace, "%s  {\"source\":\"%s\",\"scenario\":\"%s\",\"delivered\":%d,\"hooks\":%d,\"replacement_hooks\":%d}",
			        record++ ? ",\n" : "", source_names[source], scenario_names[scenario],
			        delivered, hook_count, replacement_count);
			mix_stop_music();
		}
	}
	fputs("\n]\n", trace);
	require(fclose(trace) == 0, "save trace");
	printf("PASS: %d actual production music completion cases\n", record);
	return 0;
}
