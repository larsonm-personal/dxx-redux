#pragma once
#include <string>
namespace xl_compat
{
// Authored mission scope only. Ordinal directories and animation need separate qualification.
inline std::string static_mission_texture_path(const char *hog, const char *bitmap)
{
	if (!hog || !bitmap) return {};
	const auto bounded = [](const char *value, size_t limit) {
		size_t n = 0;
		while (n < limit && value[n]) ++n;
		return n < limit;
	};
	if (!bounded(hog, 4096) || !bounded(bitmap, 128)) return {};
	const std::string archive(hog), name(bitmap);
	const auto slash = archive.find_last_of("/\\");
	const auto leaf = archive.substr(slash == std::string::npos ? 0 : slash + 1);
	if (leaf.size() <= 4) return {};
	auto ext = leaf.substr(leaf.size() - 4);
	for (char &c : ext)
		if (c >= 'A' && c <= 'Z') c += 'a' - 'A';
	if (ext != ".hog") return {};
	const auto identity = leaf.substr(0, leaf.size() - 4);
	const auto component = [](const std::string &value) {
		if (value.empty() || value.size() > 128 || value == "." || value == "..") return false;
		for (const unsigned char c : value)
			if (c < 32 || c == 127 || c == '/' || c == '\\' || c == ':' || c == '#') return false;
		return true;
	};
	if (!component(identity) || !component(name)) return {};
	return "mods/" + identity + "/textures/" + name + ".tga";
}
} // namespace xl_compat
