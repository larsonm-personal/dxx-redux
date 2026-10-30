#ifndef DXX_XL_CLASSIC_BANK_H
#define DXX_XL_CLASSIC_BANK_H
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <vector>

namespace xl_compat
{
constexpr std::size_t max_classic_bank_bytes = 128 * 1024 * 1024;
struct ClassicBankSample {
	std::size_t offset, length;
};
// DSND names replace existing native slots; unknown names never add samples
std::map<int, ClassicBankSample> select_classic_bank(const std::uint8_t *data, std::size_t size, const std::vector<std::string> &names);
} // namespace xl_compat
#endif
