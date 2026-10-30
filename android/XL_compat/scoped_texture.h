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
// Pinned addon lookup uses a d2x-xl subdirectory within the mission texture scope
inline std::string addon_mission_texture_path(const char *hog, const char *bitmap)
{
	auto path = static_mission_texture_path(hog, bitmap);
	if (!path.empty()) path.insert(path.find_last_of('/') + 1, "d2x-xl/");
	return path;
}

// Animation names require a bounded decimal frame suffix; directory components stay forbidden.
inline std::string animation_mission_texture_path(const char *hog, const char *bitmap)
{
	if (!bitmap) return {};
	size_t length = 0;
	while (length < 128 && bitmap[length]) ++length;
	if (length == 128) return {};
	const std::string name(bitmap, length);
	const auto marker = name.find('#');
	if (marker == std::string::npos || marker + 1 == length || length - marker > 4) return {};
	unsigned frame = 0;
	for (size_t i = marker + 1; i < length; ++i) {
		if (name[i] < '0' || name[i] > '9') return {};
		frame = frame * 10 + unsigned(name[i] - '0');
	}
	if (frame >= 128) return {};
	auto path = static_mission_texture_path(hog, name.substr(0, marker).c_str());
	if (path.empty()) return {};
	path.insert(path.size() - 4, name.substr(marker));
	return path;
}
} // namespace xl_compat
