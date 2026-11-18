#ifndef DXX_XL_HOG_H
#define DXX_XL_HOG_H
#include "hog_selection.h"
#include <algorithm>
#include <cstdint>
#include <functional>
#include <stdexcept>
#include <string>
#include <utility>
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
	return dxx_hog_signature(bytes) != 0;
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
	struct Reader {
		std::uint64_t &offset;
		std::uint64_t length;
		const std::function<void(std::uint64_t, unsigned char *, std::size_t)> &read;
	} reader{ offset, length, read };
	const auto next = [](void *context, unsigned char *out, std::size_t count) -> std::size_t {
		auto &source = *static_cast<Reader *>(context);
		count = static_cast<std::size_t>(std::min<std::uint64_t>(count, source.length - source.offset));
		if (count) source.read(source.offset, out, count);
		source.offset += count;
		return count;
	};
	while (offset < length) {
		dxx_hog_header entry{};
		const char *error = nullptr;
		const auto status = dxx_hog_read_header(next, &reader, &entry, &error);
		require(status == 1, error ? error : "Truncated HOG header");
		const std::string selected(entry.name);
		require(entry.size <= length - offset, "Truncated HOG member " + selected);
		entries.push_back({ selected, offset, entry.size, entry.extended != 0 });
		offset += entry.size;
	}
	std::vector<dxx_hog_lookup> order;
	order.reserve(entries.size());
	for (const auto &entry : entries) order.push_back({ entry.name.c_str(), order.size() });
	std::vector<unsigned char> selected(entries.size());
	dxx_hog_select(order.data(), order.size(), selected.data());
	std::size_t kept = 0;
	for (std::size_t i = 0; i < entries.size(); ++i) {
		if (!selected[i]) continue;
		if (kept != i) entries[kept] = std::move(entries[i]);
		++kept;
	}
	entries.resize(kept);
	return entries;
}
} // namespace xl_compat
#endif
