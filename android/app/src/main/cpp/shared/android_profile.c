#if defined(ANDROID) || defined(__ANDROID__)

#include "android_profile.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "android_log.h"

#define ANDROID_PROFILE_PERIOD_MS            10000LL
#define ANDROID_PROFILE_WINDOW_MS            1000LL
#define ANDROID_PROFILE_BATCH_CAPACITY       65536
#define ANDROID_PROFILE_TEXTURE_THRESHOLD_US 10000LL
#define ANDROID_PROFILE_TEXTURE_BURST_GAP_US 250000LL
#define ANDROID_PROFILE_STORAGE_THRESHOLD_US 2000LL

enum android_profile_gl_metric {
	ANDROID_PROFILE_GL_SWAP = 0,
	ANDROID_PROFILE_GL_GPU,
	ANDROID_PROFILE_GL_RESOLVE,
	ANDROID_PROFILE_GL_ERROR,
	ANDROID_PROFILE_GL_COUNT
};

struct android_profile_bucket_state {
	long long frame_us;
	long long sample_total_us;
	long long sample_max_us;
	long long start_us;
	int active;
};

struct android_profile_texture_burst_state {
	unsigned int sample_id;
	unsigned int load_count;
	unsigned int slow_load_count;
	unsigned int ktx2_count;
	unsigned int png_count;
	unsigned int stock_count;
	unsigned int other_count;
	unsigned int total_ktx2_attempts;
	unsigned int total_png_attempts;
	long long start_us;
	long long last_end_us;
	long long total_us;
	long long max_us;
	long long total_ktx2_read_us;
	long long total_png_read_us;
	long long total_upload_us;
	long long total_mask_us;
	long long total_ktx2_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_COUNT];
	long long total_png_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_COUNT];
	long long total_png_ext_us[ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_COUNT];
	int active;
	char game[8];
	char max_name[64];
	char max_source[16];
};

static unsigned int g_android_profile_sample_id;
static unsigned int g_android_profile_frame_id;
static unsigned int g_android_profile_sample_frame_count;
static int g_android_profile_sample_active;
static int g_android_profile_frame_active;
static long long g_android_profile_next_sample_ms;
static long long g_android_profile_sample_start_ms;
static long long g_android_profile_sample_end_ms;
static long long g_android_profile_frame_start_us;
static long long g_android_profile_sample_total_us;
static long long g_android_profile_sample_max_us;
static long long g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_COUNT];
static long long g_android_profile_gl_sample_total_us[ANDROID_PROFILE_GL_COUNT];
static long long g_android_profile_gl_sample_max_us[ANDROID_PROFILE_GL_COUNT];
static size_t g_android_profile_batch_len;
static char g_android_profile_game[8] = "";
static char g_android_profile_batch[ANDROID_PROFILE_BATCH_CAPACITY];
static struct android_profile_bucket_state g_android_profile_buckets[ANDROID_PROFILE_BUCKET_COUNT];
static struct android_profile_texture_burst_state g_android_profile_texture_burst;

static const char *g_android_profile_bucket_names[ANDROID_PROFILE_BUCKET_COUNT] = {
	"wait",
	"sim",
	"render",
	"replay",
};

static const char *g_android_profile_gl_metric_names[ANDROID_PROFILE_GL_COUNT] = {
	"swap",
	"gpu",
	"resolve",
	"glerr",
};

static const char *g_android_profile_texture_lookup_slot_names[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_COUNT] = {
	"set",
	"pref",
	"base",
};

static const char *g_android_profile_texture_lookup_ext_names[ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_COUNT] = {
	"png",
	"jpg",
	"tga",
};

static long long android_profile_read_clock_ms(clockid_t clock_id)
{
	struct timespec ts;

	clock_gettime(clock_id, &ts);
	return (long long) ts.tv_sec * 1000LL + (long long) ts.tv_nsec / 1000000LL;
}

static long long android_profile_now_us(void)
{
	struct timespec ts;

	clock_gettime(CLOCK_MONOTONIC, &ts);
	return (long long) ts.tv_sec * 1000000LL + (long long) ts.tv_nsec / 1000LL;
}

static long long android_profile_now_ms(void)
{
	return android_profile_read_clock_ms(CLOCK_MONOTONIC);
}

static long long android_profile_wall_ms(void)
{
	return android_profile_read_clock_ms(CLOCK_REALTIME);
}

static void android_profile_copy_string(char *dst, size_t dst_size,
                                        const char *src,
                                        const char *fallback)
{
	const char *value = src;

	if (!value || !value[0])
		value = fallback;
	if (!value)
		value = "";

	snprintf(dst, dst_size, "%s", value);
}

static void android_profile_reset_batch(void)
{
	g_android_profile_batch_len = 0;
	g_android_profile_batch[0] = '\0';
}

static void android_profile_flush_batch(void)
{
	if (!g_android_profile_batch_len)
		return;

	debug_log_batch(DLOG_PROFILING, g_android_profile_batch);
	android_profile_reset_batch();
}

static void android_profile_copy_game(const char *game)
{
	android_profile_copy_string(g_android_profile_game,
	                            sizeof(g_android_profile_game),
	                            game,
	                            "unknown");
}

static void android_profile_append_line(const char *line)
{
	const size_t line_len = strlen(line);

	if (!line_len || line_len + 2 > sizeof(g_android_profile_batch))
		return;

	if (g_android_profile_batch_len + line_len + 2 > sizeof(g_android_profile_batch))
		android_profile_flush_batch();

	memcpy(g_android_profile_batch + g_android_profile_batch_len, line, line_len);
	g_android_profile_batch_len += line_len;
	g_android_profile_batch[g_android_profile_batch_len++] = '\n';
	g_android_profile_batch[g_android_profile_batch_len] = '\0';
}

static void android_profile_appendf(const char *fmt, ...)
{
	char line[1024];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);

	android_profile_append_line(line);
}

static const char *android_profile_texture_lookup_slot_name(int slot)
{
	if (slot < 0 || slot >= ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_COUNT)
		return "none";

	return g_android_profile_texture_lookup_slot_names[slot];
}

static const char *android_profile_texture_lookup_ext_name(int ext)
{
	if (ext < 0 || ext >= ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_COUNT)
		return "none";

	return g_android_profile_texture_lookup_ext_names[ext];
}

static void android_profile_reset_texture_burst(void)
{
	memset(&g_android_profile_texture_burst, 0,
	       sizeof(g_android_profile_texture_burst));
}

static void android_profile_finish_texture_burst(const char *reason)
{
	const long long avg_us = g_android_profile_texture_burst.load_count ? g_android_profile_texture_burst.total_us / (long long) g_android_profile_texture_burst.load_count : 0;
	const long long span_us =
	    g_android_profile_texture_burst.last_end_us > g_android_profile_texture_burst.start_us ? g_android_profile_texture_burst.last_end_us - g_android_profile_texture_burst.start_us : 0;
	const char *burst_reason = (reason && reason[0]) ? reason : "flush";

	if (!g_android_profile_texture_burst.active)
		return;

	android_profile_appendf(
	    "prof_v=1 type=texture_burst sample=%u game=%s reason=%s loads=%u slow_loads=%u span_us=%lld total_us=%lld avg_us=%lld max_us=%lld max_name=%s max_source=%s ktx2_loads=%u png_loads=%u stock_loads=%u other_loads=%u ktx2_read_us=%lld png_read_us=%lld upload_us=%lld mask_us=%lld ktx2_attempts=%u png_attempts=%u ktx2_set_us=%lld ktx2_pref_us=%lld ktx2_base_us=%lld png_set_us=%lld png_pref_us=%lld png_base_us=%lld png_png_us=%lld png_jpg_us=%lld png_tga_us=%lld",
	    g_android_profile_texture_burst.sample_id,
	    g_android_profile_texture_burst.game,
	    burst_reason,
	    g_android_profile_texture_burst.load_count,
	    g_android_profile_texture_burst.slow_load_count,
	    span_us,
	    g_android_profile_texture_burst.total_us,
	    avg_us,
	    g_android_profile_texture_burst.max_us,
	    g_android_profile_texture_burst.max_name[0] ? g_android_profile_texture_burst.max_name : "unknown",
	    g_android_profile_texture_burst.max_source[0] ? g_android_profile_texture_burst.max_source : "unknown",
	    g_android_profile_texture_burst.ktx2_count,
	    g_android_profile_texture_burst.png_count,
	    g_android_profile_texture_burst.stock_count,
	    g_android_profile_texture_burst.other_count,
	    g_android_profile_texture_burst.total_ktx2_read_us,
	    g_android_profile_texture_burst.total_png_read_us,
	    g_android_profile_texture_burst.total_upload_us,
	    g_android_profile_texture_burst.total_mask_us,
	    g_android_profile_texture_burst.total_ktx2_attempts,
	    g_android_profile_texture_burst.total_png_attempts,
	    g_android_profile_texture_burst.total_ktx2_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_SET],
	    g_android_profile_texture_burst.total_ktx2_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_PREFIX],
	    g_android_profile_texture_burst.total_ktx2_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_BASE],
	    g_android_profile_texture_burst.total_png_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_SET],
	    g_android_profile_texture_burst.total_png_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_PREFIX],
	    g_android_profile_texture_burst.total_png_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_BASE],
	    g_android_profile_texture_burst.total_png_ext_us[ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_PNG],
	    g_android_profile_texture_burst.total_png_ext_us[ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_JPG],
	    g_android_profile_texture_burst.total_png_ext_us[ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_TGA]);
	android_profile_reset_texture_burst();
}

static void android_profile_maybe_finish_texture_burst(long long now_us,
                                                       const char *reason)
{
	if (!g_android_profile_texture_burst.active)
		return;
	if (reason && reason[0]) {
		android_profile_finish_texture_burst(reason);
		return;
	}
	if (now_us - g_android_profile_texture_burst.last_end_us >=
	    ANDROID_PROFILE_TEXTURE_BURST_GAP_US)
		android_profile_finish_texture_burst("idle");
}

static void android_profile_note_texture_burst(const char *game,
                                               const char *name,
                                               const char *source,
                                               long long total_us,
                                               long long ktx2_read_us,
                                               long long png_read_us,
                                               long long upload_us,
						  long long mask_us,
						  const struct android_profile_texture_lookup_metrics *lookup)
{
	int i;
	const char *source_name = (source && source[0]) ? source : "unknown";
	const long long now_us = android_profile_now_us();

	android_profile_maybe_finish_texture_burst(now_us, NULL);

	if (!g_android_profile_texture_burst.active) {
		g_android_profile_texture_burst.active = 1;
		g_android_profile_texture_burst.sample_id =
		    g_android_profile_sample_active ? g_android_profile_sample_id : 0;
		g_android_profile_texture_burst.start_us = now_us;
		android_profile_copy_string(g_android_profile_texture_burst.game,
		                            sizeof(g_android_profile_texture_burst.game),
		                            game,
		                            g_android_profile_game);
	}

	g_android_profile_texture_burst.last_end_us = now_us;
	g_android_profile_texture_burst.load_count++;
	g_android_profile_texture_burst.total_us += total_us;
	g_android_profile_texture_burst.total_ktx2_read_us += ktx2_read_us;
	g_android_profile_texture_burst.total_png_read_us += png_read_us;
	g_android_profile_texture_burst.total_upload_us += upload_us;
	g_android_profile_texture_burst.total_mask_us += mask_us;
	if (lookup) {
		g_android_profile_texture_burst.total_ktx2_attempts += lookup->ktx2_attempts;
		g_android_profile_texture_burst.total_png_attempts += lookup->png_attempts;
		for (i = 0; i < ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_COUNT; i++) {
			g_android_profile_texture_burst.total_ktx2_slot_us[i] += lookup->ktx2_slot_us[i];
			g_android_profile_texture_burst.total_png_slot_us[i] += lookup->png_slot_us[i];
		}
		for (i = 0; i < ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_COUNT; i++)
			g_android_profile_texture_burst.total_png_ext_us[i] += lookup->png_ext_us[i];
	}
	if (total_us >= ANDROID_PROFILE_TEXTURE_THRESHOLD_US)
		g_android_profile_texture_burst.slow_load_count++;

	if (!strcmp(source_name, "ktx2"))
		g_android_profile_texture_burst.ktx2_count++;
	else if (!strcmp(source_name, "png"))
		g_android_profile_texture_burst.png_count++;
	else if (!strcmp(source_name, "stock"))
		g_android_profile_texture_burst.stock_count++;
	else
		g_android_profile_texture_burst.other_count++;

	if (total_us > g_android_profile_texture_burst.max_us) {
		g_android_profile_texture_burst.max_us = total_us;
		android_profile_copy_string(g_android_profile_texture_burst.max_name,
		                            sizeof(g_android_profile_texture_burst.max_name),
		                            name,
		                            "unknown");
		android_profile_copy_string(g_android_profile_texture_burst.max_source,
		                            sizeof(g_android_profile_texture_burst.max_source),
		                            source_name,
		                            "unknown");
	}
}

static void android_profile_reset_frame_metrics(void)
{
	int i;

	for (i = 0; i < ANDROID_PROFILE_BUCKET_COUNT; i++) {
		g_android_profile_buckets[i].frame_us = 0;
		g_android_profile_buckets[i].start_us = 0;
		g_android_profile_buckets[i].active = 0;
	}

	for (i = 0; i < ANDROID_PROFILE_GL_COUNT; i++)
		g_android_profile_gl_frame_us[i] = 0;
}

static void android_profile_reset_sample_metrics(void)
{
	int i;

	for (i = 0; i < ANDROID_PROFILE_BUCKET_COUNT; i++) {
		g_android_profile_buckets[i].sample_total_us = 0;
		g_android_profile_buckets[i].sample_max_us = 0;
	}

	for (i = 0; i < ANDROID_PROFILE_GL_COUNT; i++) {
		g_android_profile_gl_sample_total_us[i] = 0;
		g_android_profile_gl_sample_max_us[i] = 0;
	}
}

static void android_profile_finish_open_buckets(long long now_us)
{
	int i;

	for (i = 0; i < ANDROID_PROFILE_BUCKET_COUNT; i++) {
		if (!g_android_profile_buckets[i].active)
			continue;
		g_android_profile_buckets[i].frame_us += now_us - g_android_profile_buckets[i].start_us;
		g_android_profile_buckets[i].active = 0;
		g_android_profile_buckets[i].start_us = 0;
	}
}

static void android_profile_commit_frame_metrics(void)
{
	int i;

	for (i = 0; i < ANDROID_PROFILE_BUCKET_COUNT; i++) {
		g_android_profile_buckets[i].sample_total_us += g_android_profile_buckets[i].frame_us;
		if (g_android_profile_buckets[i].frame_us > g_android_profile_buckets[i].sample_max_us)
			g_android_profile_buckets[i].sample_max_us = g_android_profile_buckets[i].frame_us;
	}

	for (i = 0; i < ANDROID_PROFILE_GL_COUNT; i++) {
		g_android_profile_gl_sample_total_us[i] += g_android_profile_gl_frame_us[i];
		if (g_android_profile_gl_frame_us[i] > g_android_profile_gl_sample_max_us[i])
			g_android_profile_gl_sample_max_us[i] = g_android_profile_gl_frame_us[i];
	}
}

static void android_profile_append_bucket_avg_fields(long long frame_count)
{
	int i;

	for (i = 0; i < ANDROID_PROFILE_BUCKET_COUNT; i++) {
		const long long avg_us =
		    frame_count ? g_android_profile_buckets[i].sample_total_us / frame_count : 0;
		android_profile_appendf(
		    "prof_v=1 type=bucket_avg sample=%u game=%s bucket=%s avg_us=%lld total_us=%lld max_us=%lld",
		    g_android_profile_sample_id,
		    g_android_profile_game,
		    g_android_profile_bucket_names[i],
		    avg_us,
		    g_android_profile_buckets[i].sample_total_us,
		    g_android_profile_buckets[i].sample_max_us);
	}
}

static void android_profile_append_gl_avg_fields(long long frame_count)
{
	int i;

	for (i = 0; i < ANDROID_PROFILE_GL_COUNT; i++) {
		const long long avg_us = frame_count ? g_android_profile_gl_sample_total_us[i] / frame_count : 0;
		android_profile_appendf(
		    "prof_v=1 type=gl_avg sample=%u game=%s bucket=%s avg_us=%lld total_us=%lld max_us=%lld",
		    g_android_profile_sample_id,
		    g_android_profile_game,
		    g_android_profile_gl_metric_names[i],
		    avg_us,
		    g_android_profile_gl_sample_total_us[i],
		    g_android_profile_gl_sample_max_us[i]);
	}
}

static void android_profile_finish_sample(long long now_ms)
{
	const long long avg_us =
	    g_android_profile_sample_frame_count ? g_android_profile_sample_total_us / (long long) g_android_profile_sample_frame_count : 0;

	if (!g_android_profile_sample_active)
		return;

	if (g_android_profile_texture_burst.active)
		android_profile_finish_texture_burst("sample_end");

	android_profile_appendf(
	    "prof_v=1 type=summary sample=%u game=%s frames=%u avg_frame_us=%lld max_frame_us=%lld wall_ms=%lld mono_ms=%lld",
	    g_android_profile_sample_id,
	    g_android_profile_game,
	    g_android_profile_sample_frame_count,
	    avg_us,
	    g_android_profile_sample_max_us,
	    android_profile_wall_ms(),
	    now_ms);
	android_profile_append_bucket_avg_fields(g_android_profile_sample_frame_count);
	android_profile_append_gl_avg_fields(g_android_profile_sample_frame_count);
	android_profile_flush_batch();
	g_android_profile_sample_active = 0;
	g_android_profile_frame_active = 0;
	g_android_profile_next_sample_ms = g_android_profile_sample_start_ms + ANDROID_PROFILE_PERIOD_MS;
	g_android_profile_sample_frame_count = 0;
	g_android_profile_sample_total_us = 0;
	g_android_profile_sample_max_us = 0;
	android_profile_reset_sample_metrics();
	android_profile_reset_frame_metrics();
}

static void android_profile_start_sample(long long now_ms, const char *game)
{
	if (g_android_profile_texture_burst.active) {
		android_profile_finish_texture_burst("sample_start");
		android_profile_flush_batch();
	}

	g_android_profile_sample_active = 1;
	g_android_profile_sample_id++;
	g_android_profile_sample_start_ms = now_ms;
	g_android_profile_sample_end_ms = now_ms + ANDROID_PROFILE_WINDOW_MS;
	g_android_profile_sample_frame_count = 0;
	g_android_profile_sample_total_us = 0;
	g_android_profile_sample_max_us = 0;
	android_profile_reset_sample_metrics();
	android_profile_reset_frame_metrics();
	android_profile_copy_game(game);
	android_profile_reset_batch();
	android_profile_appendf(
	    "prof_v=1 type=sample_start sample=%u game=%s period_ms=%lld window_ms=%lld wall_ms=%lld mono_ms=%lld",
	    g_android_profile_sample_id,
	    g_android_profile_game,
	    ANDROID_PROFILE_PERIOD_MS,
	    ANDROID_PROFILE_WINDOW_MS,
	    android_profile_wall_ms(),
	    now_ms);
}

void android_profile_frame_begin(const char *game, unsigned int frame_id)
{
	const long long now_ms = android_profile_now_ms();
	const long long now_us = android_profile_now_us();

	android_profile_maybe_finish_texture_burst(now_us, NULL);

	if (!debug_log_enabled[DLOG_PROFILING]) {
		android_profile_maybe_finish_texture_burst(now_us, "disabled");
		if (g_android_profile_sample_active)
			android_profile_finish_sample(now_ms);
		return;
	}

	if (g_android_profile_sample_active && now_ms >= g_android_profile_sample_end_ms)
		android_profile_finish_sample(now_ms);

	if (!g_android_profile_sample_active) {
		if (!g_android_profile_next_sample_ms || now_ms >= g_android_profile_next_sample_ms)
			android_profile_start_sample(now_ms, game);
	}

	if (!g_android_profile_sample_active) {
		g_android_profile_frame_active = 0;
		return;
	}

	g_android_profile_frame_active = 1;
	g_android_profile_frame_id = frame_id;
	android_profile_reset_frame_metrics();
	g_android_profile_frame_start_us = android_profile_now_us();
}

void android_profile_bucket_begin(int bucket)
{
	if (!g_android_profile_frame_active)
		return;
	if (bucket < 0 || bucket >= ANDROID_PROFILE_BUCKET_COUNT)
		return;
	if (g_android_profile_buckets[bucket].active)
		return;

	g_android_profile_buckets[bucket].active = 1;
	g_android_profile_buckets[bucket].start_us = android_profile_now_us();
}

void android_profile_bucket_end(int bucket)
{
	const long long now_us = android_profile_now_us();

	if (!g_android_profile_frame_active)
		return;
	if (bucket < 0 || bucket >= ANDROID_PROFILE_BUCKET_COUNT)
		return;
	if (!g_android_profile_buckets[bucket].active)
		return;

	g_android_profile_buckets[bucket].frame_us += now_us - g_android_profile_buckets[bucket].start_us;
	g_android_profile_buckets[bucket].active = 0;
	g_android_profile_buckets[bucket].start_us = 0;
}

void android_profile_set_gl_frame_metrics(int swap_us, int gpu_us,
                                          int resolve_us, int gl_error_us)
{
	if (!g_android_profile_frame_active)
		return;

	g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_SWAP] = swap_us > 0 ? swap_us : 0;
	g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_GPU] = gpu_us > 0 ? gpu_us : 0;
	g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_RESOLVE] = resolve_us > 0 ? resolve_us : 0;
	g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_ERROR] = gl_error_us > 0 ? gl_error_us : 0;
}

void android_profile_texture_load(const char *game, const char *name,
                                  const char *source, int width, int height,
                                  int flags, long long total_us,
                                  long long ktx2_read_us,
                                  long long png_read_us,
                                  long long upload_us,
						  long long mask_us,
						  const struct android_profile_texture_lookup_metrics *lookup)
{
	const char *game_name = (game && game[0]) ? game : g_android_profile_game;
	const char *texture_name = (name && name[0]) ? name : "unknown";
	const char *source_name = (source && source[0]) ? source : "unknown";
	const char *ktx2_hit_name = android_profile_texture_lookup_slot_name(ANDROID_PROFILE_TEXTURE_LOOKUP_NONE);
	const char *png_hit_name = android_profile_texture_lookup_slot_name(ANDROID_PROFILE_TEXTURE_LOOKUP_NONE);
	const char *png_hit_ext_name = android_profile_texture_lookup_ext_name(ANDROID_PROFILE_TEXTURE_LOOKUP_NONE);
	unsigned int ktx2_attempts = 0;
	unsigned int png_attempts = 0;
	long long ktx2_set_us = 0;
	long long ktx2_pref_us = 0;
	long long ktx2_base_us = 0;
	long long png_set_us = 0;
	long long png_pref_us = 0;
	long long png_base_us = 0;
	long long png_png_us = 0;
	long long png_jpg_us = 0;
	long long png_tga_us = 0;

	if (!debug_log_enabled[DLOG_PROFILING])
		return;
	if (!game_name || !game_name[0])
		game_name = "unknown";
	if (lookup) {
		ktx2_hit_name = android_profile_texture_lookup_slot_name(lookup->ktx2_hit_slot);
		png_hit_name = android_profile_texture_lookup_slot_name(lookup->png_hit_slot);
		png_hit_ext_name = android_profile_texture_lookup_ext_name(lookup->png_hit_ext);
		ktx2_attempts = lookup->ktx2_attempts;
		png_attempts = lookup->png_attempts;
		ktx2_set_us = lookup->ktx2_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_SET];
		ktx2_pref_us = lookup->ktx2_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_PREFIX];
		ktx2_base_us = lookup->ktx2_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_BASE];
		png_set_us = lookup->png_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_SET];
		png_pref_us = lookup->png_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_PREFIX];
		png_base_us = lookup->png_slot_us[ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_BASE];
		png_png_us = lookup->png_ext_us[ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_PNG];
		png_jpg_us = lookup->png_ext_us[ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_JPG];
		png_tga_us = lookup->png_ext_us[ANDROID_PROFILE_TEXTURE_LOOKUP_EXT_TGA];
	}

	android_profile_note_texture_burst(game_name, texture_name, source_name,
	                                   total_us, ktx2_read_us, png_read_us,
	                                   upload_us, mask_us, lookup);

	if (total_us < ANDROID_PROFILE_TEXTURE_THRESHOLD_US)
		return;

	android_profile_appendf(
	    "prof_v=1 type=texture game=%s sample=%u name=%s source=%s w=%d h=%d flags=0x%x total_us=%lld ktx2_read_us=%lld png_read_us=%lld upload_us=%lld mask_us=%lld ktx2_attempts=%u ktx2_hit=%s ktx2_set_us=%lld ktx2_pref_us=%lld ktx2_base_us=%lld png_attempts=%u png_hit=%s png_hit_ext=%s png_set_us=%lld png_pref_us=%lld png_base_us=%lld png_png_us=%lld png_jpg_us=%lld png_tga_us=%lld",
	    game_name,
	    g_android_profile_sample_active ? g_android_profile_sample_id : 0,
	    texture_name,
	    source_name,
	    width,
	    height,
	    flags,
	    total_us,
	    ktx2_read_us,
	    png_read_us,
	    upload_us,
	    mask_us,
	    ktx2_attempts,
	    ktx2_hit_name,
	    ktx2_set_us,
	    ktx2_pref_us,
	    ktx2_base_us,
	    png_attempts,
	    png_hit_name,
	    png_hit_ext_name,
	    png_set_us,
	    png_pref_us,
	    png_base_us,
	    png_png_us,
	    png_jpg_us,
	    png_tga_us);

	if (!g_android_profile_sample_active)
		android_profile_flush_batch();
}

void android_profile_storage_op(const char *name, const char *op,
                                unsigned long long offset,
                                unsigned long long size,
                                long long total_us)
{
	const char *entry_name = (name && name[0]) ? name : "unknown";
	const char *op_name = (op && op[0]) ? op : "unknown";

	if (!debug_log_enabled[DLOG_PROFILING])
		return;
	if (total_us < ANDROID_PROFILE_STORAGE_THRESHOLD_US)
		return;

	android_profile_appendf(
	    "prof_v=1 type=storage sample=%u name=%s op=%s offset=%llu size=%llu total_us=%lld",
	    g_android_profile_sample_active ? g_android_profile_sample_id : 0,
	    entry_name,
	    op_name,
	    offset,
	    size,
	    total_us);

	if (!g_android_profile_sample_active)
		android_profile_flush_batch();
}

void android_profile_frame_end(void)
{
	const long long now_us = android_profile_now_us();
	const long long now_ms = now_us / 1000LL;
	const long long total_us = now_us - g_android_profile_frame_start_us;

	if (!g_android_profile_frame_active)
		return;

	android_profile_finish_open_buckets(now_us);
	g_android_profile_frame_active = 0;
	g_android_profile_sample_frame_count++;
	g_android_profile_sample_total_us += total_us;
	if (total_us > g_android_profile_sample_max_us)
		g_android_profile_sample_max_us = total_us;
	android_profile_commit_frame_metrics();
	android_profile_appendf(
	    "prof_v=1 type=frame sample=%u game=%s frame=%u frame_index=%u total_us=%lld wait_us=%lld sim_us=%lld render_us=%lld replay_us=%lld swap_us=%lld gpu_us=%lld resolve_us=%lld glerr_us=%lld",
	    g_android_profile_sample_id,
	    g_android_profile_game,
	    g_android_profile_frame_id,
	    g_android_profile_sample_frame_count,
	    total_us,
	    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_WAIT].frame_us,
	    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_SIM].frame_us,
	    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_RENDER].frame_us,
	    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_REPLAY].frame_us,
	    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_SWAP],
	    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_GPU],
	    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_RESOLVE],
	    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_ERROR]);

	if (now_ms >= g_android_profile_sample_end_ms)
		android_profile_finish_sample(now_ms);
}

void android_profile_flush(void)
{
	android_profile_maybe_finish_texture_burst(android_profile_now_us(), "flush");

	if (g_android_profile_sample_active)
		android_profile_finish_sample(android_profile_now_ms());
	else
		android_profile_flush_batch();
}

#endif /* defined(ANDROID) || defined(__ANDROID__) */