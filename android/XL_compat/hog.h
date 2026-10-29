#ifndef DXX_XL_HOG_H
#define DXX_XL_HOG_H
#include <array>
#include <cstdint>
#include <functional>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace xl_compat
{
struct HogEntry {
	std::string name;
	std::uint64_t offset, size;
	bool extended;
};
inline std::string hog_key(std::string name)
{
	for (auto &c : name)
		if (c >= 'A' && c <= 'Z') c = static_cast<char>(c + ('a' - 'A'));
	return name;
}
inline bool hog_signature(const unsigned char *bytes)
{
	return bytes[0] == 'D' && ((bytes[1] == 'H' && bytes[2] == 'F') || (bytes[1] == '2' && bytes[2] == 'X'));
}
// The callback reads exactly count bytes at offset or throws; payloads are never copied
inline std::vector<HogEntry> index_hog(std::uint64_t length, const std::function<void(std::uint64_t, unsigned char *, std::size_t)> &read)
{
	const auto require = [](bool valid, const std::string &message) {
		if (!valid) throw std::runtime_error(message);
	};
	std::uint64_t offset = 0;
	const auto take = [&](unsigned char *out, std::size_t count) {
		require(offset <= length && count <= length - offset, "Truncated HOG header");
		read(offset, out, count);
		offset += count;
	};
	unsigned char signature[3];
	take(signature, sizeof(signature));
	require(hog_signature(signature), "Invalid HOG signature (expected DHF or D2X)");
	std::vector<HogEntry> entries;
	std::set<std::string> names;
	while (offset < length) {
		std::array<unsigned char, 256> name{};
		take(name.data(), 13);
		unsigned char size_bytes[4];
		take(size_bytes, sizeof(size_bytes));
		const std::uint32_t raw = std::uint32_t(size_bytes[0]) | std::uint32_t(size_bytes[1]) << 8 |
		                          std::uint32_t(size_bytes[2]) << 16 | std::uint32_t(size_bytes[3]) << 24;
		const bool extended = (raw & 0x80000000u) != 0;
		// INT_MIN cannot be negated by the source reader
		require(raw != 0x80000000u, "Invalid HOG member size INT_MIN");
		if (extended) take(name.data(), name.size());
		const auto width = extended ? name.size() : std::size_t(13);
		std::string selected;
		for (std::size_t i = 0; i < width && name[i]; ++i) selected += static_cast<char>(name[i]);
		require(!extended || selected.size() < width, "Unterminated extended HOG member name");
		require(!selected.empty() && selected != "." && selected != ".." && selected.find_first_of("/\\:") == std::string::npos,
		        "Invalid HOG member name");
		for (const auto c : selected) require(static_cast<unsigned char>(c) >= 32 && c != 127, "Invalid HOG member name control byte");
		require(names.insert(hog_key(selected)).second, "Ambiguous duplicate HOG member " + selected);
		const std::uint64_t size = extended ? std::uint64_t(0x100000000ull) - raw : raw;
		require(size <= length - offset, "Truncated HOG member " + selected);
		entries.push_back({ selected, offset, size, extended });
		offset += size;
	}
	return entries;
}
} // namespace xl_compat
#endif
