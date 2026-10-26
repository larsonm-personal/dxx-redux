#include "mission_label.h"
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <unordered_set>

namespace
{
std::string lower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) { return char(std::tolower(c)); });
	return value;
}
std::string trim(std::string value)
{
	const auto begin = value.find_first_not_of(" \t\r\n");
	return begin == std::string::npos ? "" : value.substr(begin, value.find_last_not_of(" \t\r\n") - begin + 1);
}
std::filesystem::path local_file(const std::filesystem::path &path)
{
	std::error_code error;
	if (std::filesystem::is_regular_file(path, error)) return path;
	// Mission filenames are case-insensitive on every engine platform
	std::filesystem::directory_iterator it(path.parent_path(), error), end;
	for (; !error && it != end; it.increment(error))
		if (lower(it->path().filename().string()) == lower(path.filename().string())) return it->path();
	return path;
}
std::uint32_t le32(const unsigned char *p)
{
	return std::uint32_t(p[0]) | std::uint32_t(p[1]) << 8 | std::uint32_t(p[2]) << 16 | std::uint32_t(p[3]) << 24;
}
bool extended_level(std::istream &input)
{
	unsigned char header[8];
	return input.read(reinterpret_cast<char *>(header), sizeof(header)) &&
	       !std::memcmp(header, "LVLP", 4) && le32(header + 4) > 8;
}
bool inspect(const std::filesystem::path &descriptor)
{
	std::ifstream input(descriptor, std::ios::binary | std::ios::ate);
	if (!input || input.tellg() < 0 || input.tellg() > 1024 * 1024) return false;
	input.seekg(0);
	std::unordered_set<std::string> levels;
	std::string line;
	while (std::getline(input, line)) {
		line = lower(trim(line.substr(0, line.find(';'))));
		const auto equal = line.find('=');
		if (equal != std::string::npos) {
			if (trim(line.substr(0, equal)) == "d2x-name") return true;
			continue;
		}
		line = trim(line.substr(0, line.find(',')));
		if (line.size() > 4 && line.substr(line.size() - 4) == ".rl2" &&
		    line.find_first_of("/\\:") == std::string::npos && line.find("..") == std::string::npos)
			levels.insert(line);
	}
	for (const auto &level : levels) {
		std::ifstream loose(local_file(descriptor.parent_path() / level), std::ios::binary);
		if (extended_level(loose)) return true;
	}
	auto hog_path = descriptor;
	hog_path.replace_extension(".hog");
	std::ifstream hog(local_file(hog_path), std::ios::binary | std::ios::ate);
	if (!hog) return false;
	const auto size = hog.tellg();
	hog.seekg(0);
	char signature[3];
	if (!hog.read(signature, 3)) return false;
	if (!std::memcmp(signature, "D2X", 3)) return true;
	if (std::memcmp(signature, "DHF", 3)) return false;
	for (unsigned entries = 0; entries < 100000 && hog.tellg() < size; ++entries) {
		unsigned char header[17];
		if (!hog.read(reinterpret_cast<char *>(header), sizeof(header))) return false;
		const auto length = le32(header + 13);
		const auto start = hog.tellg();
		if (start < 0 || std::streamoff(length) > size - start) return false;
		const auto terminator = std::find(header, header + 13, 0);
		const std::string name(reinterpret_cast<char *>(header), std::size_t(terminator - header));
		if (length >= 8 && levels.count(lower(name)) && extended_level(hog)) return true;
		hog.seekg(start + std::streamoff(length));
	}
	return false;
}
} // namespace

int xl_mission_requires_extensions(const char *descriptor_path)
{
	try {
		return descriptor_path && inspect(std::filesystem::path(descriptor_path));
	} catch (const std::exception &) {
		return 0;
	}
}
