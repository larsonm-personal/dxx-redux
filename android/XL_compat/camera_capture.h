#ifndef DXX_XL_CAMERA_CAPTURE_H
#define DXX_XL_CAMERA_CAPTURE_H
#include <stdexcept>
namespace xl_compat
{
struct CameraTargetSpec {
	int width, height, buffer_type = 1, color_buffers = 2;
};
inline CameraTargetSpec camera_target_spec(int width, int height, bool high_resolution)
{
	if (width < 1 || height < 1 || width > 32767 || height > 32767)
		throw std::runtime_error("XL camera requires bounded screen dimensions");
	const int scale = high_resolution ? 1 : 2;
	if (width < scale || height < scale) throw std::runtime_error("XL camera target would be empty");
	return { width / scale, height / scale };
}
// Source CM_FULL_SCREEN; the manager restores the caller's cockpit after capture
inline constexpr int camera_full_screen = 3;
struct CameraCapture {
	bool valid = false;
	// Host supplies canvas/viewer operations; actual scene rendering is its callback
	template <class Host, class Viewer>
	bool render(Host &host, Viewer attached, Viewer fallback)
	{
		host.camera_active(true);
		if (host.cockpit_type() != camera_full_screen) host.activate_cockpit(camera_full_screen);
		if (host.release_buffer()) {
			const auto saved = host.set_viewer(attached ? attached : fallback);
			host.activate_canvas();
			host.render_frame();
			host.deactivate_canvas();
			host.set_viewer(saved);
			valid = true;
			host.disable_buffer();
		}
		host.camera_active(false);
		return valid;
	}
};
} // namespace xl_compat
#endif
