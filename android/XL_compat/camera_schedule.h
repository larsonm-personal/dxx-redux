#ifndef DXX_XL_CAMERA_SCHEDULE_H
#define DXX_XL_CAMERA_SCHEDULE_H
#include <cstdint>
#include <limits>
#include <stdexcept>
#include <vector>

namespace xl_compat
{
// Pinned CCamera::Ready/Reset: invisible cameras still accumulate wait frames
struct CameraSchedule {
	int wait_frames = 0;
	std::int64_t timeout = 0;
	bool visible = false, timed_out = false, teleport = false;
	int ready(std::int64_t now, int fps, bool teleporter_enabled, unsigned owner_flags)
	{
		if (fps < 0 || fps > 1000 || now < 0 || wait_frames == std::numeric_limits<int>::max())
			throw std::runtime_error("Camera scheduler exceeds qualified clock/rate bounds");
		++wait_frames;
		if (!visible || (teleport && !teleporter_enabled) || (owner_flags & 7)) return 0;
		if (fps && !timed_out) {
			if (now - timeout < 1000 / fps) return 0;
			timed_out = true;
		}
		return wait_frames;
	}
	void reset(std::int64_t now)
	{
		wait_frames = 0;
		timed_out = visible = false;
		timeout = now;
	}
};
struct ScheduledCamera {
	CameraSchedule schedule;
	unsigned owner_flags = 0;
	int type = 0;
};
inline int select_camera(std::vector<ScheduledCamera> &cameras, std::int64_t now, std::int64_t reset_time, int fps, bool enabled, bool teleporter_enabled)
{
	if (!enabled) return -1;
	int selected = -1, longest = -1;
	for (std::size_t i = 0; i < cameras.size(); ++i) {
		auto &camera = cameras[i];
		if (camera.type == 3) continue;
		const int wait = camera.schedule.ready(now, fps, teleporter_enabled, camera.owner_flags);
		if (wait && wait > longest) {
			longest = wait;
			selected = int(i);
		}
	}
	if (selected >= 0) cameras[std::size_t(selected)].schedule.reset(reset_time);
	return selected;
}
} // namespace xl_compat
#endif
