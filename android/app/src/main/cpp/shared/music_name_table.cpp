#include "music_name_table.h"

#include <algorithm>
#include <cctype>
#include <stdexcept>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <nlohmann/json.hpp>

namespace
{
constexpr std::size_t max_records = 4096;
constexpr std::size_t max_paths_per_record = 4;
constexpr std::size_t max_aliases_per_record = 4;
constexpr std::size_t max_path_bytes = 1024;
constexpr std::size_t max_name_bytes = 512;
constexpr std::size_t max_file_bytes = 512 * 1024;

struct record {
	std::vector<std::string> paths;
	std::vector<std::string> aliases;
	std::string name;
};

using table = std::vector<record>;
table jukebox_table;
table mission_table;

std::string normalized_path(const std::string &value)
{
	std::string result = value;
	std::replace(result.begin(), result.end(), '\\', '/');
	while (!result.empty() && result.back() == '/') result.pop_back();
	return result;
}

std::string ascii_fold(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(), [](const unsigned char ch) {
		return ch >= 'A' && ch <= 'Z' ? static_cast<char>(ch - 'A' + 'a') : static_cast<char>(ch);
	});
	return value;
}

std::vector<std::string> parse_paths(const nlohmann::json &value, const std::size_t maximum)
{
	if (!value.is_array() || value.size() > maximum) throw std::runtime_error("invalid path array");
	std::vector<std::string> result;
	std::unordered_set<std::string> seen;
	for (const auto &item : value) {
		if (!item.is_string()) throw std::runtime_error("path is not a string");
		auto path = normalized_path(item.get<std::string>());
		if (path.empty() || path.size() > max_path_bytes) throw std::runtime_error("invalid path length");
		if (!seen.insert(ascii_fold(path)).second) throw std::runtime_error("duplicate path");
		result.emplace_back(std::move(path));
	}
	return result;
}

table parse_table(const char *text, const std::size_t length)
{
	if (!text || length == 0 || length > max_file_bytes) throw std::runtime_error("invalid sidecar size");
	std::vector<std::unordered_set<std::string>> object_keys;
	auto reject_duplicate_keys = [&object_keys](int, nlohmann::json::parse_event_t event, nlohmann::json &parsed) {
		if (event == nlohmann::json::parse_event_t::object_start) object_keys.emplace_back();
		if (event == nlohmann::json::parse_event_t::key &&
		    !object_keys.back().insert(parsed.get<std::string>()).second)
			throw std::runtime_error("duplicate JSON key");
		if (event == nlohmann::json::parse_event_t::object_end) object_keys.pop_back();
		return true;
	};
	const auto root = nlohmann::json::parse(text, text + length, reject_duplicate_keys, true, false);
	if (!root.is_object() || root.value("version", 0) != 1 || !root.contains("records") ||
	    !root["records"].is_array() || root["records"].size() > max_records)
		throw std::runtime_error("invalid sidecar root");
	for (auto field = root.begin(); field != root.end(); ++field) {
		const auto &key = field.key();
		if (key != "version" && key != "sourceIdentity" && key != "records")
			throw std::runtime_error("unknown root field");
	}
	if (root.contains("sourceIdentity") && !root["sourceIdentity"].is_string())
		throw std::runtime_error("invalid source identity");

	table result;
	std::unordered_set<std::string> exact_paths;
	for (const auto &item : root["records"]) {
		if (!item.is_object() || item.size() != 3 || !item.contains("paths") || !item.contains("aliases") ||
		    !item.contains("name") || !item["name"].is_string())
			throw std::runtime_error("invalid record");
		record parsed;
		parsed.paths = parse_paths(item["paths"], max_paths_per_record);
		parsed.aliases = parse_paths(item["aliases"], max_aliases_per_record);
		parsed.name = item["name"].get<std::string>();
		if (parsed.paths.empty() || parsed.name.empty() || parsed.name.size() > max_name_bytes)
			throw std::runtime_error("invalid record value");
		for (const auto &path : parsed.paths)
			if (!exact_paths.insert(ascii_fold(path)).second) throw std::runtime_error("duplicate exact path");
		result.emplace_back(std::move(parsed));
	}
	return result;
}

int load_table(table &destination, const char *json, const std::size_t length)
{
	try {
		auto parsed = parse_table(json, length);
		destination.swap(parsed);
		return 1;
	} catch (...) {
		return 0;
	}
}

const char *lookup(const table &source, const char *path, const bool aliases)
{
	if (!path) return nullptr;
	const auto wanted = ascii_fold(normalized_path(path));
	for (const auto &item : source)
		for (const auto &exact : item.paths)
			if (ascii_fold(exact) == wanted) return item.name.c_str();
	if (!aliases) return nullptr;
	const char *match = nullptr;
	for (const auto &item : source)
		for (const auto &alias : item.aliases)
			if (ascii_fold(alias) == wanted) {
				if (match) return nullptr;
				match = item.name.c_str();
				break;
			}
	return match;
}
} // namespace

extern "C" void music_name_table_clear_jukebox(void)
{
	jukebox_table.clear();
}
extern "C" void music_name_table_clear_mission(void)
{
	mission_table.clear();
}
extern "C" int music_name_table_load_jukebox(const char *json, const size_t length)
{
	return load_table(jukebox_table, json, length);
}
extern "C" int music_name_table_load_mission(const char *json, const size_t length)
{
	return load_table(mission_table, json, length);
}
extern "C" const char *music_name_table_lookup_jukebox(const char *path)
{
	return lookup(jukebox_table, path, false);
}
extern "C" const char *music_name_table_lookup_mission(const char *path)
{
	const auto exact = lookup(mission_table, path, false);
	if (exact) return exact;
	const auto normalized = normalized_path(path ? path : "");
	const auto slash = normalized.find_last_of('/');
	return lookup(mission_table, slash == std::string::npos ? normalized.c_str() : normalized.c_str() + slash + 1, true);
}
