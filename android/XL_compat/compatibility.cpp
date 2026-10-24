#include "compatibility.h"
#include "../app/src/main/cpp/shared/mission_descriptor_safe.h"
#include <algorithm>
#include <cctype>
#include <sstream>

namespace xl_compat
{
namespace
{
struct Category {
	const char *id;
	const char *label;
	const char *support;
};
const Category categories[] = {
	{ "container", "Archive and descriptor dialect", "Standard DHF and XL descriptor discovery; extended HOG and XL gameplay unsupported" },
	{ "serialization", "Level and game-data versions", "Production D2 loader accepts level versions 1-8; XL inspection is separate" },
	{ "capacity", "Table capacities and reference widths", "9000 segments/36000 vertices; 255 walls/1000 objects/100 triggers; XL widening unsupported" },
	{ "segments", "Segment physics, properties and ownership", "Classic functions including repair centers exist; XL environment and ownership parity unverified" },
	{ "walls", "Wall types and flags", "Stock wall semantics only; XL extensions unsupported" },
	{ "triggers", "Stateful events and geometry triggers", "Stock actions only; XL master chains, scheduling, flags and persistence unsupported" },
	{ "object_triggers", "Object-attached triggers", "Unsupported" },
	{ "objects", "Objects and AI behavior", "Stock behavior only; XL behavior parity unverified" },
	{ "gameplay_assets", "Custom robot and weapon definitions", "XL gameplay asset formats and dependencies unverified" },
	{ "baked_lighting", "Embedded colored lighting", "Parsed for inspection; not applied by production rendering" },
	{ "dynamic_lighting", "Dynamic lights and rendering modes", "XL lighting modes not implemented" },
	{ "visual_assets", "Texture and model formats", "XL asset formats require individual verification" },
	{ "mission_flow", "Mission flow and scripted behavior", "XL objectives and campaign behavior unverified" },
	{ "runtime", "Gameplay and presentation verification", "No XL mission has verified production support" },
	{ "navigation", "Teleports, boosts and spawn destinations", "XL movement actions and route-analysis edges unsupported" },
	{ "generators", "Equipment generators and extended spawning", "Classic robot matcens exist; XL item generation and spawning unverified" },
	{ "wall_materials", "Authored wall opacity and materials", "Classic cloaking and texture transparency exist; XL opacity, visibility and collision parity unverified" },
	{ "effect_objects", "Particles, lightning and ambient sounds", "XL payloads, activation and audio cues require individual verification" },
	{ "resource_scope", "Mission resource precedence and isolation", "Standard HXM/extra-HAM and HMQ paths exist; XL override semantics and transition cleanup unverified" },
};
struct Evidence {
	std::string need = "Unknown - not inspected", status = "unknown", detail;
};
bool prefix(const Bytes &b, const char *s)
{
	return b.size() >= 3 && b[0] == s[0] && b[1] == s[1] && b[2] == s[2];
}
} // namespace

std::string compatibility_json(const std::string &name, const Bytes &bytes)
{
	Evidence rows[sizeof(categories) / sizeof(categories[0])];
	bool detected = false;
	std::string status = "unknown", summary = "Insufficient evidence";
	if (prefix(bytes, "D2X")) {
		detected = true;
		status = "blocked";
		summary = "Cannot load: extended HOG archive";
		rows[0] = { "Extended HOG", "blocked", "Detected D2X archive signature" };
	} else if (bytes.size() >= 8 && prefix(bytes, "LVL") && bytes[3] == 'P') {
		const std::uint32_t version = std::uint32_t(bytes[4]) | (std::uint32_t(bytes[5]) << 8) |
		                              (std::uint32_t(bytes[6]) << 16) | (std::uint32_t(bytes[7]) << 24);
		if (version > 8) {
			detected = true;
			status = "blocked";
			summary = "Cannot load: extended level version " + std::to_string(version);
			rows[1] = { "Level version " + std::to_string(version), "blocked", "Production loader maximum is 8" };
			try {
				const auto v = inspect_level(bytes);
				rows[1].need += ", game data " + std::to_string(v.game_version);
				rows[2].need = std::to_string(v.vertices.size()) + " vertices, " + std::to_string(v.segments.size()) + " segments, " + std::to_string(v.walls.size()) + " walls, " + std::to_string(v.objects.size()) + " objects, " + std::to_string(v.triggers.size()) + " triggers";
				rows[3].need = "Segment functions and owner/group records present";
				rows[4].need = std::to_string(v.walls.size()) + " walls";
				rows[5].need = std::to_string(v.triggers.size()) + " geometry triggers";
				rows[6] = v.object_triggers ? Evidence{ std::to_string(v.object_triggers) + " object triggers", "blocked", "No production implementation" } : Evidence{ "Absent", "not_required", "No object triggers in this level" };
				rows[7].need = std::to_string(v.objects.size()) + " objects; behavior unverified";
				const auto opacity = std::count_if(v.walls.begin(), v.walls.end(), [](const Wall &w) { return w.cloak != 0; });
				rows[16].need = std::to_string(opacity) + " walls with nonzero serialized cloak/opacity";
				rows[16].detail = "Values alone do not establish material semantics or visibility/collision equivalence";
				const auto colored = std::count_if(v.vertex_colors.begin(), v.vertex_colors.end(), [](const Color &c) { return c.rgb[0] != c.rgb[1] || c.rgb[1] != c.rgb[2]; });
				rows[9] = { std::to_string(colored) + " chromatic vertex colors; side and texture color tables present", "blocked", "Preservation in an inspection report is not rendering support" };
				for (const auto &issue : v.unsupported) {
					std::size_t row = 13;
					if (issue.feature.find("segment") != std::string::npos) row = 3;
					else if (issue.feature.find("wall") != std::string::npos) row = 4;
					else if (issue.feature.find("trigger") != std::string::npos) row = 5;
					else if (issue.feature.find("object") != std::string::npos) row = 7;
					else if (issue.feature == "embedded_lighting") row = 9;
					rows[row].status = "blocked";
					rows[row].detail += issue.feature + " at record " + std::to_string(issue.record) + "; ";
				}
			} catch (const std::exception &e) {
				rows[1].detail += "; detailed inspection rejected: " + std::string(e.what());
			}
		}
	} else {
		auto lower_name = name;
		std::transform(lower_name.begin(), lower_name.end(), lower_name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
		if (lower_name.size() >= 4 && lower_name.substr(lower_name.size() - 4) == ".mn2") {
			std::istringstream text(std::string(bytes.begin(), bytes.end()));
			std::string line;
			while (std::getline(text, line)) {
				if (mission_descriptor_key_is(line.c_str(), "d2x-name")) {
					detected = true;
					status = "blocked";
					summary = "Cannot play: XL mission requires level and asset qualification";
					rows[0] = { "d2x-name descriptor", "blocked", "Native title and level-list discovery supported; referenced levels and assets still need qualification" };
				}
			}
		}
	}
	std::ostringstream out;
	out << "{\n  \"schema\": \"dxx-xl-compatibility-v1\",\n  \"source\": " << json_string(name)
	    << ",\n  \"detected\": " << (detected ? "true" : "false") << ",\n  \"status\": " << json_string(status)
	    << ",\n  \"summary\": " << json_string(summary) << ",\n  \"playable\": false,\n  \"rows\": [";
	for (std::size_t i = 0; i < sizeof(categories) / sizeof(categories[0]); ++i) {
		if (i) out << ',';
		out << "\n    {\"id\": " << json_string(categories[i].id) << ", \"feature\": " << json_string(categories[i].label)
		    << ", \"requirement\": " << json_string(rows[i].need) << ", \"support\": " << json_string(categories[i].support)
		    << ", \"status\": " << json_string(rows[i].status) << ", \"evidence\": " << json_string(rows[i].detail) << '}';
	}
	out << "\n  ]\n}\n";
	return out.str();
}
} // namespace xl_compat
