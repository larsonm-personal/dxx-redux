#ifndef DXX_XL_CAMERA_MONITOR_H
#define DXX_XL_CAMERA_MONITOR_H
#include <stdexcept>
namespace xl_compat
{
enum class CameraMonitorImage { unchanged,
	                            base,
	                            overlay };
struct CameraMonitorInput {
	bool exists = false, enabled = false, valid = false, texture = false;
	int buffer_status = 0;
	bool base = false;
	int overlay_texture = 0;
	bool fit_to_wall = false, teleport = false;
};
struct CameraMonitorSelection {
	CameraMonitorImage image = CameraMonitorImage::unchanged;
	bool visible = false, align = false, handled = false, clear_camera = false;
};
// Release SetupMonitorFace decisions for ordinary cameras
// Availability and texture ownership are resolved by the renderer before this call
inline CameraMonitorSelection camera_monitor_selection(const CameraMonitorInput &input)
{
	CameraMonitorSelection result;
	if (!input.exists) {
		result.clear_camera = true;
		return result;
	}
	if (input.teleport) throw std::runtime_error("Teleporter camera monitor rendering is not qualified");
	if (!input.enabled) return result;
	result.align = input.valid && (input.texture || input.buffer_status == 1);
	if (result.align)
		result.image = !input.overlay_texture || !input.base ? CameraMonitorImage::base : CameraMonitorImage::overlay;
	result.visible = true;
	result.handled = result.align || input.fit_to_wall;
	return result;
}
} // namespace xl_compat
#endif
