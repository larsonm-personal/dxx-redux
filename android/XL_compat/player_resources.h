#ifndef XL_PLAYER_RESOURCES_H
#define XL_PLAYER_RESOURCES_H
#include <algorithm>
#include <cstdint>
namespace xl_compat
{
constexpr bool player_resource_action(int type)
{
	return type == 17 || type == 18;
}
inline std::int32_t player_resource_after(std::int32_t current, std::int32_t value, bool energy)
{
	// Match defined XL float arithmetic; widen before subtraction and final clamp
	const float change = energy ? float(current) * (float(value) / 65536.0f) / 100.0f : float(value);
	const auto delta = static_cast<std::int64_t>(change);
	if (!delta) return current;
	const auto after = std::int64_t(current) - delta;
	return static_cast<std::int32_t>(std::clamp<std::int64_t>(after, -1, 200 * 65536));
}
} // namespace xl_compat
#endif
