#include "target_sources.h"
#include "retained_resource.h"
#include <SDL.h>
#include <cctype>
#include <filesystem>
#include <fstream>
#include <map>
#include <stdexcept>
#include <utility>
#include "mission_prepare.h"

namespace xl_compat
{
namespace
{
std::string folded(std::string name)
{
	for (auto &c : name) c = char(std::tolower(static_cast<unsigned char>(c)));
	return name;
}
void validate_name(const std::string &name)
{
	if (name.empty() || name.front() == '/' || name.find_first_of("\\:") != std::string::npos)
		throw std::runtime_error("Invalid incoming resource name");
	std::size_t begin = 0;
	for (;;) {
		const auto end = name.find('/', begin);
		const auto part = name.substr(begin, end == std::string::npos ? end : end - begin);
		if (part.empty() || part == "." || part == "..") throw std::runtime_error("Invalid incoming resource component");
		if (end == std::string::npos) return;
		begin = end + 1;
	}
}
std::vector<unsigned char> physical_bytes(const std::filesystem::path &path, std::size_t limit)
{
	std::ifstream file(path, std::ios::binary | std::ios::ate);
	const auto length = file ? file.tellg() : std::streampos(-1);
	if (length < 0 || static_cast<std::uintmax_t>(length) > limit) throw std::runtime_error("Incoming physical resource exceeds its bound: " + path.string());
	std::vector<unsigned char> bytes(static_cast<std::size_t>(length));
	file.seekg(0);
	if (!file.read(reinterpret_cast<char *>(bytes.data()), bytes.size())) throw std::runtime_error("Incomplete incoming physical resource: " + path.string());
	return bytes;
}

} // namespace

struct TargetSources::State {
	mission_preparation *mission;
	std::string archive, directory, installation;
	using Entries = std::map<std::string, std::vector<std::filesystem::path>>;
	std::map<std::string, Entries> directories;
	std::optional<std::vector<unsigned char>> installation_archive, mission_archive;
	std::filesystem::path locate(const std::string &root, const std::string &name)
	{
		validate_name(name);
		auto path = std::filesystem::u8path(root);
		if (root.empty()) return {};
		std::size_t begin = 0;
		for (;;) {
			if (!std::filesystem::is_directory(path)) return {};
			auto [directory, inserted] = directories.try_emplace(path.generic_string());
			if (inserted)
				for (const auto &entry : std::filesystem::directory_iterator(path))
					directory->second[folded(entry.path().filename().string())].push_back(entry.path());
			const auto end = name.find('/', begin);
			const auto component = folded(name.substr(begin, end == std::string::npos ? end : end - begin));
			const auto found = directory->second.find(component);
			if (found == directory->second.end()) return {};
			if (found->second.size() != 1) throw std::runtime_error("Ambiguous incoming resource case: " + name);
			path = found->second.front();
			if (end == std::string::npos) return path;
			begin = end + 1;
		}
	}
};

TargetSources::TargetSources(mission_preparation *mission, std::string installation) : state(std::make_unique<State>())
{
	if (!mission || !mission_prepared_descriptor(mission) || mission_prepared_descriptor(mission)->descent_version != 2)
		throw std::runtime_error("Incoming level sources require a retained D2 descriptor");
	state->mission = mission;
	state->archive = mission_prepared_archive(mission);
	state->directory = mission_prepared_directory(mission);
	state->installation = std::move(installation);
}
TargetSources::~TargetSources() = default;
const std::string &TargetSources::archive() const
{
	return state->archive;
}
int TargetSources::ordinal(const char *level, int requested) const
{
	if (!level || !*level) throw std::runtime_error("Incoming level sources require a level name");
	if (requested) {
		const char *name = mission_prepared_level(state->mission, requested);
		if (!name || SDL_strcasecmp(name, level)) throw std::runtime_error("Incoming source ordinal does not match the target descriptor");
		return requested;
	}
	const auto *descriptor = mission_prepared_descriptor(state->mission);
	int result = 0;
	for (int id = descriptor->last_secret_level; id <= descriptor->last_level; ++id) {
		if (!id) continue;
		const char *name = mission_prepared_level(state->mission, id);
		if (!name || SDL_strcasecmp(name, level)) continue;
		if (result) throw std::runtime_error("Repeated incoming level name requires an explicit ordinal");
		result = id;
	}
	if (!result) throw std::runtime_error("Incoming level is absent from its target descriptor");
	return result;
}
std::optional<std::vector<unsigned char>> TargetSources::archive_bytes(const std::string &name, std::size_t limit)
{
	validate_name(name);
	if (state->archive.empty()) return std::nullopt;
	if (!state->mission_archive) state->mission_archive = physical_bytes(std::filesystem::u8path(state->archive), 64 * 1024 * 1024);
	return xl_find_archive_resource(*state->mission_archive, std::filesystem::path(state->archive).extension().string(), name, limit);
}

bool TargetSources::archive_contains(const std::string &name)
{
	return bool(archive_bytes(name, 64 * 1024 * 1024));
}

std::optional<std::vector<unsigned char>> TargetSources::directory_bytes(const std::string &name, std::size_t limit, bool installation)
{
	validate_name(name);
	if (installation && !state->installation.empty() && std::filesystem::is_regular_file(std::filesystem::u8path(state->installation))) {
		if (!state->installation_archive) state->installation_archive = physical_bytes(std::filesystem::u8path(state->installation), 64 * 1024 * 1024);
		return xl_find_archive_resource(*state->installation_archive, std::filesystem::path(state->installation).extension().string(), name, limit);
	}
	const auto path = state->locate(installation ? state->installation : state->directory, name);
	if (path.empty()) return std::nullopt;
	if (!std::filesystem::is_regular_file(path)) throw std::runtime_error("Incoming resource is not a regular file: " + name);
	return physical_bytes(path, limit);
}

bool TargetSources::directory_exists(const std::string &name)
{
	const auto path = state->locate(state->directory, name);
	return !path.empty() && std::filesystem::is_directory(path);
}
} // namespace xl_compat
