#ifndef DXX_XL_ROBOT_GUN_POINT_H
#define DXX_XL_ROBOT_GUN_POINT_H
#include <array>
#include <cmath>
#include <cstdint>
#include <stdexcept>

namespace xl_compat
{
using GunVector = std::array<int32_t, 3>;
using GunMatrix = std::array<GunVector, 3>;
struct GunPart {
	int parent = 0;
	GunVector offset{};
	std::array<int16_t, 3> angles{};
};
inline int32_t gun_fixed(double value)
{
	if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX)
		throw std::runtime_error("XL gun transform exceeds fixed-point bounds");
	return int32_t(value);
}
// Default XL FixSinCos, FixMul and CFixMatrix::Create semantics
inline GunMatrix gun_angle_matrix(const std::array<int16_t, 3> &angles)
{
	std::array<int32_t, 3> sine{}, cosine{};
	for (unsigned i = 0; i < 3; ++i) {
		const double radians = (angles[i] * 2.0 * 3.141592653589793240) / 65536;
		sine[i] = gun_fixed(std::sin(radians) * 65536);
		cosine[i] = gun_fixed(std::cos(radians) * 65536);
	}
	const auto mul = [](int32_t a, int32_t b) { return gun_fixed(double(b) / 65536.0 * double(a)); };
	const auto sinp = sine[0], sinb = sine[1], sinh = sine[2];
	const auto cosp = cosine[0], cosb = cosine[1], cosh = cosine[2];
	const auto sbsh = mul(sinb, sinh), cbch = mul(cosb, cosh), cbsh = mul(cosb, sinh), sbch = mul(sinb, cosh);
	return { { { cbch + mul(sinp, sbsh), mul(sinb, cosp), mul(sinp, sbch) - cbsh },
		       { mul(sinp, cbsh) - sbch, mul(cosb, cosp), sbsh + mul(sinp, cbch) },
		       { mul(sinh, cosp), -sinp, mul(cosh, cosp) } } };
}
inline GunVector gun_transform(const GunVector &point, const GunMatrix &orientation, const GunVector &offset)
{
	GunVector result{};
	for (unsigned axis = 0; axis < 3; ++axis) {
		const auto rotated = gun_fixed((double(point[0]) * orientation[0][axis] +
		                                double(point[1]) * orientation[1][axis] +
		                                double(point[2]) * orientation[2][axis]) /
		                               65536.0);
		result[axis] = gun_fixed(double(rotated) + offset[axis]);
	}
	return result;
}
inline GunVector robot_gun_point(GunVector point, int part, const std::array<GunPart, 10> &parts,
                                 const GunMatrix &orientation, const GunVector &position)
{
	std::array<bool, 10> visited{};
	while (part) {
		if (part < 0 || part >= int(parts.size()) || visited[part])
			throw std::runtime_error("XL gun transform has an invalid fixed-header parent chain");
		visited[part] = true;
		const auto &model = parts[part];
		point = gun_transform(point, gun_angle_matrix(model.angles), model.offset);
		part = model.parent;
	}
	return gun_transform(point, orientation, position);
}
} // namespace xl_compat
#endif
