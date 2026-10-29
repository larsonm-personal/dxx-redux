#ifndef DXX_XL_MESSAGE_TIME_H
#define DXX_XL_MESSAGE_TIME_H
#include <cstdint>
namespace xl_compat
{
inline float message_opacity(std::int64_t elapsed_ms, std::int64_t remaining_ms)
{
	if (elapsed_ms < 0 || remaining_ms <= 0) return 0;
	if (elapsed_ms < 250) return float(elapsed_ms) / 250.0f;
	if (remaining_ms < 500) return float(remaining_ms) / 500.0f;
	return 1;
}
} // namespace xl_compat
#endif
