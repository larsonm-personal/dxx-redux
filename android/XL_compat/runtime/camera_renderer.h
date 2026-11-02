#ifndef DXX_XL_CAMERA_RENDERER_H
#define DXX_XL_CAMERA_RENDERER_H
#ifdef __cplusplus
namespace xl_compat
{
struct Inventory;
}
void xl_camera_renderer_load(const xl_compat::Inventory &level);
extern "C" {
#endif
struct _grs_bitmap;
void xl_camera_renderer_clear(void);
void xl_camera_renderer_reset(void);
void xl_camera_renderer_frame(void);
int xl_camera_monitor_face(int segment, int side);
void xl_camera_monitor_begin(int segment, int side, int count, const int *vertices, int overlay);
struct _grs_bitmap *xl_camera_monitor_bitmap(int overlay, struct _grs_bitmap *original);
int xl_camera_monitor_is_bitmap(const struct _grs_bitmap *bitmap);
void xl_camera_monitor_uv(int overlay, int count, float *coordinates);
void xl_camera_monitor_end(void);
#ifdef __cplusplus
}
#endif
#endif
