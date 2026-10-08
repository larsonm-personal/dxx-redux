/* Real source reader/resampler/pthread/callback/public controls. Only the
 * OpenSL device endpoint is controlled, with the real two-buffer FIFO contract
 */
#include "../../app/src/main/cpp/shared/cd_preview.c"
#include <math.h>
#include <time.h>

static const short *queued[NUM_BUFFERS];
static int queue_head, queue_count, audible_samples;
static short expected_value;

static void require(int condition, const char *message)
{
	if (!condition) {
		fprintf(stderr, "FAIL: %s\n", message);
		exit(1);
	}
}

static SLresult enqueue(SLAndroidSimpleBufferQueueItf self, const void *buffer, SLuint32 size)
{
	(void) self;
	require(size == sizeof(s_play_bufs[0]), "fixed stereo OpenSL buffer size");
	require(queue_count < NUM_BUFFERS, "OpenSL FIFO capacity");
	queued[(queue_head + queue_count) % NUM_BUFFERS] = buffer;
	++queue_count;
	return SL_RESULT_SUCCESS;
}

static SLresult clear_queue(SLAndroidSimpleBufferQueueItf self)
{
	(void) self;
	queue_head = queue_count = 0;
	return SL_RESULT_SUCCESS;
}

static SLresult set_play_state(SLPlayItf self, SLuint32 state)
{
	(void) self;
	if (state == SL_PLAYSTATE_STOPPED) queue_head = queue_count = 0;
	return SL_RESULT_SUCCESS;
}

static void destroy_player(SLObjectItf self)
{
	(void) self;
	queue_head = queue_count = 0;
}

static const struct SLAndroidSimpleBufferQueueItf_ queue_table = { .Enqueue = enqueue, .Clear = clear_queue };
static const struct SLAndroidSimpleBufferQueueItf_ *const queue_interface = &queue_table;
static const struct SLPlayItf_ play_table = { .SetPlayState = set_play_state };
static const struct SLPlayItf_ *const play_interface = &play_table;
static const struct SLObjectItf_ object_table = { .Destroy = destroy_player };
static const struct SLObjectItf_ *const object_interface = &object_table;

static void prime_queue(void)
{
	s_player_bq = &queue_interface;
	s_player_play = &play_interface;
	s_player_obj = &object_interface;
	queue_head = queue_count = 0;
	s_next_buf = 0;
	for (int i = 0; i < NUM_BUFFERS; ++i) {
		memset(s_play_bufs[i], 0, sizeof(s_play_bufs[i]));
		enqueue(s_player_bq, s_play_bufs[i], sizeof(s_play_bufs[i]));
	}
}

/* Complete the oldest buffer before calling the actual registered callback */
static void consume_device_buffer(void)
{
	require(queue_count == NUM_BUFFERS, "two live OpenSL buffers");
	const short *samples = queued[queue_head];
	int zero_seen = 0;
	for (int i = 0; i < BUF_FRAMES * 2; ++i) {
		if (samples[i]) {
			require(!zero_seen && samples[i] == expected_value,
			        "exact ordered preview samples without stale generation");
			++audible_samples;
		} else {
			zero_seen = 1;
		}
	}
	queue_head = (queue_head + 1) % NUM_BUFFERS;
	--queue_count;
	osl_callback(s_player_bq, NULL);
}

static long long monotonic_ms(void)
{
	struct timespec now;
	clock_gettime(CLOCK_MONOTONIC, &now);
	return (long long) now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

static void wait_for_audio(void)
{
	long long started = monotonic_ms();
	while (!pcm_ring_available(&s_rb)) {
		require(monotonic_ms() - started < 5000, "producer publishes preview audio");
		usleep(1000);
	}
}

static FILE *write_fixture(int sectors, short value)
{
	FILE *file = fopen("preview-tail.bin", "w+b");
	require(file != NULL, "create valid preview audio fixture");
	short sector[SECTOR_SIZE / sizeof(short)];
	for (int i = 0; i < SECTOR_SIZE / (int) sizeof(short); ++i) sector[i] = value;
	for (int i = 0; i < sectors; ++i)
		require(fwrite(sector, sizeof(sector), 1, file) == 1, "write complete CD sector");
	require(fflush(file) == 0, "flush fixture");
	return file;
}

static int prepare(int sectors, int rate, short value)
{
	expected_value = value;
	audible_samples = 0;
	FILE *file = write_fixture(sectors, value);
	s_bin_files[0].fp = file;
	s_num_bin_files = 1;
	s_start_sector = s_read_sector = s_track_file_index = 0;
	s_num_sectors = s_track_end = sectors;
	s_output_rate = rate;
	s_pcm_len = s_pcm_pos = 0;
	s_resample_frac = 0;
	s_output_frames = 0;
	s_paused = 0;
	s_playing = 1;
	s_volume = 1;
	__atomic_store_n(&s_output_failed, 0, __ATOMIC_RELEASE);
	__atomic_store_n(&s_output_enabled, 1, __ATOMIC_RELEASE);
	pcm_ring_reset(&s_rb);
	prime_queue();
	require(render_thread_start(), "start actual preview pthread");
	wait_for_audio();
	return (int) ceil((sectors * FRAMES_PER_SECTOR - 1) * (double) rate / CD_SAMPLE_RATE) * 2;
}

static void drain(int expected)
{
	long long started = monotonic_ms();
	while (cd_preview_get_state(NULL, NULL) != CDP_STOPPED) {
		require(audible_samples <= expected, "no duplicated preview samples");
		consume_device_buffer();
		if (audible_samples < expected)
			require(cd_preview_get_state(NULL, NULL) == CDP_PLAYING,
			        "state stays playing through ring and device-queue drain");
		require(monotonic_ms() - started < 10000, "preview must finish after audible tail");
		usleep(100);
	}
	require(audible_samples == expected, "stopped only after final device buffer consumption");
	require(pcm_ring_available(&s_rb) == 0, "no stranded preview ring samples");
}

static void stop_and_check(void)
{
	cd_preview_stop();
	require(!s_thread_created && !s_num_bin_files && !s_player_obj,
	        "public stop releases producer, file and device");
	require(queue_count == 0 && pcm_ring_available(&s_rb) == 0,
	        "stop discards both queued output domains");
	require(cd_preview_get_state(NULL, NULL) == CDP_STOPPED, "explicit stopped state");
}

int run_cd_preview_completion(const char *trace_path)
{
	static const char *names[] = { "short", "full-chunk", "equal-ring", "longer-ring", "pause", "stop-queued", "replace-queued", "seek-after-eof", "seek-after-completion" };
	FILE *trace = fopen(trace_path, "w");
	require(trace != NULL, "open trace");
	fputs("[\n", trace);
	for (int scenario = 0; scenario < 9; ++scenario) {
		int sectors = scenario == 1 ? 4 : scenario == 2 ? 300
		                              : scenario == 3   ? 301
		                                                : 1;
		int rate = scenario == 1 ? 38400 : scenario == 2 || scenario == 3 ? 32768
		                                                                  : 44100;
		int expected = prepare(sectors, rate, 1234);
		require(cd_preview_get_state(NULL, NULL) == CDP_PLAYING,
		        "source EOF must not report stopped with queued ring audio");
		if (scenario >= 4 && scenario <= 7) {
			/* The first callback submits the complete short tail to OpenSL */
			consume_device_buffer();
			require(pcm_ring_available(&s_rb) == 0 && audible_samples == 0,
			        "tail queued in device but not consumed");
			require(cd_preview_get_state(NULL, NULL) == CDP_PLAYING,
			        "ring empty is not device completion");
		}
		if (scenario == 4) {
			cd_preview_pause();
			require(cd_preview_get_state(NULL, NULL) == CDP_PAUSED, "paused preview state");
			unsigned int queued = pcm_ring_available(&s_rb);
			consume_device_buffer();
			consume_device_buffer();
			require(pcm_ring_available(&s_rb) == queued, "pause preserves ring");
			require(cd_preview_get_state(NULL, NULL) == CDP_PAUSED, "paused tail does not complete state");
			cd_preview_resume();
		}
		if (scenario == 5) {
			stop_and_check();
			require(audible_samples == 0, "explicit stop discards pending tail");
		} else {
			if (scenario == 6) {
				stop_and_check();
				expected = prepare(1, 44100, 2345);
			}
			if (scenario == 7) {
				require(cd_preview_seek(0) == 1, "seek remains usable while EOF drains");
				require(cd_preview_get_state(NULL, NULL) == CDP_PLAYING, "seek clears EOF/completion");
				wait_for_audio();
			}
			drain(expected);
			if (scenario == 8)
				require(cd_preview_seek(0) == 0, "completed preview retains stopped seek policy");
			stop_and_check();
		}
		fprintf(trace, "%s  {\"scenario\":\"%s\",\"audible_samples\":%d}",
		        scenario ? ",\n" : "", names[scenario], audible_samples);
		require(remove("preview-tail.bin") == 0, "remove ordinary fixture");
	}
	/* End-to-end valid BIN/CUE parsing and real platform OpenSL callbacks */
	FILE *cue = fopen("preview-tail.cue", "w");
	require(cue != NULL, "create valid CUE");
	fputs("FILE \"preview-tail.bin\" BINARY\n  TRACK 01 AUDIO\n    INDEX 01 00:00:00\n", cue);
	require(fclose(cue) == 0, "save valid CUE");
	for (int full = 0; full < 2; ++full) {
		FILE *file = write_fixture(full ? 4 : 1, 1234);
		require(fclose(file) == 0, "close seed before public start");
		require(cd_preview_start("preview-tail.bin", "preview-tail.cue", 1,
		                         full ? 38400 : 44100),
		        "public preview with real OpenSL");
		require(cd_preview_get_state(NULL, NULL) == CDP_PLAYING, "real preview initially playing");
		long long started = monotonic_ms();
		while (cd_preview_get_state(NULL, NULL) != CDP_STOPPED) {
			require(monotonic_ms() - started < 10000, "real OpenSL consumes final preview buffer");
			usleep(1000);
		}
		long long frames = __atomic_load_n(&s_output_frames, __ATOMIC_RELAXED);
		require(frames == (full ? 2048 : 587), "real output receives complete valid track");
		require(pcm_ring_available(&s_rb) == 0, "real output leaves no ring tail");
		for (int i = 0; i < NUM_BUFFERS; ++i)
			require(s_buffer_has_audio[i] == 0, "real output has consumed each audio buffer");
		fprintf(trace, ",\n  {\"scenario\":\"real-opensl-%s\",\"output_frames\":%lld}",
		        full ? "full-chunk" : "short", frames);
		cd_preview_stop();
		require(!s_player_obj && !s_thread_created && !s_num_bin_files, "real preview stop releases resources");
		require(remove("preview-tail.bin") == 0, "remove real preview fixture");
	}
	require(remove("preview-tail.cue") == 0, "remove CUE fixture");
	fputs("\n]\n", trace);
	require(fclose(trace) == 0, "save trace");
	puts("PASS: 11 actual production CD preview completion cases");
	return 0;
}
