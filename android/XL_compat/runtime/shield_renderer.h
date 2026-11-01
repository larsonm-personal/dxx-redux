#ifndef DXX_XL_RUNTIME_SHIELD_RENDERER_H
#define DXX_XL_RUNTIME_SHIELD_RENDERER_H
#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
#include <array>
#include <memory>
#include "../addon_animation.h"
namespace xl_compat
{
struct CameraHitHardware;
struct PogBitmap;
struct ShieldImageResource;
enum class ShieldFilter { nearest,
	                      linear,
	                      mip_nearest,
	                      trilinear };
// Decoded pixels must outlive the texture; the resource owns both
class ShieldTexture
{
  public:
	explicit ShieldTexture(const PogBitmap &pixels);
	~ShieldTexture();
	// Frame changes invalidate the previous handle; do not retain or leave it bound
	unsigned handle(unsigned frame, ShieldFilter filter);
	static void discard_context(bool abandoned);

  private:
	struct State;
	std::unique_ptr<State> state;
};
// One owner per logical addon, retained when its bitmap or graphics context changes
class ShieldAddon
{
	AddonAnimationClock clock;
	std::shared_ptr<const ShieldImageResource> image;
	unsigned index, frame = 0;

  public:
	explicit ShieldAddon(unsigned addon);
	ShieldAddon(const ShieldAddon &) = delete;
	ShieldAddon &operator=(const ShieldAddon &) = delete;
	void replace_image(std::shared_ptr<const ShieldImageResource> replacement);
	unsigned current_frame() const
	{
		return frame;
	}
	// InitSurface forces alpha to one in the hit branch; only resolved RGB is supplied
	void draw(uint32_t ticks, int quality, const CameraHitHardware &hit, const std::array<float, 3> &color,
	          const std::array<float, 16> &matrix, ShieldFilter filter);
};
void discard_shield_context(bool abandoned);
void draw_hit_shield(int quality, const CameraHitHardware &hit, const std::array<float, 4> &color,
                     const std::array<float, 16> &matrix, unsigned texture, int border_side, bool nearest);
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
