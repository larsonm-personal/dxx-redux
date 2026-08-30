#ifdef ANDROID

#include <string.h>

#include "android_texture_debug.h"
#include "debug_tex_overlay.h"
#include "merged_wall_debug.h"

struct debug_tex_label g_debug_tex_labels[DEBUG_TEX_MAX_LABELS];
int g_debug_tex_label_count;
volatile int g_debug_tex_overlay_active;
volatile int g_merged_wall_debug_mode;
volatile int g_merged_wall_snapshot_pending;
volatile int g_merged_wall_snapshot_request_frame;
volatile int g_merged_wall_snapshot_request_mode;
volatile int g_merged_wall_render_pass;
volatile int g_merged_wall_frame_id;
volatile int g_merged_wall_draw_seq;
volatile int g_merged_wall_force_two_pass;
struct android_draw_face_context g_android_draw_face_ctx;
struct merged_wall_snapshot_result g_merged_wall_snapshot_result;
struct merged_wall_probe_result g_merged_wall_probe_result;
struct merged_wall_last_draw_state g_merged_wall_last_draw_state;
float g_font_rgb_override[3] = { -1.f, -1.f, -1.f };
int g_ogl_render_context;
volatile int gles3_shim_debug_mode;
int ogl_aniso_level;
int ogl_msaa_samples;
int g_texfilt_level;
volatile int g_aniso_pending_apply;
volatile int g_msaa_pending_apply;
volatile int g_texfilt_pending_apply;
int r_tpolyc;
int r_water_faces;
int r_texbinds;
int r_texbind_reuse;
int r_shader_switches;
int r_mask_draws;
int r_mwall_cache_hits;
int r_mwall_cache_misses;
volatile int g_fb_sample_r = -1;
volatile int g_fb_sample_g = -1;
volatile int g_fb_sample_b = -1;
volatile int g_fb_sample_a = -1;
volatile int g_fb_avg_r = -1;
volatile int g_fb_avg_g = -1;
volatile int g_fb_avg_b = -1;
volatile int g_fb_avg_a = -1;

int gles3_shim_probe_vbo_arrays(void)
{
	return 0;
}

void android_merged_wall_request_snapshot(int request_mode)
{
	g_merged_wall_snapshot_request_mode = request_mode;
	g_merged_wall_snapshot_request_frame = g_merged_wall_frame_id;
	memset(&g_merged_wall_snapshot_result, 0, sizeof(g_merged_wall_snapshot_result));
	memset(&g_merged_wall_probe_result, 0, sizeof(g_merged_wall_probe_result));
}

void android_merged_wall_texmerge_owner_reset(struct merged_wall_texmerge_owner *owner)
{
	if (owner)
		memset(owner, 0, sizeof(*owner));
}

void android_merged_wall_texmerge_owner_note(struct merged_wall_texmerge_owner *owner)
{
	(void) owner;
}

void android_merged_wall_log_texmerge_owner(const char *event, int slot,
                                            int tmap_bottom, int tmap_top, grs_bitmap *bottom_bmp,
                                            grs_bitmap *top_bmp, int orient,
                                            const struct merged_wall_texmerge_owner *owner)
{
	(void) event;
	(void) slot;
	(void) tmap_bottom;
	(void) tmap_top;
	(void) bottom_bmp;
	(void) top_bmp;
	(void) orient;
	(void) owner;
}

void android_merged_wall_cached_texmerge_clear_cache(void) {}

void android_merged_wall_set_draw_face_context(struct segment *segp, int sidenum,
                                               int tmap1, int tmap2, int wid_flags, int nv, int face_index)
{
	(void) segp;
	(void) sidenum;
	(void) tmap1;
	(void) tmap2;
	(void) wid_flags;
	(void) nv;
	(void) face_index;
}

void android_merged_wall_clear_draw_face_context(void) {}

void android_merged_wall_log_face(struct segment *segp, int sidenum, int tmap1,
                                  int tmap2, int wid_flags, float dot, int nv, int face_index)
{
	(void) segp;
	(void) sidenum;
	(void) tmap1;
	(void) tmap2;
	(void) wid_flags;
	(void) dot;
	(void) nv;
	(void) face_index;
}

void android_texture_debug_set_target(const char *value)
{
	(void) value;
}
const char *android_texture_debug_get_target_display(void)
{
	return "unavailable";
}
int android_texture_debug_target_is_crosshair(void)
{
	return 0;
}
int android_texture_debug_matches_target_name(const char *bitmapname)
{
	(void) bitmapname;
	return 0;
}

#endif
