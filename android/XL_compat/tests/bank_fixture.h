#ifndef DXX_XL_BANK_FIXTURE_H
#define DXX_XL_BANK_FIXTURE_H
#include <cstdint>
#include <string>
#include <vector>
#include <utility>
#include <stdexcept>
namespace xl_bank_fixture
{
using Bytes = std::vector<std::uint8_t>;
inline Bytes make(const std::vector<std::pair<std::string, Bytes>> &samples)
{
	Bytes result{ 'D', 'S', 'N', 'D' };
	const auto word = [&](std::size_t value) {
		for (unsigned shift = 0; shift < 32; shift += 8) result.push_back(std::uint8_t(value >> shift));
	};
	word(1);
	word(samples.size());
	std::size_t offset = 0;
	for (const auto &[name, bytes] : samples) {
		if (name.empty() || name.size() > 8) throw std::runtime_error("Invalid derived bank name");
		for (unsigned i = 0; i < 8; ++i) result.push_back(i < name.size() ? std::uint8_t(name[i]) : 0);
		word(bytes.size());
		word(bytes.size());
		word(offset);
		offset += bytes.size();
	}
	for (const auto &entry : samples) result.insert(result.end(), entry.second.begin(), entry.second.end());
	return result;
}
} // namespace xl_bank_fixture
#endif
