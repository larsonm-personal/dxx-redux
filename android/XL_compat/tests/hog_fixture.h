#ifndef DXX_XL_HOG_FIXTURE_H
#define DXX_XL_HOG_FIXTURE_H
#include "../inventory.h"
#include <algorithm>
#include <stdexcept>
namespace xl_test
{
inline void hog_member(xl_compat::Bytes &out, const std::string &name, const xl_compat::Bytes &payload, bool extended)
{
	if (name.empty() || name.size() > (extended ? 255u : 13u) || payload.size() > INT32_MAX || (extended && payload.empty()))
		throw std::runtime_error("Invalid HOG fixture row");
	const auto at = out.size();
	out.resize(at + 17 + (extended ? 256 : 0));
	const std::string short_name = extended ? "alias.bin" : name;
	std::copy(short_name.begin(), short_name.end(), out.begin() + at);
	const auto size = extended ? 0u - static_cast<std::uint32_t>(payload.size()) : static_cast<std::uint32_t>(payload.size());
	for (unsigned b = 0; b < 4; ++b) out[at + 13 + b] = static_cast<unsigned char>(size >> (8 * b));
	if (extended) std::copy(name.begin(), name.end(), out.begin() + at + 17);
	out.insert(out.end(), payload.begin(), payload.end());
}
} // namespace xl_test
#endif
