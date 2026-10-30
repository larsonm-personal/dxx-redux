#include "classic_bank.h"
#include <cstring>
#include <stdexcept>
#include <unordered_map>

namespace xl_compat
{
namespace
{
std::string lowercase(std::string name)
{
	for (char &letter : name)
		if (letter >= 'A' && letter <= 'Z') letter = char(letter - 'A' + 'a');
	return name;
}
} // namespace

std::map<int, ClassicBankSample> select_classic_bank(const std::uint8_t *data, std::size_t size, const std::vector<std::string> &names)
{
	if (!data || size < 12 || size > max_classic_bank_bytes || std::memcmp(data, "DSND", 4))
		throw std::runtime_error("expected a bounded DSND sound bank");
	const auto word = [&](std::size_t at) {
		return std::uint32_t(data[at]) | std::uint32_t(data[at + 1]) << 8 |
		       std::uint32_t(data[at + 2]) << 16 | std::uint32_t(data[at + 3]) << 24;
	};
	if (word(4) != 1) throw std::runtime_error("unsupported DSND version (expected 1)");
	const auto count = word(8);
	if (count > (size - 12) / 20) throw std::runtime_error("truncated DSND sample headers");
	const std::size_t payload = 12 + std::size_t(count) * 20;
	std::unordered_map<std::string, int> registry;
	for (std::size_t i = 0; i < names.size(); ++i)
		if (names[i].empty() || !registry.emplace(lowercase(names[i]), int(i)).second)
			throw std::runtime_error("native sound registry has empty or duplicate names");
	std::map<int, ClassicBankSample> selected;
	for (std::size_t i = 0; i < count; ++i) {
		const auto at = 12 + i * 20;
		std::size_t length = 0;
		while (length < 8 && data[at + length]) ++length;
		const auto name = lowercase(std::string(reinterpret_cast<const char *>(data + at), length));
		const auto native = registry.find(name);
		if (native == registry.end()) continue;
		const auto bytes = word(at + 8), offset = word(at + 16);
		// SetupSounds overwrites earlier headers for the same registered name
		selected[native->second] = { offset, bytes };
	}
	for (auto &[sample, span] : selected) {
		// LoadSounds reads only the final header and ignores data_length
		if (!span.length || span.offset > size - payload || span.length > size - payload - span.offset)
			throw std::runtime_error("invalid DSND sample extent for " + names[unsigned(sample)]);
		span.offset += payload;
	}
	return selected;
}
} // namespace xl_compat
