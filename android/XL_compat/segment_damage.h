#ifndef DXX_XL_SEGMENT_DAMAGE_H
#define DXX_XL_SEGMENT_DAMAGE_H
#include <algorithm>
#include <cstdint>
namespace xl_compat
{
// Integer authored units/second times raw FrameTime is an exact fixed product
// Wide intermediates also define overflow cases outside reference signed ranges
inline std::int64_t segment_damage_amount(std::int32_t frame, std::int16_t rate, std::int32_t current, bool shield)
{
	if (!rate) return 0;
	return std::min(std::int64_t(frame) * rate, std::int64_t(current) + (shield ? 1 : 0));
}
inline std::int32_t segment_resource_after(std::int32_t current, std::int64_t amount)
{
	// XL Update bypasses Set on zero delta, retaining even over-cap resources
	if (!amount) return current;
	return static_cast<std::int32_t>(std::clamp(std::int64_t(current) - amount, INT64_C(-1), INT64_C(200) * 65536));
}
} // namespace xl_compat
#endif
