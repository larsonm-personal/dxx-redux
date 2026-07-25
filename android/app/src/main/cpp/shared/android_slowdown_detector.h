#ifndef ANDROID_SLOWDOWN_DETECTOR_H
#define ANDROID_SLOWDOWN_DETECTOR_H

#include <stdint.h>

#define ANDROID_SLOWDOWN_RING_CAPACITY 768
#define ANDROID_SLOWDOWN_WORST_COUNT   3

#define ANDROID_SLOWDOWN_EVENT_TRIGGER     0x01
#define ANDROID_SLOWDOWN_EVENT_WINDOW      0x02
#define ANDROID_SLOWDOWN_EVENT_CAPTURE_END 0x04

enum android_slowdown_state {
	ANDROID_SLOWDOWN_DISABLED = 0,
	ANDROID_SLOWDOWN_ARMED,
	ANDROID_SLOWDOWN_CAPTURING,
	ANDROID_SLOWDOWN_COOLDOWN
};

struct android_slowdown_frame {
	int64_t end_us;
	uint32_t frame_id;
	int16_t level;
	int16_t viewer_segment;
	int32_t begin_gap_us;
	int32_t flip_gap_us;
	uint32_t simulation_frame_id;
	int32_t frame_time_us;
	int32_t total_us;
	int32_t wait_us;
	int32_t sim_us;
	int32_t render_us;
	int32_t replay_us;
	int32_t swap_us;
	int32_t gpu_us;
	int32_t resolve_us;
	int32_t gl_error_us;
	int32_t textured_polys;
	int16_t water_faces;
	int32_t texture_binds;
	int32_t texture_reuses;
	int16_t shader_switches;
	int16_t mask_draws;
	int32_t merged_wall_hits;
	int32_t merged_wall_misses;
	int32_t object_draws;
	int32_t network_us;
	int16_t network_packets;
	int32_t network_bytes;
	int16_t remote_robot_updates;
	int16_t local_robot_count;
	int16_t remote_robot_count;
	int16_t stale_remote_robot_count;
	int16_t unknown_remote_robot_age;
	int32_t max_remote_robot_age_ms;
	int16_t active_object_count;
	int16_t projectile_object_count;
	int16_t reactor_object_count;
	int32_t max_object_us;
	int16_t max_object_num;
	int16_t max_object_type;
	int16_t max_object_id;
	int16_t max_object_render_type;
	int16_t max_object_model;
	int16_t max_fps;
	int16_t vsync;
};

struct android_slowdown_window {
	int64_t start_us;
	int64_t end_us;
	int64_t total_us;
	int64_t nonwait_us;
	int32_t frames;
	int32_t fps_milli;
	int32_t expected_fps_milli;
	int32_t max_nonwait_us;
	int32_t max_begin_gap_us;
	int32_t max_flip_gap_us;
	int32_t max_network_us;
	int64_t network_us;
	int32_t network_packets;
	int64_t network_bytes;
	int32_t remote_robot_updates;
	int32_t max_remote_robot_age_ms;
	struct android_slowdown_frame worst[ANDROID_SLOWDOWN_WORST_COUNT];
};

struct android_slowdown_detector {
	struct android_slowdown_frame ring[ANDROID_SLOWDOWN_RING_CAPACITY];
	struct android_slowdown_window current_window;
	struct android_slowdown_window completed_window;
	int64_t last_frame_us;
	int64_t suppress_until_us;
	int64_t capture_end_us;
	int64_t cooldown_end_us;
	int64_t severe_frame_us[ANDROID_SLOWDOWN_WORST_COUNT];
	int64_t hard_stall_us;
	uint32_t ring_write;
	uint32_t ring_count;
	uint32_t capture_id;
	int32_t baseline_fps_milli;
	int32_t configured_max_fps;
	int32_t configured_vsync;
	int32_t current_level;
	int32_t slow_windows;
	int32_t state;
	int32_t trigger_severe;
};

void android_slowdown_detector_init(struct android_slowdown_detector *detector);
void android_slowdown_detector_set_enabled(struct android_slowdown_detector *detector,
                                           int enabled);
int android_slowdown_detector_feed(struct android_slowdown_detector *detector,
                                   const struct android_slowdown_frame *frame);
int android_slowdown_detector_ring_count(const struct android_slowdown_detector *detector);
const struct android_slowdown_frame *android_slowdown_detector_ring_get(
    const struct android_slowdown_detector *detector, int oldest_index);
int android_slowdown_detector_detail_active(const struct android_slowdown_detector *detector,
                                            int64_t now_us);

#endif /* ANDROID_SLOWDOWN_DETECTOR_H */
