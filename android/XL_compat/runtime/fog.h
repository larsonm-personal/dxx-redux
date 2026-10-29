#ifndef DXX_XL_RUNTIME_FOG_H
#define DXX_XL_RUNTIME_FOG_H
#include <stdint.h>
#ifdef __cplusplus
#include "../inventory.h"
void xl_fog_bind(const xl_compat::Inventory *source);
extern "C" {
#endif
int xl_fog_active(void);
int xl_fog_colors(float colors[4][4]);
int xl_fog_boundary_points(int segment, int side, int near_cap, int32_t points[4][3], int *reversed);
const char *ogl_xl_fog_prepare(void);
void ogl_xl_fog_volumes(const short *segments, int count);
void ogl_xl_fog_composite(void);
void ogl_xl_fog_destroy(int lost_context);
struct xl_fog_diagnostics {
	uint64_t preparations, volume_passes, composite_passes, boundaries, context_rebuilds, failures;
	int width, height, depth_bits, last_types, depth_copy_bits;
	float depth_copy_value;
};
void ogl_xl_fog_diagnostics(struct xl_fog_diagnostics *out);
struct xl_fog_gpu_result {
	int cases, pixels, multisample_cases, state_restored, max_error;
};
const char *ogl_xl_fog_gpu_probe(struct xl_fog_gpu_result *result);
struct xl_fog_scene_result {
	int completed, passed, pixels, covered, changed, types, samples, depth_bits, max_error;
	float center_depth, center_caps[8], max_length;
	int center_before[3], center_after[3], center_expected[3];
};
void ogl_xl_fog_scene_request(void);
void ogl_xl_fog_scene_result(struct xl_fog_scene_result *result);
const char *ogl_xl_fog_scene_error(void);
#ifdef __cplusplus
}
#endif
#endif
