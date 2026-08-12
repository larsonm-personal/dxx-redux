#include "pilot_pref_transaction.h"

#include "playsave_transaction.h"

#include <climits>
#include <cstdio>
#include <vector>

namespace
{
constexpr size_t max_file_size = 16u * 1024u * 1024u;
constexpr size_t max_total_size = 64u * 1024u * 1024u;

struct original_file {
	const char *path;
	std::vector<unsigned char> bytes;
};

bool read_original(const char *path, original_file &original, size_t &total_size)
{
	FILE *file = std::fopen(path, "rb");
	if (!file)
		return false;
	if (std::fseek(file, 0, SEEK_END) != 0) {
		std::fclose(file);
		return false;
	}
	const long length = std::ftell(file);
	if (length < 0 || static_cast<unsigned long>(length) > max_file_size ||
	    static_cast<size_t>(length) > max_total_size - total_size ||
	    std::fseek(file, 0, SEEK_SET) != 0) {
		std::fclose(file);
		return false;
	}
	original.path = path;
	original.bytes.resize(static_cast<size_t>(length));
	if (!original.bytes.empty() &&
	    std::fread(original.bytes.data(), 1, original.bytes.size(), file) != original.bytes.size()) {
		std::fclose(file);
		return false;
	}
	if (std::fclose(file) != 0)
		return false;
	total_size += original.bytes.size();
	return true;
}

bool restore_originals(const std::vector<original_file> &originals)
{
	bool restored = true;
	for (const auto &original : originals) {
		const void *data = original.bytes.empty()
		                       ? static_cast<const void *>("")
		                       : static_cast<const void *>(original.bytes.data());
		if (!playsave_atomic_replace_file(original.path, data, original.bytes.size()))
			restored = false;
	}
	return restored;
}
} // namespace

extern "C" int pilot_pref_patch_transaction(
    const struct pilot_pref_patch_target *targets, size_t target_count)
{
	if ((!targets && target_count) || target_count > static_cast<size_t>(INT_MAX))
		return -1;

	std::vector<original_file> originals;
	originals.reserve(target_count);
	size_t total_size = 0;
	for (size_t i = 0; i < target_count; ++i) {
		if (!targets[i].path || !targets[i].patch) return -1;
		originals.emplace_back();
		if (!read_original(targets[i].path, originals.back(), total_size)) return -1;
	}

	for (size_t i = 0; i < target_count; ++i) {
		if (targets[i].patch(targets[i].path, targets[i].context) != 1)
			return restore_originals(originals) ? -1 : -2;
	}
	return static_cast<int>(target_count);
}
