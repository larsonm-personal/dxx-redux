#ifndef DXX_XL_MOTION_MATH_H
#define DXX_XL_MOTION_MATH_H
#include "geometry.h"
#include <cmath>
#include <cstdlib>
extern "C" {
#include "vecmat.h"
}
namespace xl_compat
{
inline vms_angvec teleport_angles(const vms_vector &vector)
{
	const auto direction = xl_compat::geometry_fixed_direction({ vector.x, vector.y, vector.z });
	vms_angvec angles{};
	// XL retains the positive endpoint for both signs at exactly unit magnitude
	angles.p = std::abs(direction[1]) >= F1_0 ? 0x4000 : fix_asin(-direction[1]);
	angles.h = direction[0] || direction[2] ? fix_atan2(direction[2], direction[0]) : 0;
	return angles;
}

inline fix teleport_dot(const vms_vector &a, const vms_vector &b)
{
	return static_cast<fix>((double(a.x) * b.x + double(a.y) * b.y + double(a.z) * b.z) / 65536.0);
}

inline vms_matrix teleport_matrix(const vms_angvec &angles)
{
	const auto sine = [](fixang a) { return static_cast<fix>(std::sin(double(a) * 2.0 * 3.14159265358979323846 / 65536.0) * 65536.0); };
	const auto cosine = [](fixang a) { return static_cast<fix>(std::cos(double(a) * 2.0 * 3.14159265358979323846 / 65536.0) * 65536.0); };
	const auto multiply = [](fix a, fix b) { return static_cast<fix>(double(b) / 65536.0 * a); };
	const fix sp = sine(angles.p), cp = cosine(angles.p), sb = sine(angles.b), cb = cosine(angles.b), sh = sine(angles.h), ch = cosine(angles.h);
	const fix sbsh = multiply(sb, sh), cbch = multiply(cb, ch), cbsh = multiply(cb, sh), sbch = multiply(sb, ch);
	vms_matrix result;
	result.rvec = { cbch + multiply(sp, sbsh), multiply(sb, cp), multiply(sp, sbch) - cbsh };
	result.uvec = { multiply(sp, cbsh) - sbch, multiply(cb, cp), sbsh + multiply(sp, cbch) };
	result.fvec = { multiply(sh, cp), -sp, multiply(ch, cp) };
	return result;
}

} // namespace xl_compat
#endif
