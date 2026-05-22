#if defined(ANDROID) || defined(__ANDROID__)

#include "android_profile.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "android_log.h"

#define ANDROID_PROFILE_PERIOD_MS 10000LL
#define ANDROID_PROFILE_WINDOW_MS 1000LL
#define ANDROID_PROFILE_BATCH_CAPACITY 65536

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
static size_t g_android_profile_batch_len;
static char g_android_profile_game[8] = "";
static char g_android_profile_batch[ANDROID_PROFILE_BATCH_CAPACITY];

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
	if (!game || !game[0]) {
		snprintf(g_android_profile_game, sizeof(g_android_profile_game), "unknown");
		return;
	}

	snprintf(g_android_profile_game, sizeof(g_android_profile_game), "%s", game);
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
	char line[256];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);

	android_profile_append_line(line);
}

static void android_profile_finish_sample(long long now_ms)
{
	const long long avg_us =
		g_android_profile_sample_frame_count ?
		g_android_profile_sample_total_us / (long long) g_android_profile_sample_frame_count : 0;

	if (!g_android_profile_sample_active)
		return;

	android_profile_appendf(
		"prof_v=1 type=summary sample=%u game=%s frames=%u avg_frame_us=%lld max_frame_us=%lld wall_ms=%lld mono_ms=%lld",
		g_android_profile_sample_id,
		g_android_profile_game,
		g_android_profile_sample_frame_count,
		avg_us,
		g_android_profile_sample_max_us,
		android_profile_wall_ms(),
		now_ms);
	android_profile_flush_batch();
	g_android_profile_sample_active = 0;
	g_android_profile_frame_active = 0;
	g_android_profile_next_sample_ms = g_android_profile_sample_start_ms + ANDROID_PROFILE_PERIOD_MS;
	g_android_profile_sample_frame_count = 0;
	g_android_profile_sample_total_us = 0;
	g_android_profile_sample_max_us = 0;
}

static void android_profile_start_sample(long long now_ms, const char *game)
{
	g_android_profile_sample_active = 1;
	g_android_profile_sample_id++;
	g_android_profile_sample_start_ms = now_ms;
	g_android_profile_sample_end_ms = now_ms + ANDROID_PROFILE_WINDOW_MS;
	g_android_profile_sample_frame_count = 0;
	g_android_profile_sample_total_us = 0;
	g_android_profile_sample_max_us = 0;
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

	if (!debug_log_enabled[DLOG_PROFILING]) {
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
	g_android_profile_frame_start_us = android_profile_now_us();
}

void android_profile_frame_end(void)
{
	const long long now_us = android_profile_now_us();
	const long long now_ms = now_us / 1000LL;
	const long long total_us = now_us - g_android_profile_frame_start_us;

	if (!g_android_profile_frame_active)
		return;

	g_android_profile_frame_active = 0;
	g_android_profile_sample_frame_count++;
	g_android_profile_sample_total_us += total_us;
	if (total_us > g_android_profile_sample_max_us)
		g_android_profile_sample_max_us = total_us;
	android_profile_appendf(
		"prof_v=1 type=frame sample=%u game=%s frame=%u frame_index=%u total_us=%lld",
		g_android_profile_sample_id,
		g_android_profile_game,
		g_android_profile_frame_id,
		g_android_profile_sample_frame_count,
		total_us);

	if (now_ms >= g_android_profile_sample_end_ms)
		android_profile_finish_sample(now_ms);
}

void android_profile_flush(void)
{
	if (g_android_profile_sample_active)
		android_profile_finish_sample(android_profile_now_ms());
	else
		android_profile_flush_batch();
}

#endif /* defined(ANDROID) || defined(__ANDROID__) */