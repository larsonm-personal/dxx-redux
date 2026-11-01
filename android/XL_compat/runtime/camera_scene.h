#ifndef DXX_XL_CAMERA_SCENE_H
#define DXX_XL_CAMERA_SCENE_H
#ifdef __cplusplus
extern "C" {
#endif
int xl_camera_scene_active(void);
int xl_camera_scene_particles(void);
int xl_camera_scene_lightning(void);
#ifdef __cplusplus
}
struct object;
namespace xl_compat
{
class CameraTargetGpu;
struct CameraTargetSpec;
// Resolved ordinary-camera options; capture must precede the main scene
void capture_camera_scene(CameraTargetGpu &target, const CameraTargetSpec &spec, object &viewer,
                          int zoom, bool particles, bool lightning);
} // namespace xl_compat
#endif
#endif
