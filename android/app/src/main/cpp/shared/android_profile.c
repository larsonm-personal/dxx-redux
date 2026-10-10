#if defined(ANDROID) || defined(__ANDROID__)

#include "android_profile.h"

#include <limits.h>
#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "android_log.h"
#include "android_slowdown_detector.h"
#include "game.h"
#include "object.h"
#include "player.h"

void android_profile_scene_state(void)
{
	int active_objects = 0;
	int projectile_objects = 0;
	int reactor_objects = 0;
	int i;

	for (i = 0; i <= Highest_object_index; i++) {
		object *obj = &Objects[i];
		int owner;

		if (obj->type == OBJ_NONE)
			continue;
		active_objects++;
		if (obj->type == OBJ_WEAPON)
			projectile_objects++;
		else if (obj->type == OBJ_CNTRLCEN)
			reactor_objects++;
		if (obj->type != OBJ_ROBOT)
			continue;
		owner = obj->ctype.ai_info.REMOTE_OWNER;
		android_profile_remote_robot_live(
		    i, obj->signature,
		    (Game_mode & GM_MULTI) && owner >= 0 && owner != Player_num);
	}
	android_profile_set_scene_object_counts(
	    active_objects, projectile_objects, reactor_objects);
}

extern int r_tpolyc;
extern int r_water_faces;
extern int r_texbinds;
extern int r_texbind_reuse;
extern int r_shader_switches;
extern int r_mask_draws;
extern int r_mwall_cache_hits;
extern int r_mwall_cache_misses;

#define ANDROID_PROFILE_PERIOD_MS             10000LL
#define ANDROID_PROFILE_WINDOW_MS             1000LL
#define ANDROID_PROFILE_BATCH_CAPACITY        65536
#define ANDROID_PROFILE_TEXTURE_THRESHOLD_US  10000LL
#define ANDROID_PROFILE_TEXTURE_BURST_GAP_US  250000LL
#define ANDROID_PROFILE_STORAGE_THRESHOLD_US  2000LL
#define ANDROID_PROFILE_SLOW_FRAME_US         100000LL
#define ANDROID_PROFILE_SLOW_LOG_INTERVAL_US  1000000LL
#define ANDROID_FLIGHT_BATCH_CAPACITY         65536
#define ANDROID_FLIGHT_CAPTURE_MAX_BYTES      (256 * 1024)
#define ANDROID_FLIGHT_HISTORY_US             5000000LL
#define ANDROID_PROFILE_REMOTE_ROBOT_CAPACITY 2048
#define ANDROID_PROFILE_REMOTE_STALE_MS       250

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

struct android_profile_remote_robot_state {
	long long last_update_us;
	unsigned int live_generation;
	int signature;
	int remote_owned;
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
static long long g_android_profile_last_frame_begin_us;
static long long g_android_profile_last_flip_us;
static long long g_android_profile_latest_flip_gap_us;
static long long g_android_profile_frame_begin_gap_us;
static long long g_android_profile_frame_flip_gap_us;
static long long g_android_profile_next_slow_log_us;
static long long g_android_profile_sample_total_us;
static long long g_android_profile_sample_max_us;
static long long g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_COUNT];
static long long g_android_profile_gl_sample_total_us[ANDROID_PROFILE_GL_COUNT];
static long long g_android_profile_gl_sample_max_us[ANDROID_PROFILE_GL_COUNT];
static size_t g_android_profile_batch_len;
static char g_android_profile_game[8] = "";
static char g_android_profile_batch[ANDROID_PROFILE_BATCH_CAPACITY];
static int g_android_profile_level;
static int g_android_profile_viewer_segment;
static int g_android_profile_object_draws;
static long long g_android_profile_object_total_us;
static long long g_android_profile_object_max_us;
static int g_android_profile_object_max_objnum;
static int g_android_profile_object_max_type;
static int g_android_profile_object_max_id;
static int g_android_profile_object_max_render_type;
static int g_android_profile_object_max_model;
static unsigned int g_android_profile_simulation_frame_id;
static int g_android_profile_frame_time_us;
static long long g_android_profile_network_us;
static struct android_network_frame g_android_profile_network_detail;
static int g_android_profile_network_packets;
static int g_android_profile_network_bytes;
static int g_android_profile_remote_robot_updates;
static int g_android_profile_active_object_count;
static int g_android_profile_projectile_object_count;
static int g_android_profile_reactor_object_count;
static unsigned int g_android_profile_remote_live_generation;
static int g_android_profile_remote_level = -32768;
static struct android_profile_remote_robot_state
    g_android_profile_remote_robots[ANDROID_PROFILE_REMOTE_ROBOT_CAPACITY];
static struct android_profile_bucket_state g_android_profile_buckets[ANDROID_PROFILE_BUCKET_COUNT];
static struct android_profile_texture_burst_state g_android_profile_texture_burst;
static struct android_slowdown_detector g_android_slowdown_detector;
static struct android_stutter_detector g_android_stutter_detector;
static volatile int g_android_slowdown_capture_requested;
static volatile int g_android_profile_resume_pending;
static int g_android_profile_max_fps;
static int g_android_profile_vsync;
static int g_android_profile_object_detail_active;
static char g_android_flight_batch[ANDROID_FLIGHT_BATCH_CAPACITY];
static size_t g_android_flight_batch_len;
static size_t g_android_flight_capture_bytes;
static unsigned int g_android_flight_dropped_lines;

static const char *g_android_profile_bucket_names[ANDROID_PROFILE_BUCKET_COUNT] = {
	"wait",
	"sim",
	"render",
	"replay",
	"record",
	"multi",
	"move",
	"ai",
	"sound",
	"effects",
	"rewind",
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

void android_profile_texture_lookup_note_ktx2(
    struct android_profile_texture_lookup_metrics *lookup, int slot,
    long long elapsed_us, int loaded)
{
	if (!lookup)
		return;

	lookup->ktx2_attempts++;
	if (slot >= 0 && slot < ANDROID_PROFILE_TEXTURE_LOOKUP_SLOT_COUNT)
		lookup->ktx2_slot_us[slot] += elapsed_us;
	if (loaded)
		lookup->ktx2_hit_slot = slot;
}

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

long long android_profile_monotonic_us(void)
{
	return android_profile_now_us();
}

long long android_profile_take_elapsed_us(long long *last_us)
{
	const long long now_us = android_profile_now_us();
	const long long elapsed_us = now_us - *last_us;
	*last_us = now_us;
	return elapsed_us;
}

void android_profile_log_level_load(
    const char *game, int level, const char *file, int game_mode,
    const struct android_profile_level_load_metrics *metrics)
{
	debug_log_force(
	    DLOG_PROFILING,
	    "loadprof_v=1 type=level_load game=%s level=%d file='%s' mode=0x%x total_us=%lld file_us=%lld presentation_us=%lld endlevel_us=%lld replacements_us=%lld robots_us=%lld textures_us=%lld network_us=%lld sound_us=%lld music_us=%lld finish_us=%lld",
	    game, level, file ? file : "", game_mode, metrics->total_us,
	    metrics->file_us, metrics->presentation_us, metrics->endlevel_us,
	    metrics->replacements_us, metrics->robots_us, metrics->textures_us,
	    metrics->network_us, metrics->sound_us, metrics->music_us,
	    metrics->finish_us);
}

void android_profile_log_level_init(
    const char *game, int requested_level, int current_level, int game_mode,
    const struct android_profile_level_init_metrics *metrics)
{
	debug_log_force(
	    DLOG_PROFILING,
	    "loadprof_v=1 type=level_init game=%s requested=%d current=%d mode=0x%x total_us=%lld pre_load_us=%lld load_us=%lld network_sync_us=%lld post_load_us=%lld",
	    game, requested_level, current_level, game_mode, metrics->total_us,
	    metrics->pre_load_us, metrics->load_us, metrics->network_sync_us,
	    metrics->post_load_us);
}

void android_profile_log_restore(
    const char *game, int level, const char *file, int game_mode,
    int had_game_window, const struct android_profile_restore_metrics *metrics)
{
	debug_log_force(
	    DLOG_PROFILING,
	    "loadprof_v=1 type=restore game=%s level=%d file='%s' mode=0x%x had_game_window=%d total_us=%lld pre_level_us=%lld level_init_us=%lld state_data_us=%lld finalize_us=%lld",
	    game, level, file ? file : "", game_mode, had_game_window,
	    metrics->total_us, metrics->pre_level_us, metrics->level_init_us,
	    metrics->state_data_us, metrics->finalize_us);
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

static void android_flight_reset_batch(void)
{
	g_android_flight_batch_len = 0;
	g_android_flight_batch[0] = '\0';
}

static void android_flight_flush_batch(void)
{
	if (!g_android_flight_batch_len)
		return;

	debug_log_batch_force(DLOG_PROFILING, g_android_flight_batch);
	android_flight_reset_batch();
}

static void android_flight_append_line(const char *line)
{
	const size_t line_len = strlen(line);

	if (!line_len || line_len + 2 > sizeof(g_android_flight_batch))
		return;
	if (g_android_flight_capture_bytes + line_len + 2 > ANDROID_FLIGHT_CAPTURE_MAX_BYTES - 1024) {
		g_android_flight_dropped_lines++;
		return;
	}
	if (g_android_flight_batch_len + line_len + 2 > sizeof(g_android_flight_batch))
		android_flight_flush_batch();

	memcpy(g_android_flight_batch + g_android_flight_batch_len, line, line_len);
	g_android_flight_batch_len += line_len;
	g_android_flight_batch[g_android_flight_batch_len++] = '\n';
	g_android_flight_batch[g_android_flight_batch_len] = '\0';
	g_android_flight_capture_bytes += line_len + 2;
}

static void android_flight_appendf(const char *fmt, ...)
{
	char line[2048];
	va_list ap;

	va_start(ap, fmt);
	vsnprintf(line, sizeof(line), fmt, ap);
	va_end(ap);
	android_flight_append_line(line);
}

static long long android_flight_nonwait_us(const struct android_slowdown_frame *frame)
{
	const long long value = (long long) frame->total_us - frame->wait_us;
	return value > 0 ? value : 0;
}

static int android_profile_i32_duration(long long value)
{
	if (value > INT_MAX)
		return INT_MAX;
	if (value < 0)
		return 0;
	return (int) value;
}

static void android_stutter_format_frame(char *line, size_t capacity, const char *role,
                                         const struct android_stutter_frame *sample)
{
	const struct android_slowdown_frame *frame = &sample->frame;
	const long long other_us = (long long) frame->total_us - frame->wait_us -
	                           frame->sim_us - frame->render_us - frame->replay_us - sample->rewind_us;
	/* Subsystem fields overlap their parent sim/render bucket; GPU is asynchronous */
	snprintf(line, capacity,
	         "stutter_v=1 type=frame role=%s game=%s frame=%u mono_us=%lld level=%d seg=%d mode=0x%x sim_frame=%u total_us=%d wait_us=%d sim_us=%d render_us=%d replay_us=%d rewind_us=%d other_us=%lld multi_us=%d move_us=%d ai_us=%d sound_us=%d effects_us=%d record_us=%d net_us=%d net_packets=%d net_bytes=%d latest_swap_us=%d latest_gpu_us=%d latest_resolve_us=%d latest_glerr_us=%d latest_flip_gap_us=%d objects=%d projectiles=%d robots_local=%d robots_remote=%d robots_stale=%d max_robot_age_ms=%d tpolys=%d texbinds=%d max_fps=%d vsync=%d\n",
	         role, g_android_profile_game, frame->frame_id, (long long) frame->end_us,
	         frame->level, frame->viewer_segment, (unsigned int) sample->mode,
	         frame->simulation_frame_id, frame->total_us, frame->wait_us, frame->sim_us,
	         frame->render_us, frame->replay_us, sample->rewind_us, other_us,
	         sample->multi_us, sample->move_us, sample->ai_us, sample->sound_us,
	         sample->effects_us, frame->record_us, frame->network_us,
	         frame->network_packets, frame->network_bytes, frame->swap_us, frame->gpu_us,
	         frame->resolve_us, frame->gl_error_us, frame->flip_gap_us,
	         frame->active_object_count, frame->projectile_object_count,
	         frame->local_robot_count, frame->remote_robot_count,
	         frame->stale_remote_robot_count, frame->max_remote_robot_age_ms,
	         frame->textured_polys, frame->texture_binds, frame->max_fps, frame->vsync);
	{
		static const char *names[ANDROID_OUTER_COUNT] = {
			"unattributed", "profile", "present", "readback", "swap", "introspect", "lifecycle", "automation", "input", "dispatch"
		};
		int i;
		for (i = 0; i < ANDROID_OUTER_COUNT; ++i) {
			size_t used = strlen(line);
			if (used >= capacity - 1) break;
			snprintf(line + used, capacity - used,
			         "stutter_v=1 type=outer role=%s frame=%u stage=%s wall_us=%lld cpu_us=%lld\n",
			         role, frame->frame_id, names[i], (long long) sample->outer_us[i], (long long) sample->outer_cpu_us[i]);
		}
	}
}

static void android_stutter_log_window(void)
{
	const struct android_stutter_window *window = &g_android_stutter_detector.completed;
	const long long outside_us = window->worst.frame.end_us -
	                             window->before_worst.frame.end_us - window->worst.frame.total_us;
	char line[16384];
	snprintf(line, sizeof(line),
	         "stutter_v=1 type=window game=%s start_us=%lld end_us=%lld frames=%d hitches=%d over_100ms=%d over_250ms=%d avg_interval_us=%lld max_interval_us=%d baseline_us=%d threshold_us=%d excess_us=%lld worst_frame=%u outside_us=%lld\n",
	         g_android_profile_game, (long long) window->start_us, (long long) window->end_us,
	         window->frames, window->hitches, window->over_100ms, window->over_250ms,
	         (long long) (window->interval_total_us / window->frames), window->max_interval_us,
	         window->baseline_us, window->threshold_us, (long long) window->excess_us,
	         window->worst.frame.frame_id, outside_us > 0 ? outside_us : 0);
	if (window->hitches) {
		size_t used = strlen(line);
		android_stutter_format_frame(line + used, sizeof(line) - used,
		                             "before", &window->before_worst);
		used = strlen(line);
		android_stutter_format_frame(line + used, sizeof(line) - used,
		                             "worst", &window->worst);
		android_network_profile_format(line, sizeof(line), "before", window->before_worst.frame.frame_id, &window->before_worst.network);
		android_network_profile_format(line, sizeof(line), "worst", window->worst.frame.frame_id, &window->worst.network);
	}
	debug_log_batch_force(DLOG_PROFILING, line);
}

static void android_flight_append_frame(const char *type,
                                        const struct android_slowdown_frame *frame)
{
	android_flight_appendf(
	    "prof_v=3 type=%s capture=%u frame=%u mono_us=%lld level=%d viewer_seg=%d begin_gap_us=%d flip_gap_us=%d sim_frame=%u frame_time_us=%d total_us=%d nonwait_us=%lld wait_us=%d sim_us=%d record_us=%d render_us=%d replay_us=%d swap_us=%d gpu_us=%d resolve_us=%d glerr_us=%d net_us=%d net_packets=%d net_bytes=%d remote_updates=%d robots_local=%d robots_remote=%d robots_stale=%d robots_age_unknown=%d max_robot_age_ms=%d objects_active=%d projectiles=%d reactors=%d tpolys=%d water_faces=%d texbinds=%d texreuse=%d shader_switches=%d mask_draws=%d mwall_hits=%d mwall_misses=%d object_draws=%d max_object_us=%d max_obj=%d max_type=%d max_id=%d max_render=%d max_model=%d max_fps=%d vsync=%d",
	    type,
	    g_android_slowdown_detector.capture_id,
	    frame->frame_id,
	    (long long) frame->end_us,
	    frame->level,
	    frame->viewer_segment,
	    frame->begin_gap_us,
	    frame->flip_gap_us,
	    frame->simulation_frame_id,
	    frame->frame_time_us,
	    frame->total_us,
	    android_flight_nonwait_us(frame),
	    frame->wait_us,
	    frame->sim_us,
	    frame->record_us,
	    frame->render_us,
	    frame->replay_us,
	    frame->swap_us,
	    frame->gpu_us,
	    frame->resolve_us,
	    frame->gl_error_us,
	    frame->network_us,
	    frame->network_packets,
	    frame->network_bytes,
	    frame->remote_robot_updates,
	    frame->local_robot_count,
	    frame->remote_robot_count,
	    frame->stale_remote_robot_count,
	    frame->unknown_remote_robot_age,
	    frame->max_remote_robot_age_ms,
	    frame->active_object_count,
	    frame->projectile_object_count,
	    frame->reactor_object_count,
	    frame->textured_polys,
	    frame->water_faces,
	    frame->texture_binds,
	    frame->texture_reuses,
	    frame->shader_switches,
	    frame->mask_draws,
	    frame->merged_wall_hits,
	    frame->merged_wall_misses,
	    frame->object_draws,
	    frame->max_object_us,
	    frame->max_object_num,
	    frame->max_object_type,
	    frame->max_object_id,
	    frame->max_object_render_type,
	    frame->max_object_model,
	    frame->max_fps,
	    frame->vsync);
}

static void android_flight_append_window(const char *type,
                                         const struct android_slowdown_window *window)
{
	int i;
	const long long span_us = window->end_us - window->start_us;
	const long long avg_total_us = window->frames ? window->total_us / window->frames : 0;
	const long long avg_nonwait_us = window->frames ? window->nonwait_us / window->frames : 0;

	android_flight_appendf(
	    "prof_v=3 type=%s capture=%u start_us=%lld end_us=%lld span_us=%lld frames=%d fps_milli=%d expected_fps_milli=%d avg_total_us=%lld avg_nonwait_us=%lld max_nonwait_us=%d max_begin_gap_us=%d max_flip_gap_us=%d net_us=%lld max_net_us=%d net_packets=%d net_bytes=%lld remote_updates=%d max_robot_age_ms=%d",
	    type,
	    g_android_slowdown_detector.capture_id,
	    (long long) window->start_us,
	    (long long) window->end_us,
	    span_us,
	    window->frames,
	    window->fps_milli,
	    window->expected_fps_milli,
	    avg_total_us,
	    avg_nonwait_us,
	    window->max_nonwait_us,
	    window->max_begin_gap_us,
	    window->max_flip_gap_us,
	    (long long) window->network_us,
	    window->max_network_us,
	    window->network_packets,
	    (long long) window->network_bytes,
	    window->remote_robot_updates,
	    window->max_remote_robot_age_ms);
	for (i = 0; i < ANDROID_SLOWDOWN_WORST_COUNT; i++) {
		if (window->worst[i].frame_id)
			android_flight_append_frame("worst_frame", &window->worst[i]);
	}
}

static void android_flight_append_history(void)
{
	struct android_slowdown_window bin;
	const int count = android_slowdown_detector_ring_count(&g_android_slowdown_detector);
	const struct android_slowdown_frame *last =
	    android_slowdown_detector_ring_get(&g_android_slowdown_detector, count - 1);
	const long long cutoff_us = last ? last->end_us - ANDROID_FLIGHT_HISTORY_US : 0;
	int i;

	memset(&bin, 0, sizeof(bin));
	for (i = 0; i < count; i++) {
		const struct android_slowdown_frame *frame =
		    android_slowdown_detector_ring_get(&g_android_slowdown_detector, i);
		const int nonwait_us = frame ? (int) android_flight_nonwait_us(frame) : 0;
		if (!frame || frame->end_us < cutoff_us)
			continue;
		if (!bin.start_us)
			bin.start_us = frame->end_us;
		if (frame->end_us - bin.start_us >= 100000 && bin.frames) {
			bin.end_us = frame->end_us;
			bin.fps_milli = (int) ((long long) bin.frames * 1000000000LL /
			                       (bin.end_us - bin.start_us));
			android_flight_append_window("history", &bin);
			memset(&bin, 0, sizeof(bin));
			bin.start_us = frame->end_us;
		}
		bin.frames++;
		bin.total_us += frame->total_us;
		bin.nonwait_us += nonwait_us;
		if (nonwait_us > bin.max_nonwait_us)
			bin.max_nonwait_us = nonwait_us;
		if (frame->begin_gap_us > bin.max_begin_gap_us)
			bin.max_begin_gap_us = frame->begin_gap_us;
		if (frame->flip_gap_us > bin.max_flip_gap_us)
			bin.max_flip_gap_us = frame->flip_gap_us;
		bin.network_us += frame->network_us;
		bin.network_packets += frame->network_packets;
		bin.network_bytes += frame->network_bytes;
		bin.remote_robot_updates += frame->remote_robot_updates;
		if (frame->network_us > bin.max_network_us)
			bin.max_network_us = frame->network_us;
		if (frame->max_remote_robot_age_ms > bin.max_remote_robot_age_ms)
			bin.max_remote_robot_age_ms = frame->max_remote_robot_age_ms;
	}
	if (bin.frames) {
		bin.end_us = last ? last->end_us : bin.start_us;
		if (bin.end_us > bin.start_us)
			bin.fps_milli = (int) ((long long) bin.frames * 1000000000LL /
			                       (bin.end_us - bin.start_us));
		android_flight_append_window("history", &bin);
	}
}

static void android_flight_start_capture(const struct android_slowdown_frame *frame)
{
	g_android_flight_capture_bytes = 0;
	g_android_flight_dropped_lines = 0;
	android_flight_reset_batch();
	android_flight_appendf(
	    "prof_v=3 type=capture_start capture=%u game=%s reason=%s wall_ms=%lld mono_us=%lld level=%d viewer_seg=%d max_fps=%d vsync=%d expected_fps_milli=%d observed_fps_milli=%d duration_ms=60000 history_ms=5000 max_bytes=%d manual_profiling=%d",
	    g_android_slowdown_detector.capture_id,
	    g_android_profile_game,
	    g_android_slowdown_detector.trigger_severe ? "severe" : "sustained",
	    android_profile_wall_ms(),
	    (long long) frame->end_us,
	    frame->level,
	    frame->viewer_segment,
	    frame->max_fps,
	    frame->vsync,
	    g_android_slowdown_detector.completed_window.expected_fps_milli,
	    g_android_slowdown_detector.completed_window.fps_milli,
	    ANDROID_FLIGHT_CAPTURE_MAX_BYTES,
	    debug_log_enabled[DLOG_PROFILING] ? 1 : 0);
	android_flight_append_history();
	android_flight_flush_batch();
}

static void android_flight_end_capture(const struct android_slowdown_frame *frame)
{
	android_flight_appendf(
	    "prof_v=3 type=capture_end capture=%u game=%s mono_us=%lld frame=%u bytes=%u dropped_lines=%u cooldown_ms=300000",
	    g_android_slowdown_detector.capture_id,
	    g_android_profile_game,
	    (long long) frame->end_us,
	    frame->frame_id,
	    (unsigned int) g_android_flight_capture_bytes,
	    g_android_flight_dropped_lines);
	android_flight_flush_batch();
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

	g_android_profile_level = 0;
	g_android_profile_viewer_segment = -1;
	g_android_profile_object_draws = 0;
	g_android_profile_object_total_us = 0;
	g_android_profile_object_max_us = 0;
	g_android_profile_object_max_objnum = -1;
	g_android_profile_object_max_type = -1;
	g_android_profile_object_max_id = -1;
	g_android_profile_object_max_render_type = -1;
	g_android_profile_object_max_model = -1;
	g_android_profile_simulation_frame_id = 0;
	g_android_profile_frame_time_us = 0;
	g_android_profile_network_us = 0;
	memset(&g_android_profile_network_detail, 0, sizeof(g_android_profile_network_detail));
	g_android_profile_network_packets = 0;
	g_android_profile_network_bytes = 0;
	g_android_profile_remote_robot_updates = 0;
	g_android_profile_active_object_count = 0;
	g_android_profile_projectile_object_count = 0;
	g_android_profile_reactor_object_count = 0;
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

/* Only the game thread touches these end-to-begin gap accumulators */
static long long outer_wall, outer_cpu;
static enum android_profile_outer_stage outer_stage;
static int64_t outer_us[ANDROID_OUTER_COUNT], outer_cpu_us[ANDROID_OUTER_COUNT];
static int64_t frame_outer_us[ANDROID_OUTER_COUNT], frame_outer_cpu_us[ANDROID_OUTER_COUNT];
static int64_t android_profile_thread_cpu_us(void);

void android_profile_outer_mark(enum android_profile_outer_stage stage)
{
	long long wall, cpu;
	if (g_android_profile_frame_active || !outer_wall) return;
	wall = android_profile_now_us();
	cpu = android_profile_thread_cpu_us();
	outer_us[outer_stage] += wall - outer_wall;
	if (cpu >= 0 && outer_cpu >= 0 && outer_cpu_us[outer_stage] >= 0)
		outer_cpu_us[outer_stage] += cpu - outer_cpu;
	else
		outer_cpu_us[outer_stage] = -1;
	outer_wall = wall;
	outer_cpu = cpu;
	outer_stage = stage;
}

void android_profile_frame_begin(const char *game, unsigned int frame_id)
{
	long long now_ms;
	long long now_us;
	const int manual_enabled = debug_log_enabled[DLOG_PROFILING] ? 1 : 0;
	const int resuming = __atomic_exchange_n(&g_android_profile_resume_pending, 0,
	                                         __ATOMIC_ACQ_REL);

	if ((!g_android_slowdown_capture_requested || resuming) &&
	    g_android_stutter_detector.previous.frame.end_us) {
		if (android_stutter_detector_flush(&g_android_stutter_detector))
			android_stutter_log_window();
		android_stutter_detector_reset(&g_android_stutter_detector);
	}

	if (!g_android_slowdown_capture_requested &&
	    g_android_slowdown_detector.state == ANDROID_SLOWDOWN_CAPTURING)
		android_flight_flush_batch();
	android_slowdown_detector_set_enabled(&g_android_slowdown_detector,
	                                      g_android_slowdown_capture_requested);
	if (resuming) {
		g_android_profile_last_frame_begin_us = 0;
		g_android_profile_last_flip_us = 0;
		g_android_profile_latest_flip_gap_us = 0;
		if (g_android_slowdown_detector.state != ANDROID_SLOWDOWN_DISABLED)
			android_slowdown_detector_suppress_next_frame(
			    &g_android_slowdown_detector);
	}
	if (!manual_enabled &&
	    g_android_slowdown_detector.state == ANDROID_SLOWDOWN_DISABLED) {
		g_android_profile_frame_active = 0;
		g_android_profile_object_detail_active = 0;
		outer_wall = 0;
		memset(outer_us, 0, sizeof(outer_us));
		memset(outer_cpu_us, 0, sizeof(outer_cpu_us));
		return;
	}

	android_profile_outer_mark(ANDROID_OUTER_UNATTRIBUTED);
	memcpy(frame_outer_us, outer_us, sizeof(outer_us));
	memcpy(frame_outer_cpu_us, outer_cpu_us, sizeof(outer_cpu_us));
	memset(outer_us, 0, sizeof(outer_us));
	memset(outer_cpu_us, 0, sizeof(outer_cpu_us));
	outer_wall = 0;
	now_ms = android_profile_now_ms();
	now_us = android_profile_now_us();

	android_profile_maybe_finish_texture_burst(now_us, NULL);

	if (!manual_enabled) {
		android_profile_maybe_finish_texture_burst(now_us, "disabled");
		if (g_android_profile_sample_active)
			android_profile_finish_sample(now_ms);
	} else {
		if (g_android_profile_sample_active && now_ms >= g_android_profile_sample_end_ms)
			android_profile_finish_sample(now_ms);

		if (!g_android_profile_sample_active) {
			if (!g_android_profile_next_sample_ms || now_ms >= g_android_profile_next_sample_ms)
				android_profile_start_sample(now_ms, game);
		}
	}

	g_android_profile_frame_active = 1;
	g_android_profile_frame_id = frame_id;
	android_profile_copy_game(game);
	android_profile_reset_frame_metrics();
	g_android_profile_frame_start_us = now_us;
	g_android_profile_frame_begin_gap_us =
	    g_android_profile_last_frame_begin_us ? now_us - g_android_profile_last_frame_begin_us : 0;
	g_android_profile_last_frame_begin_us = now_us;
	g_android_profile_frame_flip_gap_us = g_android_profile_latest_flip_gap_us;
	g_android_profile_remote_live_generation++;
	if (!g_android_profile_remote_live_generation) {
		memset(g_android_profile_remote_robots, 0,
		       sizeof(g_android_profile_remote_robots));
		g_android_profile_remote_live_generation = 1;
	}
	g_android_profile_object_detail_active =
	    g_android_profile_sample_active ||
	    android_slowdown_detector_detail_active(&g_android_slowdown_detector, now_us);
}

void android_profile_resume(void)
{
	__atomic_store_n(&g_android_profile_resume_pending, 1, __ATOMIC_RELEASE);
}

void android_profile_set_frame_context(int level, int viewer_segment)
{
	if (!g_android_profile_frame_active)
		return;

	g_android_profile_level = level;
	g_android_profile_viewer_segment = viewer_segment;
	if (level != g_android_profile_remote_level) {
		memset(g_android_profile_remote_robots, 0,
		       sizeof(g_android_profile_remote_robots));
		g_android_profile_remote_level = level;
	}
}

void android_profile_set_frame_pacing(int max_fps, int vsync)
{
	if (!g_android_profile_frame_active)
		return;
	g_android_profile_max_fps = max_fps;
	g_android_profile_vsync = vsync ? 1 : 0;
}

void android_profile_set_simulation_metrics(unsigned int simulation_frame_id,
                                            int frame_time_us)
{
	if (!g_android_profile_frame_active)
		return;
	g_android_profile_simulation_frame_id = simulation_frame_id;
	g_android_profile_frame_time_us = frame_time_us;
}

void android_profile_note_flip(void)
{
	const long long now_us = android_profile_now_us();

	g_android_profile_latest_flip_gap_us =
	    g_android_profile_last_flip_us ? now_us - g_android_profile_last_flip_us : 0;
	g_android_profile_last_flip_us = now_us;
}

long long android_profile_network_begin(void)
{
	return g_android_profile_frame_active ? android_profile_now_us() : 0;
}

void android_profile_network_packet(int packet_bytes)
{
	if (!g_android_profile_frame_active)
		return;
	g_android_profile_network_packets++;
	if (packet_bytes > 0)
		g_android_profile_network_bytes += packet_bytes;
}

void android_profile_network_end(long long start_us)
{
	if (!g_android_profile_frame_active || start_us <= 0)
		return;
	g_android_profile_network_us += android_profile_now_us() - start_us;
}

static int64_t android_profile_thread_cpu_us(void)
{
	struct timespec ts;
	if (clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts) != 0)
		return -1;
	return (int64_t) ts.tv_sec * 1000000 + ts.tv_nsec / 1000;
}

struct android_network_stamp android_profile_net_begin(void)
{
	struct android_network_stamp stamp = { 0, -1 };
	const int saved_errno = errno;
	if (g_android_profile_frame_active &&
	    g_android_slowdown_detector.state != ANDROID_SLOWDOWN_DISABLED) {
		stamp.wall_us = android_profile_now_us();
		stamp.cpu_us = android_profile_thread_cpu_us();
	}
	errno = saved_errno;
	return stamp;
}

void android_profile_net_end(struct android_network_stamp start, int stage,
                             int socket_id, int packet_type, const char *packet_name,
                             int bytes, int result)
{
	const int saved_errno = errno;
	if (start.wall_us && g_android_profile_frame_active) {
		struct android_network_sample sample;
		const int64_t wall_us = android_profile_now_us();
		const int64_t cpu_us = android_profile_thread_cpu_us();
		memset(&sample, 0, sizeof(sample));
		sample.wall_us = wall_us - start.wall_us;
		sample.cpu_us = start.cpu_us >= 0 && cpu_us >= start.cpu_us ? cpu_us - start.cpu_us : -1;
		sample.socket_id = socket_id;
		sample.packet_type = packet_type;
		sample.bytes = bytes;
		sample.result = result;
		sample.error = result < 0 ? saved_errno : 0;
		strncpy(sample.packet_name, packet_name ? packet_name : "none", sizeof(sample.packet_name) - 1);
		android_network_profile_record(&g_android_profile_network_detail, stage, &sample);
	}
	errno = saved_errno;
}

void android_profile_remote_robot_update(int objnum, int signature)
{
	struct android_profile_remote_robot_state *state;

	if (!g_android_profile_frame_active || objnum < 0 ||
	    objnum >= ANDROID_PROFILE_REMOTE_ROBOT_CAPACITY)
		return;
	state = &g_android_profile_remote_robots[objnum];
	state->signature = signature;
	state->last_update_us = android_profile_now_us();
	g_android_profile_remote_robot_updates++;
}

void android_profile_remote_robot_live(int objnum, int signature,
                                       int remote_owned)
{
	struct android_profile_remote_robot_state *state;

	if (!g_android_profile_frame_active || objnum < 0 ||
	    objnum >= ANDROID_PROFILE_REMOTE_ROBOT_CAPACITY)
		return;
	state = &g_android_profile_remote_robots[objnum];
	if (state->signature != signature) {
		state->signature = signature;
		state->last_update_us = 0;
	}
	state->live_generation = g_android_profile_remote_live_generation;
	state->remote_owned = remote_owned ? 1 : 0;
}

void android_profile_set_scene_object_counts(int active_objects,
                                             int projectile_objects,
                                             int reactor_objects)
{
	if (!g_android_profile_frame_active)
		return;
	g_android_profile_active_object_count = active_objects;
	g_android_profile_projectile_object_count = projectile_objects;
	g_android_profile_reactor_object_count = reactor_objects;
}

void android_profile_set_slowdown_capture_enabled(int enabled)
{
	g_android_slowdown_capture_requested = enabled ? 1 : 0;
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
	long long now_us;

	if (!g_android_profile_frame_active)
		return;
	if (bucket < 0 || bucket >= ANDROID_PROFILE_BUCKET_COUNT)
		return;
	if (!g_android_profile_buckets[bucket].active)
		return;
	now_us = android_profile_now_us();

	g_android_profile_buckets[bucket].frame_us += now_us - g_android_profile_buckets[bucket].start_us;
	g_android_profile_buckets[bucket].active = 0;
	g_android_profile_buckets[bucket].start_us = 0;
}

long long android_profile_object_begin(void)
{
	if (!g_android_profile_frame_active || !g_android_profile_object_detail_active)
		return 0;

	return android_profile_now_us();
}

void android_profile_object_end(long long start_us, int objnum, int object_type,
                                int object_id, int render_type, int model_num)
{
	long long elapsed_us;

	if (!g_android_profile_frame_active)
		return;
	g_android_profile_object_draws++;
	if (start_us <= 0)
		return;

	elapsed_us = android_profile_now_us() - start_us;
	g_android_profile_object_total_us += elapsed_us;
	if (elapsed_us <= g_android_profile_object_max_us)
		return;

	g_android_profile_object_max_us = elapsed_us;
	g_android_profile_object_max_objnum = objnum;
	g_android_profile_object_max_type = object_type;
	g_android_profile_object_max_id = object_id;
	g_android_profile_object_max_render_type = render_type;
	g_android_profile_object_max_model = model_num;
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

	if (g_android_slowdown_detector.state == ANDROID_SLOWDOWN_CAPTURING &&
	    total_us >= ANDROID_PROFILE_TEXTURE_THRESHOLD_US) {
		android_flight_appendf(
		    "prof_v=2 type=texture capture=%u game=%s name=%s source=%s w=%d h=%d flags=0x%x total_us=%lld ktx2_read_us=%lld png_read_us=%lld upload_us=%lld mask_us=%lld",
		    g_android_slowdown_detector.capture_id,
		    game_name ? game_name : "unknown",
		    texture_name,
		    source_name,
		    width,
		    height,
		    flags,
		    total_us,
		    ktx2_read_us,
		    png_read_us,
		    upload_us,
		    mask_us);
	}
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

	if (g_android_slowdown_detector.state == ANDROID_SLOWDOWN_CAPTURING &&
	    total_us >= ANDROID_PROFILE_STORAGE_THRESHOLD_US) {
		android_flight_appendf(
		    "prof_v=2 type=storage capture=%u name=%s op=%s offset=%llu size=%llu total_us=%lld",
		    g_android_slowdown_detector.capture_id,
		    entry_name,
		    op_name,
		    offset,
		    size,
		    total_us);
	}
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
	long long now_us;
	long long now_ms;
	long long total_us;
	struct android_slowdown_frame flight_frame;
	int flight_events = 0;
	int local_robot_count = 0;
	int remote_robot_count = 0;
	int stale_remote_robot_count = 0;
	int unknown_remote_robot_age = 0;
	int max_remote_robot_age_ms = 0;
	int i;

	if (!g_android_profile_frame_active)
		return;
	now_us = android_profile_now_us();
	now_ms = now_us / 1000LL;
	total_us = now_us - g_android_profile_frame_start_us;
	outer_wall = now_us;
	outer_cpu = android_profile_thread_cpu_us();
	outer_stage = ANDROID_OUTER_PROFILE;

	android_profile_finish_open_buckets(now_us);
	g_android_profile_frame_active = 0;
	g_android_profile_object_detail_active = 0;
	for (i = 0; i < ANDROID_PROFILE_REMOTE_ROBOT_CAPACITY; i++) {
		struct android_profile_remote_robot_state *state =
		    &g_android_profile_remote_robots[i];
		int age_ms;

		if (state->live_generation != g_android_profile_remote_live_generation)
			continue;
		if (!state->remote_owned) {
			local_robot_count++;
			continue;
		}
		remote_robot_count++;
		if (!state->last_update_us) {
			unknown_remote_robot_age++;
			continue;
		}
		age_ms = android_profile_i32_duration(
		             now_us - state->last_update_us) /
		         1000;
		if (age_ms > max_remote_robot_age_ms)
			max_remote_robot_age_ms = age_ms;
		if (age_ms > ANDROID_PROFILE_REMOTE_STALE_MS)
			stale_remote_robot_count++;
	}
	if (g_android_slowdown_detector.state != ANDROID_SLOWDOWN_DISABLED) {
		memset(&flight_frame, 0, sizeof(flight_frame));
		flight_frame.end_us = now_us;
		flight_frame.frame_id = g_android_profile_frame_id;
		flight_frame.level = g_android_profile_level;
		flight_frame.viewer_segment = g_android_profile_viewer_segment;
		flight_frame.begin_gap_us =
		    android_profile_i32_duration(g_android_profile_frame_begin_gap_us);
		flight_frame.flip_gap_us =
		    android_profile_i32_duration(g_android_profile_frame_flip_gap_us);
		flight_frame.simulation_frame_id = g_android_profile_simulation_frame_id;
		flight_frame.frame_time_us = g_android_profile_frame_time_us;
		flight_frame.total_us = (int) total_us;
		flight_frame.wait_us = (int) g_android_profile_buckets[ANDROID_PROFILE_BUCKET_WAIT].frame_us;
		flight_frame.sim_us = (int) g_android_profile_buckets[ANDROID_PROFILE_BUCKET_SIM].frame_us;
		flight_frame.record_us = (int) g_android_profile_buckets[ANDROID_PROFILE_BUCKET_RECORD].frame_us;
		flight_frame.render_us = (int) g_android_profile_buckets[ANDROID_PROFILE_BUCKET_RENDER].frame_us;
		flight_frame.replay_us = (int) g_android_profile_buckets[ANDROID_PROFILE_BUCKET_REPLAY].frame_us;
		flight_frame.swap_us = (int) g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_SWAP];
		flight_frame.gpu_us = (int) g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_GPU];
		flight_frame.resolve_us = (int) g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_RESOLVE];
		flight_frame.gl_error_us = (int) g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_ERROR];
		flight_frame.textured_polys = r_tpolyc;
		flight_frame.water_faces = r_water_faces;
		flight_frame.texture_binds = r_texbinds;
		flight_frame.texture_reuses = r_texbind_reuse;
		flight_frame.shader_switches = r_shader_switches;
		flight_frame.mask_draws = r_mask_draws;
		flight_frame.merged_wall_hits = r_mwall_cache_hits;
		flight_frame.merged_wall_misses = r_mwall_cache_misses;
		flight_frame.object_draws = g_android_profile_object_draws;
		flight_frame.network_us =
		    android_profile_i32_duration(g_android_profile_network_us);
		flight_frame.network_packets = g_android_profile_network_packets;
		flight_frame.network_bytes = g_android_profile_network_bytes;
		flight_frame.remote_robot_updates = g_android_profile_remote_robot_updates;
		flight_frame.local_robot_count = local_robot_count;
		flight_frame.remote_robot_count = remote_robot_count;
		flight_frame.stale_remote_robot_count = stale_remote_robot_count;
		flight_frame.unknown_remote_robot_age = unknown_remote_robot_age;
		flight_frame.max_remote_robot_age_ms = max_remote_robot_age_ms;
		flight_frame.active_object_count = g_android_profile_active_object_count;
		flight_frame.projectile_object_count =
		    g_android_profile_projectile_object_count;
		flight_frame.reactor_object_count = g_android_profile_reactor_object_count;
		flight_frame.max_object_us = (int) g_android_profile_object_max_us;
		flight_frame.max_object_num = g_android_profile_object_max_objnum;
		flight_frame.max_object_type = g_android_profile_object_max_type;
		flight_frame.max_object_id = g_android_profile_object_max_id;
		flight_frame.max_object_render_type = g_android_profile_object_max_render_type;
		flight_frame.max_object_model = g_android_profile_object_max_model;
		flight_frame.max_fps = g_android_profile_max_fps;
		flight_frame.vsync = g_android_profile_vsync;
		{
			struct android_stutter_frame sample;
			memset(&sample, 0, sizeof(sample));
			sample.frame = flight_frame;
			sample.network = g_android_profile_network_detail;
			sample.multi_us = android_profile_i32_duration(g_android_profile_buckets[ANDROID_PROFILE_BUCKET_MULTI].frame_us);
			sample.move_us = android_profile_i32_duration(g_android_profile_buckets[ANDROID_PROFILE_BUCKET_MOVE].frame_us);
			sample.ai_us = android_profile_i32_duration(g_android_profile_buckets[ANDROID_PROFILE_BUCKET_AI].frame_us);
			sample.sound_us = android_profile_i32_duration(g_android_profile_buckets[ANDROID_PROFILE_BUCKET_SOUND].frame_us);
			sample.effects_us = android_profile_i32_duration(g_android_profile_buckets[ANDROID_PROFILE_BUCKET_EFFECTS].frame_us);
			sample.rewind_us = android_profile_i32_duration(g_android_profile_buckets[ANDROID_PROFILE_BUCKET_REWIND].frame_us);
			sample.mode = Game_mode;
			memcpy(sample.outer_us, frame_outer_us, sizeof(sample.outer_us));
			memcpy(sample.outer_cpu_us, frame_outer_cpu_us, sizeof(sample.outer_cpu_us));
			if (android_stutter_detector_feed(&g_android_stutter_detector, &sample))
				android_stutter_log_window();
		}
		flight_events = android_slowdown_detector_feed(&g_android_slowdown_detector,
		                                               &flight_frame);
		if (flight_events & ANDROID_SLOWDOWN_EVENT_TRIGGER)
			android_flight_start_capture(&flight_frame);
		if (flight_events & ANDROID_SLOWDOWN_EVENT_WINDOW) {
			android_flight_append_window("window",
			                             &g_android_slowdown_detector.completed_window);
			android_flight_flush_batch();
		}
		if (flight_events & ANDROID_SLOWDOWN_EVENT_CAPTURE_END)
			android_flight_end_capture(&flight_frame);
	}
	if (total_us >= ANDROID_PROFILE_SLOW_FRAME_US &&
	    now_us >= g_android_profile_next_slow_log_us) {
		debug_log(
		    DLOG_PROFILING,
		    "prof_v=1 type=slow_frame game=%s frame=%u level=%d viewer_seg=%d total_us=%lld wait_us=%lld sim_us=%lld record_us=%lld render_us=%lld replay_us=%lld swap_us=%lld gpu_us=%lld resolve_us=%lld glerr_us=%lld tpolys=%d water_faces=%d texbinds=%d texreuse=%d shader_switches=%d mask_draws=%d mwall_hits=%d mwall_misses=%d object_us=%lld object_draws=%d max_object_us=%lld max_obj=%d max_type=%d max_id=%d max_render=%d max_model=%d",
		    g_android_profile_game,
		    g_android_profile_frame_id,
		    g_android_profile_level,
		    g_android_profile_viewer_segment,
		    total_us,
		    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_WAIT].frame_us,
		    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_SIM].frame_us,
		    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_RECORD].frame_us,
		    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_RENDER].frame_us,
		    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_REPLAY].frame_us,
		    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_SWAP],
		    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_GPU],
		    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_RESOLVE],
		    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_ERROR],
		    r_tpolyc,
		    r_water_faces,
		    r_texbinds,
		    r_texbind_reuse,
		    r_shader_switches,
		    r_mask_draws,
		    r_mwall_cache_hits,
		    r_mwall_cache_misses,
		    g_android_profile_object_total_us,
		    g_android_profile_object_draws,
		    g_android_profile_object_max_us,
		    g_android_profile_object_max_objnum,
		    g_android_profile_object_max_type,
		    g_android_profile_object_max_id,
		    g_android_profile_object_max_render_type,
		    g_android_profile_object_max_model);
		g_android_profile_next_slow_log_us = now_us + ANDROID_PROFILE_SLOW_LOG_INTERVAL_US;
	}
	if (!g_android_profile_sample_active)
		return;

	g_android_profile_sample_frame_count++;
	g_android_profile_sample_total_us += total_us;
	if (total_us > g_android_profile_sample_max_us)
		g_android_profile_sample_max_us = total_us;
	android_profile_commit_frame_metrics();
	android_profile_appendf(
	    "prof_v=1 type=frame sample=%u game=%s frame=%u frame_index=%u total_us=%lld wait_us=%lld sim_us=%lld record_us=%lld render_us=%lld replay_us=%lld swap_us=%lld gpu_us=%lld resolve_us=%lld glerr_us=%lld tpolys=%d water_faces=%d texbinds=%d texreuse=%d shader_switches=%d mask_draws=%d mwall_hits=%d mwall_misses=%d",
	    g_android_profile_sample_id,
	    g_android_profile_game,
	    g_android_profile_frame_id,
	    g_android_profile_sample_frame_count,
	    total_us,
	    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_WAIT].frame_us,
	    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_SIM].frame_us,
	    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_RECORD].frame_us,
	    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_RENDER].frame_us,
	    g_android_profile_buckets[ANDROID_PROFILE_BUCKET_REPLAY].frame_us,
	    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_SWAP],
	    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_GPU],
	    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_RESOLVE],
	    g_android_profile_gl_frame_us[ANDROID_PROFILE_GL_ERROR],
	    r_tpolyc,
	    r_water_faces,
	    r_texbinds,
	    r_texbind_reuse,
	    r_shader_switches,
	    r_mask_draws,
	    r_mwall_cache_hits,
	    r_mwall_cache_misses);

	if (now_ms >= g_android_profile_sample_end_ms)
		android_profile_finish_sample(now_ms);
}

void android_profile_flush(void)
{
	if (android_stutter_detector_flush(&g_android_stutter_detector))
		android_stutter_log_window();
	android_stutter_detector_reset(&g_android_stutter_detector);
	android_profile_maybe_finish_texture_burst(android_profile_now_us(), "flush");

	if (g_android_profile_sample_active)
		android_profile_finish_sample(android_profile_now_ms());
	else
		android_profile_flush_batch();
	android_flight_flush_batch();
}

#endif /* defined(ANDROID) || defined(__ANDROID__) */
