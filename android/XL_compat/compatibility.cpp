#include "compatibility.h"
#include "admission.h"
#include "companion_lighting.h"
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
	const char *explanation;
};
const Category categories[] = {
	{ "container", "Archive and descriptor dialect", "Standard DHF and d2x-name descriptors; gameplay requires supported extended-level features and owned resources; extended HOG unsupported", "Archives package mission files; descriptors name the mission and list its levels. Recognizing the package is separate from loading its contents." },
	{ "serialization", "Level and game-data versions", "OpenGL builds translate bounded 15/33, 17/36 and 23/40 layouts after feature checks", "XL versions change the binary layout of level and game records. A reader must understand each layout before interpreting its contents." },
	{ "capacity", "Table capacities and reference widths", "9000 segments/36000 vertices; 255 walls/1000 objects/100 triggers; XL widening unsupported", "Larger mines can require wider references and more segments, vertices, walls, objects or triggers. A readable file can still exceed engine limits." },
	{ "segments", "Segment physics, properties and ownership", "Classic functions including repair centers exist; XL environment and ownership parity unverified", "Segment properties describe whole volumes, including fluid physics, protected areas and ownership. They are distinct from a wall texture." },
	{ "walls", "Wall types and flags", "Stock wall semantics only; XL extensions unsupported", "Wall types and flags control doors, barriers and other surfaces. Visibility, collision and activation behavior must agree." },
	{ "triggers", "Stateful events and geometry triggers", "Supported wall actions include delayed scheduling and permanent/alternate/autoplay state; other actions, orientation/silent flags and object events remain blocked", "Triggers activate linked actions. XL adds action types, flags and event chains whose live state can matter after save/load." },
	{ "object_triggers", "Object-attached triggers", "Nonplayer leaf events and immediate masters implemented; other variants gated", "These actions are attached to objects rather than only to walls. Their activation rules, owner lifecycle and linked actions are checked separately." },
	{ "objects", "Objects and AI behavior", "Stock behavior only; XL behavior parity unverified", "Level objects include players, robots, pickups and effects. Knowing the object count does not establish its behavior or AI compatibility." },
	{ "gameplay_assets", "Custom robot and weapon definitions", "XL gameplay asset formats and dependencies unverified", "Custom robot and weapon definitions can change behavior and refer to models, animations, projectiles and sounds. Those dependencies must resolve together." },
	{ "baked_lighting", "Embedded colored lighting", "Baked colors and detected method-0 ambient reconstruction implemented in OpenGL builds", "Authored color tables describe lighting at vertices, sides and textures. Reading the numbers is separate from applying them in the renderer." },
	{ "dynamic_lighting", "Dynamic lights and rendering modes", "XL runtime implements vertex lighting and coronas; other rendering modes remain unverified", "Live lighting responds to emitters, weapons and switched lights. Its behavior and presentation need runtime validation." },
	{ "visual_assets", "Texture and model formats", "XL asset formats require individual verification", "Textures and models give the mine its appearance. XL asset formats and their referenced resources need verification beyond level parsing." },
	{ "mission_flow", "Mission flow and scripted behavior", "XL objectives and campaign behavior unverified", "Exits, secret routes and objectives connect gameplay to mission progression. An arena with no exit has different completion criteria from a campaign level." },
	{ "runtime", "Gameplay and presentation verification", "Static inspection is not gameplay qualification; runtime checks dependencies on every load", "Runtime qualification checks actual loading, play and presentation. A successful static inventory is not proof that a level works." },
	{ "navigation", "Teleports, boosts and spawn destinations", "XL movement actions and route-analysis edges unsupported", "Teleports and speed boosts can change position or motion outside ordinary connected passages. Route analysis must account for their authored destinations." },
	{ "generators", "Equipment generators and extended spawning", "Singleplayer robot matcen assignment, timing, activation, health and disk/rewind checked; equipment and object spawning unsupported", "Robot generation uses authored contents and detected source requirements. Expanded gameplay data and capacities remain separate gates; derived controls do not qualify a mission." },
	{ "wall_materials", "Authored wall opacity and materials", "Classic cloaking and texture transparency exist; XL opacity, visibility and collision parity unverified", "Authored opacity changes how surfaces appear. Transparency alone does not prove that ship movement, weapon hits or AI visibility are correct." },
	{ "effect_objects", "Particles, lightning and ambient sounds", "XL payloads, activation and audio cues require individual verification", "Particles, lightning and ambient sound objects add effects to the mine. Their payloads, activation and presentation require separate qualification." },
	{ "resource_scope", "Mission resource precedence and isolation", "Standard HXM/extra-HAM and HMQ paths exist; XL override semantics and transition cleanup unverified", "A mission can override stock resources. The right definitions must load for that mission and be cleared or restored when switching missions." },
	{ "package_identity", "Package identity and resource ownership", "Descriptor ownership and immutable publication exist; HOG aliases and ambiguous wrappers need qualification", "Archive name, mission title, descriptor and selected HOG are different identities. Package resources must belong to the selected mission; a level-only report cannot establish their ownership or absence." },
	{ "scoped_textures", "Mission and level texture folders", "Android XL-enabled mission-owned static TGA lookup is partial; GLES3 merged walls support power-of-two binary cutouts and super-hole masks. Opaque and binary-coverage wall/eclip strips use existing effect timing; door/blastable strips map applied stock frames when source count does not exceed stock count. Color and coverage masks follow the same frame. Partial-alpha overlays are limited to proven opaque bases on full-opacity normal-blend GLES3 walls. Soft base/standalone surfaces, vclips, sub-frame door interpolation and ordinal folders remain unqualified", "Authored textures can live under a mission mod folder or signed level ordinal. Preserving a TGA does not prove selection, decoding, animation or transparency. Exact frames, fallback frames and POG precedence require separate checks." },
	{ "scoped_sounds", "Mission WAV effects and sound banks", "Opt-in imported-D1 WAV path implemented; playback unqualified; scoped banks unsupported", "Sound effects are distinct from music. A mission sound bank can suppress external WAV lookup. Names, scope, sample conversion, playback and cleanup must be verified for the selected game." },
	{ "scoped_music", "Mission playlists and ordinal music", "SNG and nested music paths exist; XL playlist and secret-level precedence unqualified", "Music selection can depend on HOG options, mission playlists and signed level ordinals. Finding or preserving an audio file does not prove that the intended track plays or stops on transition." },
	{ "scoped_models", "Mission models and model skins", "ASE infrastructure exists; scoped aliases and skins unqualified; OOF unsupported", "Model format, model-number mapping and skin ownership are separate requirements. A skin must resolve in the owning model scope rather than borrowing a same-named file from another mission." },
	{ "companion_lighting", "LGT and CLR lighting companions", "Owned level-named LGT overrides feed D2 emitters; 23/40 embedded texture colors replace CLR; other D2 CLR profiles, malformed or foreign companions remain rejected", "Level-named LGT and CLR files describe texture brightness and colors. They are not HAM definitions or proof of embedded level lighting; their values, precedence and renderer use must be checked separately." },
	{ "package_options", "Mission configuration and presentation options", "Packaged INI, wallpapers and extended briefing options unqualified", "Recognizing packaged configuration does not authorize executing it. Each supported option needs bounded parsing, explicit semantics, resource dependencies and restoration on mission changes." },
	{ "distribution_archive", "Distribution archive extraction", "ZIP, 7z and RAR readers exist; codec and original-package extraction require verification", "The downloaded archive is separate from the mission HOG. Recognizing a 7z catalog does not prove its compression methods can be decoded. Extraction must preserve owned paths and pass byte, integrity and collision checks before mission resources can be evaluated." },
};
struct Evidence {
	std::string need = "Unknown - not inspected", status = "unknown", detail;
};
// Counts describe serialized records, not executable route edges or gameplay admission
struct CountedFeature {
	const char *id, *label, *support, *present_status, *explanation;
	const char *limitations = nullptr;
	int count = -1;
};
std::vector<CountedFeature> counted_features()
{
	return {
		{ "wall_teleports", "Wall teleports", "Wall-trigger actions only; object actions are not decoded", "blocked", "Wall-trigger teleports move a ship to an authored destination. Position, orientation and collision behavior need to remain consistent. This count excludes object-attached actions." },
		{ "speed_boosts", "Wall speed boosts", "XL movement actions unsupported", "blocked", "Wall-trigger speed boosts change a ship's motion. Their direction, activation and release rules matter for traversing the mine. This count excludes object-attached actions." },
		{ "cameras", "Wall camera triggers", "XL camera actions unsupported", "blocked", "Wall camera triggers activate authored views. A parsed trigger does not establish that the correct view is displayed. This count excludes object-attached actions." },
		{ "master_triggers", "Wall master triggers", "XL event chains unsupported", "blocked", "Wall master triggers activate linked triggers, allowing chains of actions. Order and saved event state can matter. This count excludes object-attached actions." },
		{ "water_segments", "Water segments", "Experimental hash-pinned arena pilot only; other levels unqualified", "partial", "Water fills a whole segment, not just a watery-looking wall. With fluid physics enabled and normal speed settings, motion runs at 75% speed, including weapons and other moving objects. Player ships and robots experience mild turbulence. Submerged movement has different sound behavior from touching a waterfall. XL can give the volume a translucent blue appearance. Water itself does not inflict fluid damage; lava does.", "Experimental Robotech Arena pilot only: water motion, boundary/collision behavior and native save/replay are validated. Sound distinctions have logic checks; audible playback and full visual parity remain unqualified. Other levels are not qualified" },
		{ "lava_segments", "Lava segments", "XL segment lava semantics unqualified", "blocked", "Lava is a whole-volume fluid property that can inflict damage and alter motion with fluid physics enabled. It is distinct from a lava-looking wall texture." },
		{ "blocked_segments", "Blocked segments", "XL movement restrictions unsupported", "blocked", "Blocked segment properties restrict movement through a volume. The precise object and collision rules need qualification; a count alone does not establish them." },
		{ "no_damage_segments", "No-damage segments", "Singleplayer damage/firing gates, boundary behavior and save restoration implemented; multiplayer and optional persistent-debris policy unsupported", "supported", "Protected segments suppress damage and some firing or direct-contact behavior. They are not simply force-free zones: blast knockback can still occur." },
		{ "colored_walls", "Colored walls", "XL colored-wall semantics unsupported", "blocked", "XL colored walls add authored surface color and transparency behavior. Rendering and physical interaction must be checked separately." },
		{ "opacity_walls", "Wall opacity records", "Experimental pilot; nonzero serialized values are not proof of material parity", "partial", "These walls contain nonzero serialized opacity values. Their presence is not proof of a particular transparency, collision or visibility behavior." },
		{ "chromatic_vertices", "Colored vertices", "Implemented in OpenGL builds; rendered-scene and campaign parity require qualification", "partial", "These vertices contain authored color channels that are not all equal. The count describes stored lighting data, not proof of correctly rendered colors." },
		{ "attached_trigger_count", "Object-attached triggers", "XL object events unsupported", "blocked", "Object-attached triggers hold actions linked to objects. A declared count is not trusted until the corresponding records and references are validated." },
		{ "equipment_count", "Equipment generators", "XL item generation unsupported", "blocked", "Equipment generators spawn authored items. Their contents, activation, timing and persistence are separate from classic robot generators." },
		{ "trigger_switches", "Wall trigger enable/disable actions", "Immediate wall targets implemented; effect-object targets unsupported", "supported", "Enable and disable actions change linked wall triggers, including the raw reference count on master triggers. This count excludes object-attached actions." },
		{ "delayed_wall_actions", "Delayed wall actions", "Positive and randomized delays implemented on supported wall actions", "supported", "Delayed wall actions retain the first activating actor and selected delay. Disabling suspends pending work; permanent actions can rearm after execution." },
		{ "stateful_trigger_flags", "Permanent, alternate and autoplay wall actions", "Supported actions retain flag transitions and scheduling state across saves", "supported", "Permanent actions rearm, alternating actions switch to their opposite after execution, and autoplay actions start from frame processing. Unsupported actions remain blocked independently." },
	};
}
void count_features(std::vector<CountedFeature> &features, const Inventory &v)
{
	const int trigger_types[] = { 14, 15, 16, 31 };
	for (std::size_t i = 0; i < 4; ++i)
		features[i].count = static_cast<int>(std::count_if(v.triggers.begin(), v.triggers.end(), [&](const Trigger &t) { return t.type == trigger_types[i]; }));
	const bool blocked_teleport = std::any_of(v.triggers.begin(), v.triggers.end(), [](const Trigger &t) {
		return t.type == 14 && (t.flags & ~159);
	});
	features[0].present_status = blocked_teleport ? "blocked" : "supported";
	features[0].support = blocked_teleport ? "Wall teleport requires unsupported flags" : "Singleplayer wall teleports include delayed scheduling, destination rotation, actor dispatch and saved state";
	features[0].limitations = "Object-attached actions, orientation/silent flags, speed boosts and multiplayer remain unsupported. Derived controls do not qualify an original mission or campaign";
	const auto chain_blocked = [&](int type) {
		return std::any_of(v.triggers.begin(), v.triggers.end(), [&](const Trigger &t) {
			return t.type == type && ((t.flags & ~159) || std::any_of(t.targets.begin(), t.targets.end(), [](const auto &target) { return target[1] < 0; }));
		});
	};
	features[3].present_status = chain_blocked(31) ? "blocked" : "supported";
	features[3].support = "Singleplayer wall chains preserve link order, delayed scheduling, stateful flags and saved enable counters";
	features[3].limitations = "Orientation/silent flags, object-attached actions and multiplayer remain unsupported. Each child action and resource is checked independently; this does not qualify an original mission";
	features[13].count = static_cast<int>(std::count_if(v.triggers.begin(), v.triggers.end(), [](const Trigger &t) { return t.type == 32 || t.type == 33; }));
	features[13].present_status = chain_blocked(32) || chain_blocked(33) ? "blocked" : "supported";
	features[13].limitations = features[3].limitations;
	features[14].count = static_cast<int>(std::count_if(v.triggers.begin(), v.triggers.end(), [](const Trigger &t) { const auto time = std::abs(std::int64_t(t.time)); return t.type != 25 && t.type != 29 && t.type != 30 && time >= 100 && time <= 900000; }));
	features[15].count = static_cast<int>(std::count_if(v.triggers.begin(), v.triggers.end(), [](const Trigger &t) { return (t.flags & (8 | 16 | 128)) != 0; }));
	for (const auto &trigger : v.triggers) {
		const bool unsupported = (trigger.type > 14 && (trigger.type < 31 || trigger.type > 33)) || (trigger.flags & ~159) || std::any_of(trigger.targets.begin(), trigger.targets.end(), [](const auto &target) { return target[1] < 0; });
		const auto time = std::abs(std::int64_t(trigger.time));
		if (unsupported && trigger.type != 25 && trigger.type != 29 && trigger.type != 30 && time >= 100 && time <= 900000) features[14].present_status = "blocked";
		if (unsupported && (trigger.flags & (8 | 16 | 128))) features[15].present_status = "blocked";
	}
	features[14].limitations = features[15].limitations = features[3].limitations;
	for (std::size_t i = 0; i < 4; ++i) {
		features[i + 4].count = static_cast<int>(std::count_if(v.segments.begin(), v.segments.end(), [&](const Segment &s) {
			return (segment_property_bits(v.level_version, s) & (1 << i)) != 0;
		}));
		if (v.level_version >= 21 && i != 0 && i != 3) {
			features[i + 4].present_status = "blocked";
			features[i + 4].support = "Split segment property detected; runtime translation is not implemented";
		}
	}
	if (v.level_version >= 21) {
		const auto blockers = runtime_blockers(v);
		const bool actors = std::any_of(blockers.begin(), blockers.end(), [](const Issue &issue) { return issue.feature == "water actors"; });
		features[4].present_status = actors ? "blocked" : "supported";
		features[4].support = actors ? "Water with robots or generators is unqualified" : "Modern singleplayer water motion, material boundaries, contact audio and source restoration implemented";
		features[4].limitations = "Robot-free 23/40 profile with default fluid physics and non-volumetric colored boundaries; multiplayer, robots, generators and optional volumetric fog remain unqualified. Derived fixture checks do not qualify an original mission or campaign";
	}
	features[8].count = static_cast<int>(std::count_if(v.walls.begin(), v.walls.end(), [](const Wall &w) { return w.type == 8; }));
	features[9].count = static_cast<int>(std::count_if(v.walls.begin(), v.walls.end(), [](const Wall &w) { return w.cloak != 0; }));
	features[10].count = static_cast<int>(std::count_if(v.vertex_colors.begin(), v.vertex_colors.end(), [](const Color &c) { return c.rgb[0] != c.rgb[1] || c.rgb[1] != c.rgb[2]; }));
	// A count is evidence only after all action records and owner references decode
	if (std::none_of(v.unknown.begin(), v.unknown.end(), [](const Issue &i) { return i.feature.find("object_trigger") == 0; })) features[11].count = v.object_triggers;
	const auto object_blockers = runtime_blockers(v);
	const bool blocked_objects = std::any_of(object_blockers.begin(), object_blockers.end(), [](const Issue &issue) { return issue.feature.find("object") == 0; });
	features[11].present_status = blocked_objects ? "blocked" : "partial";
	features[11].support = "Nonplayer leaf events and immediate masters have native/Android activation and disk/rewind checks";
	features[11].limitations = "Delayed object-chain descendants, other extended actions/flags, player owners and boss teleports remain gated. Native derived controls do not qualify an original mission";
	// Game 33 has no separate equipment-generator section
	features[12].count = 0;
	for (const auto &section : v.sections)
		if (section.name == "equipment") features[12].count = section.count;
}
bool prefix(const Bytes &b, const char *s)
{
	return b.size() >= 3 && b[0] == s[0] && b[1] == s[1] && b[2] == s[2];
}
} // namespace

std::string compatibility_json(const std::string &name, const Bytes &bytes)
{
	Evidence rows[sizeof(categories) / sizeof(categories[0])];
	auto features = counted_features();
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
			rows[1] = { "Level version " + std::to_string(version), "blocked", "Software-renderer builds accept versions 1-8; OpenGL builds require validated layouts and features" };
			try {
				const auto v = inspect_level(bytes);
				count_features(features, v);
				const auto blockers = runtime_blockers(v);
				status = blockers.empty() ? "eligible" : "blocked";
				summary = blockers.empty() ? "Detected features supported by OpenGL builds; assets checked on load, campaign unverified" : blocker_message(blockers);
				rows[1].status = blockers.empty() ? "eligible" : "blocked";
				rows[1].detail = "Admission uses detected requirements, not mission identity";
				rows[1].need += ", game data " + std::to_string(v.game_version);
				rows[2].need = std::to_string(v.vertices.size()) + " vertices, " + std::to_string(v.segments.size()) + " segments, " + std::to_string(v.walls.size()) + " walls, " + std::to_string(v.objects.size()) + " objects, " + std::to_string(v.triggers.size()) + " triggers";
				rows[3].need = "Segment functions and owner/group records present";
				rows[4].need = std::to_string(v.walls.size()) + " walls";
				rows[5].need = std::to_string(v.triggers.size()) + " geometry triggers";
				rows[6] = v.object_triggers ? Evidence{ "Decoded " + std::to_string(v.object_triggers) + " object triggers and owner references", features[11].present_status, features[11].limitations } : Evidence{ "Absent", "not_required", "No object triggers in this level" };
				if (!v.object_triggers && features[11].count < 0) rows[6] = {};
				rows[7].need = std::to_string(v.objects.size()) + " objects; behavior unverified";
				const bool spawning = std::any_of(v.triggers.begin(), v.triggers.end(), [](const Trigger &trigger) { return trigger.type == 26; }) ||
				                      std::any_of(v.object_trigger_records.begin(), v.object_trigger_records.end(), [](const ObjectTrigger &trigger) { return trigger.action.type == 26; });
				rows[15] = { std::to_string(v.matcens.size()) + " robot centers, " + std::to_string(features[12].count) + " equipment generators", spawning || features[12].count ? "blocked" : v.matcens.empty() ? "not_required"
					                                                                                                                                                                                              : "partial",
					         "Default-profile robot matcen controls pass; implicit high-ID selection is algorithm-tested with synthetic definitions, while expanded assets remain gated. Equipment and object spawn actions are unsupported; original-mission qualification is separate" };
				const auto opacity = std::count_if(v.walls.begin(), v.walls.end(), [](const Wall &w) { return w.cloak != 0; });
				rows[16].need = std::to_string(opacity) + " walls with nonzero serialized cloak/opacity";
				rows[16].detail = "Values alone do not establish material semantics or visibility/collision equivalence";
				const auto colored = std::count_if(v.vertex_colors.begin(), v.vertex_colors.end(), [](const Color &c) { return c.rgb[0] != c.rgb[1] || c.rgb[1] != c.rgb[2]; });
				rows[9] = { std::to_string(colored) + " chromatic vertex colors; side and texture color tables present", "eligible", has_registered_colors(v) ? "Registered baked colors retained; this report does not verify the rendered scene" : "No registered baked colors: method-0 ambient reconstruction required and implemented; this report does not verify the rendered scene" };
				for (const auto &issue : blockers) {
					std::size_t row = 13;
					if (issue.feature.find("segment") != std::string::npos) row = 3;
					else if (issue.feature.find("wall") != std::string::npos) row = 4;
					else if (issue.feature.find("trigger") != std::string::npos) row = 5;
					else if (issue.feature.find("object") != std::string::npos) row = 7;
					else if (issue.feature.find("generator") != std::string::npos) row = 15;
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
		const auto extension = lower_name.size() >= 4 ? lower_name.substr(lower_name.size() - 4) : std::string{};
		if (extension == ".lgt" || extension == ".clr") {
			detected = true;
			status = "blocked";
			summary = "Lighting companion requires unqualified runtime rendering support";
			Evidence lighting{ extension == ".lgt" ? "Texture brightness companion" : "Texture RGB companion", "blocked", "" };
			try {
				if (extension == ".lgt") {
					const auto table = decode_texture_brightness(bytes.data(), bytes.size());
					lighting.detail = "910 brightness entries; " + std::to_string(std::count_if(table.begin(), table.end(), [](auto value) { return value != 0; })) + " nonzero";
				} else {
					const auto table = decode_texture_colors(bytes.data(), bytes.size());
					lighting.detail = "910 color entries; " + std::to_string(std::count_if(table.begin(), table.end(), [](const auto &value) { return value.channels[0] != value.channels[1] || value.channels[1] != value.channels[2]; })) + " chromatic";
				}
				lighting.detail += "; decoded layout only. Level association, defaults, rendered effects and restoration require verification";
			} catch (const std::exception &e) {
				summary = "Cannot decode lighting companion: " + std::string(e.what());
				lighting.detail = summary;
			}
			for (std::size_t i = 0; i < sizeof(categories) / sizeof(categories[0]); ++i)
				if (std::string(categories[i].id) == "companion_lighting") rows[i] = lighting;
		}
		if (lower_name.size() >= 4 && lower_name.substr(lower_name.size() - 4) == ".mn2") {
			std::istringstream text(std::string(bytes.begin(), bytes.end()));
			std::string line;
			while (std::getline(text, line)) {
				if (mission_descriptor_key_is(line.c_str(), "d2x-name")) {
					detected = true;
					status = "unknown";
					summary = "XL descriptor detected; inspect referenced levels and assets";
					rows[0] = { "d2x-name descriptor", "unknown", "Native title and level-list discovery supported; verified base initialization in opt-in builds; descriptor gameplay and failed-selection rollback remain guarded" };
				}
			}
		}
	}
	std::ostringstream out;
	out << "{\n  \"schema\": \"dxx-xl-compatibility-v1\",\n  \"source\": " << json_string(name)
	    << ",\n  \"detected\": " << (detected ? "true" : "false") << ",\n  \"status\": " << json_string(status)
	    << ",\n  \"summary\": " << json_string(summary) << ",\n  \"playable\": false,\n  \"blocks_analysis\": " << (detected ? "true" : "false") << ",\n  \"rows\": [";
	for (std::size_t i = 0; i < sizeof(categories) / sizeof(categories[0]); ++i) {
		if (i) out << ',';
		out << "\n    {\"id\": " << json_string(categories[i].id) << ", \"feature\": " << json_string(categories[i].label)
		    << ", \"requirement\": " << json_string(rows[i].need) << ", \"support\": " << json_string(categories[i].support)
		    << ", \"explanation\": " << json_string(categories[i].explanation)
		    << ", \"status\": " << json_string(rows[i].status) << ", \"evidence\": " << json_string(rows[i].detail) << '}';
	}
	for (const auto &feature : features) {
		const std::string state = feature.count < 0 ? "unknown" : feature.count == 0 ? "not_required"
		                                                                             : feature.present_status;
		out << ",\n    {\"id\": " << json_string(feature.id) << ", \"feature\": " << json_string(feature.label)
		    << ", \"count\": " << (feature.count < 0 ? "null" : std::to_string(feature.count))
		    << ", \"requirement\": " << json_string(feature.count < 0 ? "Not inspected" : std::to_string(feature.count))
		    << ", \"support\": " << json_string(feature.support) << ", \"explanation\": " << json_string(feature.explanation) << ", \"limitations\": " << json_string(feature.limitations ? feature.limitations : feature.support) << ", \"status\": " << json_string(state)
		    << ", \"evidence\": " << json_string(feature.count < 0 ? "Requires successful bounded level inspection" : "Serialized record count; does not establish playability") << '}';
	}
	out << "\n  ]\n}\n";
	return out.str();
}
std::string package_compatibility_json(const std::string &owner, const std::string &descriptor,
                                       const std::vector<PackageResource> &resources)
{
	if (owner.empty() || descriptor.empty() || resources.size() > 4096)
		throw std::runtime_error("Invalid owned package catalog");
	std::ostringstream out;
	out << "{\n  \"schema\": \"dxx-xl-compatibility-v1\",\n  \"source\": " << json_string(owner + "/" + descriptor + " (package catalog)")
	    << ",\n  \"detected\": true,\n  \"status\": \"unknown\",\n  \"summary\": \"Owned package files inspected; runtime selection and mission qualification unverified\","
	    << "\n  \"playable\": false,\n  \"blocks_analysis\": false,\n  \"rows\": [";
	bool first = true;
	for (const auto &category : categories) {
		const std::string id = category.id;
		if (id != "package_identity" && id != "scoped_textures" && id != "scoped_sounds" &&
		    id != "scoped_music" && id != "scoped_models" && id != "companion_lighting" && id != "package_options") continue;
		std::string evidence;
		int count = 0;
		for (const auto &resource : resources) {
			if (resource.path.size() > 4096 || resource.sha256.size() != 64)
				throw std::runtime_error("Invalid package resource evidence");
			auto path = resource.path;
			std::transform(path.begin(), path.end(), path.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
			const auto dot = path.find_last_of('.');
			const auto ext = dot == std::string::npos ? std::string{} : path.substr(dot);
			const bool mod = path.rfind("mods/", 0) == 0;
			const bool match =
			    (id == "package_identity" && resource.path == descriptor) ||
			    (id == "scoped_textures" && mod && path.find("/textures/") != std::string::npos) ||
			    (id == "scoped_sounds" && mod && (path.find("/sounds/") != std::string::npos || ext == ".s11" || ext == ".s22")) ||
			    (id == "scoped_music" && (ext == ".sng" || (mod && path.find("/music/") != std::string::npos))) ||
			    (id == "scoped_models" && mod && path.find("/models/") != std::string::npos) ||
			    (id == "companion_lighting" && (ext == ".lgt" || ext == ".clr")) ||
			    (id == "package_options" && mod && (ext == ".ini" || path.find("/wallpapers/") != std::string::npos));
			if (match) {
				++count;
				evidence += resource.path + " [SHA-256 " + resource.sha256 + "]\n";
			}
		}
		evidence += "Owned catalog candidates only; filenames do not establish active HOG identity, precedence, decoding or playback. Embedded archive members are not included in these counts. No candidates does not establish absence of the feature.";
		if (!first) out << ',';
		first = false;
		out << "\n    {\"id\": " << json_string(id) << ", \"feature\": " << json_string(category.label)
		    << ", \"requirement\": " << json_string(std::to_string(count) + " catalogued candidate files")
		    << ", \"support\": " << json_string(category.support) << ", \"explanation\": " << json_string(category.explanation)
		    << ", \"status\": \"unknown\", \"evidence\": " << json_string(evidence) << '}';
	}
	out << "\n  ]\n}\n";
	return out.str();
}
} // namespace xl_compat
