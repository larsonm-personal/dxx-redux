#ifndef DXX_XL_ADDON_ANIMATION_H
#define DXX_XL_ADDON_ANIMATION_H
#include <cstdint>
#include <stdexcept>
namespace xl_compat
{
// The addon owns this clock; replacing/unloading its bitmap does not reset it
class AddonAnimationClock
{
	int fps = 0;
	int64_t start = 0;

  public:
	bool advance(uint32_t ticks, bool bitmap, bool frame, int requested_fps = 10)
	{
		if (requested_fps < 0 || requested_fps > 1000) throw std::runtime_error("Unsupported XL addon animation rate");
		if (fps != requested_fps) {
			fps = requested_fps;
			if (fps) start = ticks;
		}
		if (!fps || int64_t(ticks) - start < 1000 / fps) return false;
		// Source consumes expiry before checking the bitmap and prepared frame
		start = ticks;
		return bitmap && frame;
	}
};
inline unsigned addon_next_frame(unsigned current, unsigned count)
{
	if (!count || count > 128 || current >= count) throw std::runtime_error("XL addon frame is out of range");
	return current + 1 >= count ? 0 : current + 1;
}
} // namespace xl_compat
#endif
