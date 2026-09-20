#pragma once

#include <algorithm>
#include <cctype>
#include <ctime>
#include <fstream>
#include <map>
#include <set>
#include <string>
#include <vector>
#include <physfs.h>
#include <nlohmann/json.hpp>
#include "hog_midi_catalog.h"

namespace mission_provenance
{
using Json = nlohmann::ordered_json;

inline std::string lower(std::string text)
{
	for (char &c : text)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return text;
}

inline std::string leaf(const std::string &path)
{
	return path.substr(path.find_last_of("/\\") + 1);
}

inline std::string stem(const std::string &path)
{
	const std::string name = lower(leaf(path));
	return name.substr(0, name.find_last_of('.'));
}

inline std::string trim(const std::string &text)
{
	const auto start = text.find_first_not_of(" \t\r\n");
	return start == std::string::npos ? "" : text.substr(start, text.find_last_not_of(" \t\r\n") - start + 1);
}

inline std::string field_name(const std::string &label)
{
	std::string key;
	for (unsigned char c : lower(label))
		if (std::isalnum(c)) key += static_cast<char>(c);
	static const std::map<std::string, std::string> fields = {
		{ "author", "author" }, { "authors", "author" }, { "nickname", "nickname" }, { "pilotname", "nickname" }, { "realname", "real_name" }, { "email", "email" }, { "emailaddress", "email" }, { "website", "website" }, { "url", "website" }, { "urladdress", "website" }, { "version", "version" }, { "editor", "editor" }, { "editorsused", "editor" }, { "revision", "version" }, { "buildtime", "build_time" }, { "date", "date" }, { "reldate", "release_date" }, { "releasedate", "release_date" }, { "datefinished", "finished_date" }, { "datestarted", "started_date" }
	};
	const auto found = fields.find(key);
	return found == fields.end() ? "" : found->second;
}

inline void parse_document(Json &credits, const std::string &source, const std::string &text)
{
	size_t offset = 0;
	while (offset < text.size() && credits.size() < 128) {
		const size_t end = text.find_first_of("\r\n", offset);
		std::string line = trim(text.substr(offset, end == std::string::npos ? end : end - offset));
		offset = end == std::string::npos ? text.size() : end + 1;
		if (!line.empty() && line.front() == ';') line = trim(line.substr(1));
		const size_t separator = line.find_first_of("=:");
		if (separator == std::string::npos || separator > 40) continue;
		const std::string key = field_name(line.substr(0, separator));
		const std::string value = trim(line.substr(separator + 1));
		if (key.empty() || value.empty() || value.size() > 512) continue;
		const std::string normalized = lower(value);
		if (normalized == "n/a" || normalized == "none" || normalized == "unknown" || normalized == "none yet") continue;
		bool merged = false;
		for (auto &credit : credits) {
			if (credit["field"] == key && credit["value"] == value) {
				auto &sources = credit["sources"];
				if (std::find(sources.begin(), sources.end(), source) == sources.end()) sources.push_back(source);
				merged = true;
				break;
			}
		}
		if (!merged) credits.push_back({ { "field", key }, { "value", value }, { "sources", Json::array({ source }) } });
	}
}

inline int current_year()
{
	const std::time_t now = std::time(nullptr);
	const std::tm *utc = std::gmtime(&now);
	return utc ? utc->tm_year + 1900 : 2026;
}

// Keep ambiguous free-form dates verbatim; only a standalone four-digit year is interpreted
inline int declared_year(const std::string &value)
{
	int result = 0;
	for (size_t i = 0; i < value.size();) {
		if (!std::isdigit(static_cast<unsigned char>(value[i]))) {
			++i;
			continue;
		}
		const size_t start = i;
		while (i < value.size() && std::isdigit(static_cast<unsigned char>(value[i]))) ++i;
		if (i - start == 4) {
			const int year = std::stoi(value.substr(start, 4));
			if (year < 1995 || year > current_year() || (result && result != year)) return 0;
			result = year;
		}
	}
	return result;
}

inline Json estimate(const Json &credits, Json &dates)
{
	std::set<int> declared;
	for (const auto &credit : credits)
		if (credit["field"] == "release_date" || credit["field"] == "finished_date") {
			const int year = declared_year(credit["value"].get<std::string>());
			if (year) declared.insert(year);
		}
	std::set<int> years;
	for (auto &date : dates) {
		const std::string value = date.value("modified", "");
		const int year = value.size() >= 10 ? declared_year(value.substr(0, 4)) : 0;
		if (!year) date["excluded"] = "invalid_or_implausible_year";
		else years.insert(year);
	}
	Json result = { { "year", nullptr }, { "basis", "unknown" }, { "confidence", "unknown" } };
	if (declared.size() == 1) {
		result["year"] = *declared.begin();
		result["basis"] = "declared_release_or_completion";
		result["confidence"] = "medium";
	} else if (declared.empty() && years.size() == 1) {
		result["year"] = *years.begin();
		result["basis"] = "archive_modification_dates";
		result["confidence"] = "low";
	} else if (declared.size() > 1 || years.size() > 1) {
		result["basis"] = "conflicting_dates";
	}
	if (!years.empty()) result["file_year_range"] = Json::array({ *years.begin(), *years.rbegin() });
	return result;
}

inline std::string resolve(const std::string &name, const std::vector<std::string> &entries)
{
	const auto key = lower(name);
	for (const auto &entry : entries)
		if (lower(entry) == key) return entry;
	return "";
}

inline int credit_document(const char *name)
{
	return hog_midi_has_extension(name, ".aut") || hog_midi_has_extension(name, ".msn") ||
	       hog_midi_has_extension(name, ".mn2") || hog_midi_has_extension(name, ".txt");
}

inline Json collect(const Json &request, const Json &result)
{
	Json credits = Json::array(), dates = Json::array();
	std::vector<std::string> entries;
	for (const char *directory : { "", "missions" }) {
		char **list = PHYSFS_enumerateFiles(directory);
		if (!list) continue;
		for (char **entry = list; *entry; ++entry)
			entries.emplace_back(std::string(directory).empty() ? *entry : std::string(directory) + "/" + *entry);
		PHYSFS_freeList(list);
	}
	std::sort(entries.begin(), entries.end());
	std::set<std::string> asset_names, hogs;
	const std::string descriptor = request.value("mission_filename", "");
	const std::string extra = request.value("extra_data_dir", "");
	const auto source_name = [&](const std::string &name) {
		if (request.contains("provenance_file_dates"))
			for (const auto &date : request["provenance_file_dates"])
				if (lower(leaf(date.value("file", ""))) == lower(leaf(name))) return date.value("file", "");
		return leaf(name);
	};
	// A HOG can shadow its external descriptor after mission loading
	// Read the requested descriptor itself, not whichever virtual file currently wins
	if (!extra.empty() && !descriptor.empty()) {
		std::ifstream file(extra + "/" + descriptor, std::ios::binary | std::ios::ate);
		if (file) {
			const auto size = file.tellg();
			if (size > 0 && size <= 65536) {
				std::string text(static_cast<size_t>(size), '\0');
				file.seekg(0);
				if (file.read(&text[0], size)) {
					parse_document(credits, source_name(descriptor), text);
					asset_names.insert(lower(leaf(descriptor)));
				}
			}
		}
	}
	const auto add_asset = [&](const std::string &name) {
		const std::string path = resolve(name, entries);
		const char *origin = path.empty() ? nullptr : PHYSFS_getRealDir(path.c_str());
		if (!origin) return;
		const std::string container = origin;
		if (lower(container).size() >= 4 && lower(container).substr(container.size() - 4) == ".hog") {
			// Only mission containers, never the stock game HOG
			const auto base = lower(leaf(container));
			if (base != "descent.hog" && base != "descent2.hog" && base != "d2demo.hog") {
				hogs.insert(container);
				asset_names.insert(base);
			}
		} else if (!extra.empty() && (container == extra || container == extra + "/missions")) {
			asset_names.insert(lower(leaf(path)));
		}
	};
	if (!descriptor.empty()) add_asset(descriptor);
	if (result.contains("levels"))
		for (const auto &level : result["levels"]) add_asset(level.value("level_file", ""));
	for (const auto &path : entries) {
		const char *origin = PHYSFS_getRealDir(path.c_str());
		if (!origin) continue;
		const std::string name = lower(leaf(path));
		const auto dot = name.find_last_of('.');
		const std::string ext = dot == std::string::npos ? "" : name.substr(dot);
		const bool in_directory = !extra.empty() && (extra == origin || extra + "/missions" == origin);
		const bool is_descriptor = !descriptor.empty() && lower(descriptor) == lower(path);
		const bool is_text = ext == ".txt" && !descriptor.empty() && stem(path) == stem(descriptor);
		if (!(in_directory && (is_descriptor || is_text))) continue;
		PHYSFS_File *file = PHYSFS_openRead(path.c_str());
		if (!file) continue;
		const auto size = PHYSFS_fileLength(file);
		if (size > 0 && size <= 65536) {
			std::string text(static_cast<size_t>(size), '\0');
			if (PHYSFS_readBytes(file, &text[0], size) == size)
				parse_document(credits, source_name(path), text);
		}
		PHYSFS_close(file);
	}
	// Read each selected container directly so virtual mount precedence cannot hide credits
	for (const auto &hog : hogs) {
		hog_midi_catalog catalog = {};
		if (hog_catalog_load_filtered(hog.c_str(), &catalog, credit_document) != HOG_MIDI_CATALOG_OK) continue;
		if (catalog.count > 1)
			std::sort(catalog.entries, catalog.entries + catalog.count, [](const hog_midi_entry &a, const hog_midi_entry &b) {
				return std::string(a.name) < std::string(b.name);
			});
		for (size_t i = 0; i < catalog.count; ++i) {
			const auto &entry = catalog.entries[i];
			if (entry.size > 65536 || (!hog_midi_has_extension(entry.name, ".aut") && stem(entry.name) != stem(descriptor))) continue;
			unsigned char *data = nullptr;
			int size = 0;
			if (hog_midi_catalog_read(hog.c_str(), &catalog, i, &data, &size)) {
				parse_document(credits, source_name(hog) + "/" + entry.name, std::string(reinterpret_cast<const char *>(data), size));
				free(data);
			}
		}
		hog_midi_catalog_free(&catalog);
	}
	// Transport supplies original archive timestamps, never staging filesystem times
	const auto supplied = request.find("provenance_file_dates");
	if (supplied != request.end() && supplied->is_array())
		for (const auto &date : *supplied)
			if (asset_names.count(lower(leaf(date.value("file", ""))))) dates.push_back(date);
	std::sort(dates.begin(), dates.end(), [](const Json &a, const Json &b) { return a["file"].get<std::string>() < b["file"].get<std::string>(); });
	Json provenance = { { "credits", credits }, { "date_estimate", estimate(credits, dates) }, { "file_dates", dates } };
	provenance["notes"] = Json::array({ "File dates describe modification, not verified creation or release", "D1/D2 HOG entries do not store individual asset timestamps" });
	return provenance;
}
} // namespace mission_provenance
