#include "inventory.h"
#include "hog.h"
#include "admission.h"

#include <algorithm>
#include <cctype>
#include <fstream>
#include <filesystem>
#include <iostream>
#include <iterator>
#include <sstream>
#include <stdexcept>

int main(int argc, char **argv)
{
	if (argc != 3) {
		std::cerr << "Usage: xl_inspect input.rl2|input.hog report.json\nInspection only; does not authorize gameplay\n";
		return 2;
	}
	try {
		if (std::filesystem::weakly_canonical(argv[1]) == std::filesystem::weakly_canonical(argv[2]) ||
		    (std::filesystem::exists(argv[2]) && std::filesystem::equivalent(argv[1], argv[2])))
			throw std::runtime_error("input and report must be different files");
		std::ifstream input(argv[1], std::ios::binary | std::ios::ate);
		if (!input) throw std::runtime_error("cannot open input");
		const auto size = input.tellg();
		if (size < 0) throw std::runtime_error("cannot determine input size");
		const auto read = [&](std::uint64_t offset, unsigned char *out, std::size_t count) {
			if (offset > static_cast<std::uint64_t>(size) || count > static_cast<std::uint64_t>(size) - offset)
				throw std::runtime_error("input read outside file");
			input.clear();
			input.seekg(static_cast<std::streamoff>(offset));
			input.read(reinterpret_cast<char *>(out), static_cast<std::streamsize>(count));
			if (!input || input.gcount() != static_cast<std::streamsize>(count)) throw std::runtime_error("input read failed");
		};
		unsigned char signature[3]{};
		if (size >= 3) read(0, signature, sizeof(signature));
		std::vector<xl_compat::HogEntry> members;
		if (size >= 3 && xl_compat::hog_signature(signature)) members = xl_compat::index_hog(static_cast<std::uint64_t>(size), read);
		else {
			if (size > 64 * 1024 * 1024) throw std::runtime_error("input exceeds 64 MiB inspection limit");
			members.push_back({ "input.rl2", 0, static_cast<std::uint64_t>(size), false });
		}
		std::ostringstream report;
		report << "{\n  \"schema\": 1,\n  \"status\": \"inspection_only\",\n  \"playable\": false,\n  \"members\": [";
		for (std::size_t i = 0; i < members.size(); ++i) {
			if (i) report << ',';
			report << "\n    {\"name\": " << xl_compat::json_string(members[i].name) << ", \"bytes\": " << members[i].size
			       << ", \"offset\": " << members[i].offset << ", \"extended\": " << (members[i].extended ? "true" : "false") << '}';
		}
		report << "\n  ],\n  \"levels\": [";
		std::size_t levels = 0, errors = 0;
		for (const auto &m : members) {
			auto name = m.name;
			std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			if (name.size() < 4 || name.substr(name.size() - 4) != ".rl2") continue;
			if (levels++) report << ',';
			report << "\n    {\"name\": " << xl_compat::json_string(m.name) << ", \"inventory\": ";
			try {
				if (m.size > 64 * 1024 * 1024) throw std::runtime_error("level exceeds 64 MiB inspection limit");
				xl_compat::Bytes bytes(static_cast<std::size_t>(m.size));
				read(m.offset, bytes.data(), bytes.size());
				const auto inventory = xl_compat::inspect_level(bytes);
				const auto blockers = xl_compat::runtime_blockers(inventory);
				std::ostringstream level_report;
				level_report << xl_compat::inventory_json(inventory) << ", \"runtime_blockers\": [";
				for (std::size_t i = 0; i < blockers.size(); ++i) {
					if (i) level_report << ',';
					const auto &issue = blockers[i];
					level_report << "{\"feature\": " << xl_compat::json_string(issue.feature) << ", \"record\": " << issue.record
					             << ", \"detail\": " << xl_compat::json_string(issue.detail) << '}';
				}
				level_report << ']';
				report << level_report.str();
			} catch (const std::exception &e) {
				++errors;
				report << "{\"status\": \"rejected\", \"playable\": false, \"reason\": " << xl_compat::json_string(e.what()) << '}';
			}
			report << '}';
		}
		report << "\n  ]\n}\n";
		if (!levels) throw std::runtime_error("no RL2 levels found");
		const auto result = report.str();
		// Identical reports preserve bytes and modification time
		std::ifstream previous(argv[2], std::ios::binary);
		const std::string existing{ std::istreambuf_iterator<char>(previous), std::istreambuf_iterator<char>() };
		if (existing != result) {
			std::ofstream output(argv[2], std::ios::binary | std::ios::trunc);
			output << result;
			output.close();
			if (!output) throw std::runtime_error("report write failed");
		}
		std::cout << "Inspected " << levels << " levels; " << errors << " rejected; no gameplay admission granted\n";
		return errors ? 1 : 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 2;
	}
}
