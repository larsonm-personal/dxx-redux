#ifndef DXX_XL_FIREBALL_MATERIAL_H
#define DXX_XL_FIREBALL_MATERIAL_H
#include <array>
#include <stdexcept>
namespace xl_compat
{
struct FireballMaterial {
	bool visible = false;
	std::array<float, 4> color{};
};
// DrawVClipObject rejects frames beyond the source's 30-entry animation bank
// DrawObjectBitmap uses gray modulation and additive blending for fireballs
inline FireballMaterial fireball_material(int frame, int frames)
{
	if (frames <= 0) throw std::runtime_error("Invalid XL fireball frame count");
	if (frame < 0 || frame >= 30) return {};
	if (frame >= frames) throw std::runtime_error("XL fireball frame exceeds its strip");
	return { true, { .5f, .5f, .5f, float(.8 - double(frame) / double(frames) * .8) } };
}
} // namespace xl_compat
#endif
