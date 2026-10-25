#include "inventory.h"
#include "probe.h"
#include "compatibility.h"
#include "admission.h"

#include <functional>
#include <fstream>
#include <algorithm>
#include <iostream>
#include <stdexcept>

namespace
{
using xl_compat::Bytes;
void check(bool condition, const char *why)
{
	if (!condition) throw std::runtime_error(why);
}
void put(Bytes &b, std::uint32_t value, int width)
{
	for (int i = 0; i < width; ++i) b.push_back(static_cast<std::uint8_t>(value >> (i * 8)));
}
void patch(Bytes &b, std::size_t offset, std::uint32_t value, int width = 4)
{
	for (int i = 0; i < width; ++i) b.at(offset + i) = static_cast<std::uint8_t>(value >> (i * 8));
}
void rejects(const std::function<void()> &action)
{
	bool rejected = false;
	try {
		action();
	} catch (const std::runtime_error &) {
		rejected = true;
	}
	check(rejected, "malformed input accepted");
}
Bytes fixture(int version = 15, int segment_flags = 0)
{
	Bytes b;
	put(b, 0x504c564c, 4);
	put(b, version, 4);
	put(b, 16, 4);
	put(b, 0, 4);
	b.resize(23);
	put(b, 0, 1);
	put(b, 30, 4);
	put(b, 0xffffffff, 4);
	put(b, 0, 4);
	for (int i = 0; i < 10; ++i) put(b, 0, 4);
	patch(b, 8, static_cast<std::uint32_t>(b.size()));
	put(b, 0, 1);
	put(b, 8, 2);
	put(b, 1, 2);
	for (int i = 0; i < 24; ++i) put(b, i * 65536, 4);
	put(b, 255, 1);
	put(b, 255, 1);
	put(b, 0, 1);
	for (int i = 0; i < 8; ++i) put(b, i, 2);
	put(b, 0, 1);
	for (int side = 0; side < 6; ++side) {
		put(b, side + 1, 2);
		for (int i = 0; i < 12; ++i) put(b, i, 2);
	}
	put(b, 0, 1);
	put(b, 255, 1);
	put(b, 255, 1);
	put(b, segment_flags, 1);
	put(b, 65536, 4);
	for (int i = 0; i < 14; ++i) {
		put(b, 1, 1);
		put(b, 2147483647, 4);
		put(b, 1073741824, 4);
		put(b, 0, 4);
	}
	// Texture colors switch from double to integer components after level 15
	for (int i = 0; i < 910; ++i) {
		put(b, 1, 1);
		if (version == 15) {
			put(b, 0, 4);
			put(b, 0x3ff00000, 4);
			for (int j = 0; j < 4; ++j) put(b, 0, 4);
		} else {
			put(b, 2147483647, 4);
			put(b, 0, 4);
			put(b, 0, 4);
		}
	}
	const auto game = b.size();
	patch(b, 12, static_cast<std::uint32_t>(game));
	put(b, 0x6705, 2);
	put(b, version == 15 ? 33 : 36, 2);
	b.resize(b.size() + 31);
	for (int i = 0; i < (version == 15 ? 9 : 10); ++i) {
		put(b, 0xffffffff, 4);
		put(b, 0, 4);
		put(b, 0, 4);
	}
	put(b, 0, 1);
	put(b, 0, 2);
	patch(b, game + 71, static_cast<std::uint32_t>(b.size()));
	put(b, 0, 4);
	if (version == 17) put(b, 0, 2);
	return b;
}

std::string feature_row(const std::string &json, const char *id)
{
	const auto begin = json.find(std::string("\"id\": \"") + id + "\"");
	check(begin != std::string::npos, "feature row missing");
	return json.substr(begin, json.find('}', begin) - begin);
}
void feature_contract()
{
	auto bytes = fixture();
	auto declared = bytes;
	patch(declared, declared.size() - 4, 1);
	const auto declaration_report = xl_compat::compatibility_json("declaration.rl2", declared);
	check(feature_row(declaration_report, "attached_trigger_count").find("\"count\": null") != std::string::npos, "unvalidated object declaration became inspected count");
	check(declaration_report.find("records unverified") != std::string::npos, "object declaration was not qualified");
	auto nonempty_head = bytes;
	nonempty_head.resize(bytes.size() + 1400, 255);
	patch(nonempty_head, bytes.size(), 0, 2);
	const auto head_report = xl_compat::compatibility_json("head.rl2", nonempty_head);
	check(feature_row(head_report, "attached_trigger_count").find("\"count\": null") != std::string::npos, "nonempty object head reported absent");
	check(feature_row(head_report, "object_triggers").find("\"status\": \"unknown\"") != std::string::npos, "broad object row reported false absence");
	const auto inventory = xl_compat::inspect_level(bytes);
	const auto game = inventory.game_offset;
	// Append three wall teleports plus one boost and one camera, with neutral targets
	bytes.resize(bytes.size() - 4);
	for (const int type : { 14, 14, 14, 15, 16 }) {
		put(bytes, type, 1);
		bytes.resize(bytes.size() + 51);
	}
	put(bytes, 0, 4);
	patch(bytes, game + 75, 5);
	auto json = xl_compat::compatibility_json("features.rl2", bytes);
	auto row = feature_row(json, "wall_teleports");
	check(row.find("\"count\": 3") != std::string::npos && row.find("\"status\": \"blocked\"") != std::string::npos, "wall teleports not counted or falsely supported");
	check(json.find("Wall speed boosts") != std::string::npos && json.find("Wall camera triggers") != std::string::npos && json.find("Wall master triggers") != std::string::npos, "action counts lost wall-only scope");
	check(feature_row(json, "speed_boosts").find("\"count\": 1") != std::string::npos, "boost count");
	check(feature_row(json, "master_triggers").find("\"status\": \"not_required\"") != std::string::npos, "absent feature not distinct");
	bytes.at(inventory.mine_offset + 277) = 7;
	row = feature_row(xl_compat::compatibility_json("water.rl2", bytes), "water_segments");
	check(row.find("\"count\": 1") != std::string::npos && row.find("\"status\": \"partial\"") != std::string::npos, "water pilot generalized or missing");
	check(row.find("75%") != std::string::npos && row.find("fluid physics enabled") != std::string::npos && row.find("whole segment") != std::string::npos, "water explanation lost its conditional volume semantics");
	check(row.find("native save/replay") != std::string::npos && row.find("audible playback and full visual parity remain unqualified") != std::string::npos, "water explanation overclaims qualification");
	check(feature_row(json, "wall_teleports").find("excludes object-attached actions") != std::string::npos, "teleport explanation lost count scope");
	check(feature_row(json, "serialization").find("binary layout") != std::string::npos, "broad category has no explanation");
	check(json.find("\"explanation\": \"\"") == std::string::npos, "catalog emitted an empty feature explanation");
	for (const char *id : { "package_identity", "scoped_textures", "scoped_sounds", "scoped_music", "scoped_models", "companion_lighting", "package_options", "distribution_archive" }) {
		const auto package_row = feature_row(json, id);
		check(package_row.find("\"status\": \"unknown\"") != std::string::npos, "level-only report inferred package resource support or absence");
		check(package_row.find("Unknown - not inspected") != std::string::npos, "package evidence missing its inspection boundary");
	}
	bytes.pop_back();
	row = feature_row(xl_compat::compatibility_json("broken.rl2", bytes), "wall_teleports");
	check(row.find("\"count\": null") != std::string::npos, "failed parse leaked partial counts");
	patch(bytes, 4, 23);
	row = feature_row(xl_compat::compatibility_json("newer.rl2", bytes), "wall_teleports");
	check(row.find("\"count\": null") != std::string::npos, "unsupported version reported false zero");
}

void version_17_contract()
{
	const auto bytes = fixture(17);
	const auto inventory = xl_compat::inspect_level(bytes);
	check(inventory.level_version == 17 && inventory.game_version == 36 && inventory.sections.size() == 10, "17/36 header layout");
	check(inventory.sections.back().name == "equipment" && inventory.sections.back().count == 0, "equipment section header lost");
	check(inventory.texture_colors.at(909).rgb[0] == 1 && inventory.texture_colors.at(909).rgb[1] == 0, "17 integer texture colors");
	check(inventory.object_trigger_heads.empty(), "empty sparse references changed inventory");
	auto no_tail = bytes;
	no_tail.resize(no_tail.size() - 2);
	check(xl_compat::inspect_level(no_tail).object_trigger_heads.empty(), "omitted neutral sparse-reference tail rejected");
	rejects([&] { xl_compat::prepare_gameplay_probe(bytes); });
	const auto compatibility = xl_compat::compatibility_json("fixture.rl2", bytes);
	check(compatibility.find("\"status\": \"blocked\"") != std::string::npos && compatibility.find("\"playable\": false") != std::string::npos, "17 inspection granted production admission");
	auto bad = bytes;
	patch(bad, inventory.game_offset + 2, 33, 2);
	rejects([&] { xl_compat::inspect_level(bad); });
	bad = bytes;
	bad.pop_back();
	rejects([&] { xl_compat::inspect_level(bad); });
	bad = bytes;
	patch(bad, bad.size() - 2, 1, 2);
	rejects([&] { xl_compat::inspect_level(bad); });
	bad = bytes;
	bad.push_back(0);
	rejects([&] { xl_compat::inspect_level(bad); });
	bad = bytes;
	patch(bad, inventory.game_offset + 35 + 9 * 12, inventory.game_offset);
	patch(bad, inventory.game_offset + 39 + 9 * 12, 1);
	rejects([&] { xl_compat::inspect_level(bad); });
	auto equipment = bytes;
	patch(equipment, inventory.game_offset + 35 + 9 * 12, static_cast<std::uint32_t>(equipment.size()));
	patch(equipment, inventory.game_offset + 39 + 9 * 12, 1);
	equipment.resize(equipment.size() + 20);
	const auto with_equipment = xl_compat::inspect_level(equipment);
	rejects([&] { xl_compat::prepare_serialization_probe_17_36(equipment); });
	check(std::any_of(with_equipment.unsupported.begin(), with_equipment.unsupported.end(), [](const auto &issue) { return issue.feature == "equipment_generators"; }), "equipment payload silently dropped");
	equipment.pop_back();
	rejects([&] { xl_compat::inspect_level(equipment); });

	auto lights = bytes;
	const auto index_offset = lights.size();
	patch(lights, inventory.game_offset + 35 + 7 * 12, static_cast<std::uint32_t>(index_offset));
	patch(lights, inventory.game_offset + 39 + 7 * 12, 1);
	put(lights, 0, 2);
	put(lights, (300 << 3) | 5, 2);
	put(lights, 0, 2);
	patch(lights, inventory.game_offset + 35 + 8 * 12, static_cast<std::uint32_t>(lights.size()));
	patch(lights, inventory.game_offset + 39 + 8 * 12, 300);
	for (int i = 0; i < 300; ++i) {
		put(lights, 0, 2);
		put(lights, 4, 1);
		put(lights, 0, 1);
		for (int corner = 0; corner < 4; ++corner) put(lights, corner + 1, 1);
	}
	const auto decoded = xl_compat::inspect_level(lights);
	check(decoded.light_indices.at(0) == std::array<int, 4>{ 0, 5, 300, 0 } && decoded.delta_lights.size() == 300, "packed light count narrowed or side misdecoded");
	bad = lights;
	patch(bad, index_offset + 2, (300 << 3) | 7, 2);
	rejects([&] { xl_compat::inspect_level(bad); });
	bad = lights;
	patch(bad, index_offset + 2, (8191 << 3) | 5, 2);
	rejects([&] { xl_compat::inspect_level(bad); });
	lights.pop_back();
	rejects([&] { xl_compat::inspect_level(lights); });
}

void light_count_contract()
{
	for (const int count : { 0, 1, 127, 128, 255 }) {
		auto bytes = fixture();
		const auto game = xl_compat::inspect_level(bytes).game_offset;
		patch(bytes, game + 35 + 7 * 12, static_cast<std::uint32_t>(bytes.size()));
		patch(bytes, game + 39 + 7 * 12, 1);
		patch(bytes, game + 43 + 7 * 12, 6);
		put(bytes, 0, 2);
		put(bytes, 5, 1);
		put(bytes, count, 1);
		put(bytes, 0, 2);
		patch(bytes, game + 35 + 8 * 12, static_cast<std::uint32_t>(bytes.size()));
		patch(bytes, game + 39 + 8 * 12, count);
		patch(bytes, game + 43 + 8 * 12, 8);
		for (int i = 0; i < count; ++i) {
			put(bytes, 0, 2);
			put(bytes, 4, 1);
			put(bytes, 0, 1);
			for (int corner = 0; corner < 4; ++corner) put(bytes, corner + 1, 1);
		}
		const auto inventory = xl_compat::inspect_level(bytes);
		check(inventory.light_indices.at(0)[2] == count, "source light count was narrowed");
		check(xl_compat::runtime_blockers(inventory).empty(), "losslessly representable light count rejected");
		const auto translated = xl_compat::prepare_gameplay_probe(bytes).stock_level;
		const auto read = [&](std::size_t offset, int width) {
			unsigned value = 0;
			for (int i = 0; i < width; ++i) value |= unsigned(translated.at(offset + i)) << (8 * i);
			return value;
		};
		const auto translated_game = read(12, 4);
		const auto table = read(translated_game + 35 + 7 * 12, 4);
		const auto records = read(translated_game + 39 + 7 * 12, 4);
		check(records == unsigned(count ? (count + 126) / 127 : 1), "translated light table size differs");
		int covered = 0;
		for (unsigned i = 0; i < records; ++i) {
			const auto record = table + i * 6;
			check(read(record, 2) == 0 && read(record + 2, 1) == 5 && read(record + 4, 2) == unsigned(covered), "split light source or target order changed");
			const auto chunk = read(record + 3, 1);
			check(chunk <= 127 && (chunk || !count), "split light count is not representable");
			covered += chunk;
		}
		check(covered == count, "split light targets lost");
		if (count == 128) {
			for (const int copies : { 250, 251 }) {
				auto crowded = bytes;
				const auto index_offset = inventory.sections.at(7).offset;
				const Bytes record(bytes.begin() + index_offset, bytes.begin() + index_offset + 6);
				for (int i = 1; i < copies; ++i)
					crowded.insert(crowded.begin() + index_offset, record.begin(), record.end());
				patch(crowded, game + 39 + 7 * 12, copies);
				patch(crowded, game + 35 + 8 * 12, inventory.sections.at(8).offset + (copies - 1) * 6);
				if (copies == 250) xl_compat::prepare_gameplay_probe(crowded);
				else {
					const auto message = xl_compat::blocker_message(xl_compat::runtime_blockers(xl_compat::inspect_level(crowded)));
					check(message.find("more than 500 light indices") != std::string::npos, "expanded capacity rejection missing");
					rejects([&] { xl_compat::prepare_gameplay_probe(crowded); });
					check(xl_compat::compatibility_json("lights.rl2", crowded).find("\"status\": \"blocked\"") != std::string::npos, "browser admitted expanded light overflow");
				}
			}
		}
	}
}

void serialization_17_contract()
{
	for (const int count : { 0, 1, 31, 32, 127, 128, 255, 300, 8191 }) {
		auto bytes = fixture(17, 0xfc);
		const auto game = xl_compat::inspect_level(bytes).game_offset;
		const auto read = [](const Bytes &data, std::size_t offset, int width = 4) {
			unsigned value = 0;
			for (int i = 0; i < width; ++i) value |= unsigned(data.at(offset + i)) << (i * 8);
			return value;
		};
		// An actual name must follow the retained nine-section header
		bytes.insert(bytes.begin() + game + 155, { 'L', '1', '7' });
		patch(bytes, game + 71, read(bytes, game + 71) + 3);
		patch(bytes, game + 27, read(bytes, game + 71));
		patch(bytes, game + 35 + 7 * 12, static_cast<unsigned>(bytes.size()));
		patch(bytes, game + 39 + 7 * 12, 1);
		put(bytes, 0, 2);
		put(bytes, (count << 3) | 5, 2);
		put(bytes, 0, 2);
		patch(bytes, game + 35 + 8 * 12, static_cast<unsigned>(bytes.size()));
		patch(bytes, game + 39 + 8 * 12, count);
		for (int i = 0; i < count; ++i) {
			put(bytes, 0, 2);
			put(bytes, 4, 1);
			put(bytes, 0, 1);
			for (int corner = 0; corner < 4; ++corner) put(bytes, corner + 1, 1);
		}
		const auto source = xl_compat::inspect_level(bytes);
		const auto converted = xl_compat::prepare_serialization_probe_17_36(bytes);
		const auto &out = converted.stock_level;
		const auto output_game = read(out, 12);
		check(read(out, 4) == 8 && read(out, output_game + 2, 2) == 32 && read(out, output_game + 4) == 143, "17/36 output header differs");
		check(std::string(out.begin() + output_game + 143, out.begin() + output_game + 146) == "L17", "equipment descriptor displaced level name");
		check(converted.source.segments[0].flags == 0xfc, "layout qualification masked reserved source bits");
		for (const auto field : { 27, 71, 35 + 8 * 12 })
			check(read(out, output_game + field) == read(bytes, game + field) - game + output_game - 12, "retained section offset not rebased");
		const auto delta = read(out, output_game + 35 + 8 * 12);
		check(std::equal(bytes.begin() + source.sections[8].offset, bytes.end(), out.begin() + delta), "delta payload changed");
		const auto table = read(out, output_game + 35 + 7 * 12), records = read(out, output_game + 39 + 7 * 12);
		check(records == unsigned(count ? (count + 126) / 127 : 1), "17/36 light split count differs");
		unsigned covered = 0;
		for (unsigned i = 0; i < records; ++i) {
			check(read(out, table + i * 6, 2) == 0 && read(out, table + i * 6 + 2, 1) == 5 &&
			          read(out, table + i * 6 + 4, 2) == covered,
			      "17/36 light side or target order changed");
			const auto chunk = read(out, table + i * 6 + 3, 1);
			check(chunk <= 127 && (chunk || !count), "17/36 light count unrepresentable");
			covered += chunk;
		}
		check(covered == unsigned(count), "17/36 light targets lost");
		rejects([&] { xl_compat::prepare_gameplay_probe(bytes); });
		bytes.at(source.mine_offset + 277) = 13;
		rejects([&] { xl_compat::prepare_serialization_probe_17_36(bytes); });
	}
	rejects([&] { xl_compat::prepare_serialization_probe_17_36(fixture()); });
	std::cout << "17/36 layout qualification: header removal, retained offsets, packed counts 0..8191; runtime and no-damage remain blocked\n";
}

void segment_flags_contract()
{
	for (int flags = 0; flags < 256; ++flags) {
		const auto bytes = fixture(17, flags);
		const auto v = xl_compat::inspect_level(bytes);
		check(v.segments.at(0).flags == flags, "17/36 raw segment flags changed");
		check(v.observations.size() == (flags & 0xfc ? 1u : 0u), "reserved-bit observation count");
		if (flags & 0xfc) {
			check(v.observations[0].feature == "reserved_segment_flags" && v.observations[0].detail == std::to_string(flags), "reserved flags lost source value");
			check(xl_compat::inventory_json(v).find("reserved_segment_flags") != std::string::npos, "reserved flags omitted from report");
		}
		const auto blockers = xl_compat::runtime_blockers(v);
		check(std::any_of(blockers.begin(), blockers.end(), [](const auto &i) { return i.feature == "serialization"; }), "flag audit admitted unsupported serialization");
		rejects([&] { xl_compat::prepare_gameplay_probe(bytes); });
	}
	const auto legacy_bytes = fixture(15, 255);
	const auto legacy = xl_compat::inspect_level(legacy_bytes);
	check(legacy.observations.empty() && std::any_of(legacy.unknown.begin(), legacy.unknown.end(), [](const auto &i) { return i.feature == "segment_flags"; }), "17/36 audit widened another profile");
	rejects([&] { xl_compat::prepare_gameplay_probe(legacy_bytes); });
}

void original_observatory(const char *path)
{
	std::ifstream input(path, std::ios::binary | std::ios::ate);
	check(input && input.tellg() == 82014, "original Observatory HOG size differs");
	input.seekg(0);
	Bytes bytes(82014);
	input.read(reinterpret_cast<char *>(bytes.data()), bytes.size());
	check(input && input.peek() == std::char_traits<char>::eof() && !input.bad(), "original Observatory HOG read failed");
	const auto members = xl_compat::read_hog(bytes);
	const auto level = std::find_if(members.begin(), members.end(), [](const auto &member) { return member.name == "obsrv-xl.rl2"; });
	check(level != members.end() && level->bytes.size() == 52176, "original Observatory level missing");
	const auto v = xl_compat::inspect_level(level->bytes);
	const auto report = xl_compat::compatibility_json("obsrv-xl.rl2", level->bytes);
	check(feature_row(report, "no_damage_segments").find("\"count\": 26") != std::string::npos, "original no-damage UI count");
	check(feature_row(report, "wall_teleports").find("\"count\": 0") != std::string::npos, "original teleport UI count");
	check(feature_row(report, "opacity_walls").find("\"count\": 41") != std::string::npos, "original opacity UI count");
	check(v.vertices.size() == 328 && v.segments.size() == 110 && v.walls.size() == 54 && v.objects.size() == 44, "original Observatory counts differ");
	check(v.mine_offset == 86 && v.game_offset == 43377 && v.sections.at(0).offset == 45741 && v.sections.at(1).offset == 50832, "original Observatory section boundaries differ");
	check(v.triggers.empty() && v.object_triggers == 0 && v.object_trigger_heads.empty() && v.light_indices.empty() && v.delta_lights.empty(), "original Observatory neutral sections differ");
	check(std::count_if(v.segments.begin(), v.segments.end(), [](const auto &segment) { return segment.function == 13; }) == 26, "no-damage properties were dropped");
	check(v.unsupported.size() == 27 && v.unknown.size() == 3 && v.observations.size() == 37, "unexpected original Observatory structural findings");
	for (const auto &issue : v.unsupported)
		check(issue.feature == "embedded_lighting" || (issue.feature == "segment_function" && issue.detail == "13"), "unexpected original Observatory unsupported feature");
	for (const auto &issue : v.unknown)
		check(issue.feature == "object_dependencies" || issue.feature == "runtime_semantics" || issue.feature == "assets", "unconsumed original Observatory section data");
	check(std::count_if(v.observations.begin(), v.observations.end(), [](const auto &issue) { return issue.feature == "reserved_segment_flags"; }) == 37 &&
	          v.segments.at(0).flags == 65 && v.segments.at(4).flags == 202 && v.segments.at(106).flags == 255,
	      "audited reserved segment flags were discarded");
	rejects([&] { xl_compat::prepare_gameplay_probe(level->bytes); });
	std::cout << "Original Observatory 17/36 inventory passed; 26 no-damage segments retained; gameplay remains blocked\n";
}
} // namespace

int main(int argc, char **argv)
{
	try {
		check(argc == 1 || argc == 2, "expected optional original Observatory HOG path");
		if (argc == 2) original_observatory(argv[1]);
		version_17_contract();
		serialization_17_contract();
		feature_contract();
		segment_flags_contract();
		light_count_contract();
		const auto bytes = fixture();
		const auto out = xl_compat::inspect_level(bytes);
		const std::size_t mine = bytes[8];
		auto legacy = bytes;
		legacy.at(mine + 103) = 64;
		check(xl_compat::inventory_json(xl_compat::inspect_level(legacy)) == xl_compat::inventory_json(out), "ignored legacy child marker changed inventory");
		legacy.at(mine + 103) = 128;
		rejects([&] { xl_compat::inspect_level(legacy); });
		auto water = bytes;
		water.at(mine + 277) = 7;
		rejects([&] { xl_compat::prepare_gameplay_probe(water); });
		const auto water_probe = xl_compat::prepare_gameplay_probe(water, true);
		check(water_probe.source.segments.at(0).function == 7, "water source property lost");
		check(water_probe.stock_level == xl_compat::prepare_gameplay_probe(bytes).stock_level, "water leaked into stock segment enum");
		water.at(mine + 277) = 8;
		rejects([&] { xl_compat::prepare_gameplay_probe(water, true); });

		const auto compatibility = xl_compat::compatibility_json("pilot.rl2", bytes);
		const std::string hash(64, 'a');
		const auto package = xl_compat::package_compatibility_json("owner", "missions/pilot.msn", { { "missions/pilot.msn", hash }, { "mods/pilot/sounds/BOSS01.WAV", hash }, { "mods/pilot/pilot.s22", hash }, { "mods/pilot/textures/rock.tga", hash }, { "missions/pilot.hog", hash } });
		check(package.find("\"blocks_analysis\": false") != std::string::npos && package.find("\"playable\": false") != std::string::npos,
		      "package evidence must not block analysis or imply playability");
		const auto sound_row = feature_row(package, "scoped_sounds");
		check(sound_row.find("2 catalogued candidate files") != std::string::npos && sound_row.find("BOSS01.WAV") != std::string::npos &&
		          sound_row.find("pilot.s22") != std::string::npos && sound_row.find(hash) != std::string::npos,
		      "owned WAV and bank candidates must preserve source evidence without selecting a winner");
		check(feature_row(package, "companion_lighting").find("\"status\": \"unknown\"") != std::string::npos,
		      "uninspected HOG companions must not be reported absent");
		check(package.find("\"status\": \"eligible\"") == std::string::npos, "catalog candidates gained eligibility");
		check(compatibility.find("\"status\": \"eligible\"") != std::string::npos, "supported features should be eligible for runtime validation");
		check(compatibility.find("\"playable\": false") != std::string::npos, "compatibility must remain nonplayable");
		check(compatibility.find("Object-attached triggers") != std::string::npos, "catalog missing category");
		for (const auto id : { "navigation", "generators", "wall_materials", "effect_objects", "resource_scope" }) {
			const auto begin = compatibility.find(std::string("\"id\": \"") + id + "\"");
			check(begin != std::string::npos, "survey category missing");
			const auto row = compatibility.substr(begin, compatibility.find('}', begin) - begin);
			check(row.find("\"status\": \"unknown\"") != std::string::npos, "unverified survey category gained support or false absence");
		}
		check(xl_compat::compatibility_json("base.hog", { 'D', '2', 'X' }).find("\"detected\": true") != std::string::npos, "extended HOG not detected");
		check(xl_compat::compatibility_json("D2X-XL stock.rl2", {}).find("\"detected\": false") != std::string::npos, "filename used as format evidence");
		const std::string descriptor = " d2x-name = mission\n";
		check(xl_compat::compatibility_json("mission.mn2", Bytes(descriptor.begin(), descriptor.end())).find("\"detected\": true") != std::string::npos, "XL descriptor not detected");
		const auto descriptor_report = xl_compat::compatibility_json("mission.mn2", Bytes(descriptor.begin(), descriptor.end()));
		check(descriptor_report.find("Native title and level-list discovery supported") != std::string::npos &&
		          descriptor_report.find("\"playable\": false") != std::string::npos,
		      "descriptor discovery must not imply gameplay admission");
		const std::string invalid_key = "d2x -name = mission\n";
		check(xl_compat::compatibility_json("mission.mn2", Bytes(invalid_key.begin(), invalid_key.end())).find("\"detected\": false") != std::string::npos,
		      "descriptor detection must use the native exact-key rules");
		check(out.vertices.size() == 8 && out.segments.size() == 1, "geometry counts");
		check(out.segments[0].sides[5].texture == 6 && out.segments[0].sides[5].uvl[11] == 11, "side layout");
		check(out.vertex_colors[0].rgb[0] == 1 && out.vertex_colors[0].rgb[2] == 0, "integer colors");
		check(out.texture_colors[909].rgb[0] == 1 && out.texture_colors[909].rgb[2] == 0, "double colors");
		check(!out.unknown.empty() && !out.unsupported.empty(), "inspection must not imply admission");
		check(xl_compat::inventory_json(out).find("\"playable\": false") != std::string::npos, "explicit non-playable report");
		const auto probe = xl_compat::prepare_gameplay_probe(bytes);
		check(probe.stock_level[4] == 8 && probe.stock_level.size() < bytes.size(), "probe stock envelope");
		check(probe.source.vertex_colors.size() == 8 && probe.source.texture_colors.size() == 910, "probe must retain lighting evidence");
		check(xl_compat::runtime_blockers(probe.source).empty(), "implemented XL features rejected");
		auto unsupported = probe.source;
		unsupported.unsupported.push_back({ "trigger_type", 2, "teleport action is not implemented" });
		check(xl_compat::blocker_message(xl_compat::runtime_blockers(unsupported)).find("teleport") != std::string::npos, "missing actionable trigger rejection");
		unsupported = probe.source;
		unsupported.unknown.push_back({ "unrecognized_feature", 5, "unknown serialized semantics" });
		check(!xl_compat::runtime_blockers(unsupported).empty(), "unknown feature silently admitted");
		unsupported = probe.source;
		unsupported.triggers.resize(101);
		check(!xl_compat::runtime_blockers(unsupported).empty(), "runtime trigger capacity ignored");
		for (const auto n : { std::size_t(0), std::size_t(15), std::size_t(20), bytes.size() - 1 }) {
			rejects([&] { xl_compat::inspect_level(Bytes(bytes.begin(), bytes.begin() + n)); });
		}
		auto bad = bytes;
		patch(bad, 4, 16);
		rejects([&] { xl_compat::inspect_level(bad); });
		bad = bytes;
		patch(bad, 12, 8);
		rejects([&] { xl_compat::inspect_level(bad); });
		bad = bytes;
		patch(bad, out.game_offset + 2, 34, 2);
		rejects([&] { xl_compat::inspect_level(bad); });
		bad = bytes;
		patch(bad, out.mine_offset + 5 + 96 + 3, 8, 2);
		rejects([&] { xl_compat::inspect_level(bad); });
		bad = bytes;
		patch(bad, out.game_offset + 75, 1000000);
		rejects([&] { xl_compat::inspect_level(bad); });
		bad = bytes;
		patch(bad, out.mine_offset + 5 + 96, 1, 1);
		const auto ownership = xl_compat::inspect_level(bad);
		check(ownership.unknown[0].feature == "segment_owner_group", "ownership silently ignored");
		rejects([&] { xl_compat::prepare_gameplay_probe(bad); });
		Bytes hog{ 'D', 'H', 'F' };
		const std::string name = "level.rl2";
		hog.insert(hog.end(), name.begin(), name.end());
		hog.resize(16);
		put(hog, static_cast<std::uint32_t>(bytes.size()), 4);
		hog.insert(hog.end(), bytes.begin(), bytes.end());
		const auto members = xl_compat::read_hog(hog);
		check(members.size() == 1 && members[0].bytes == bytes, "HOG round trip");
		hog.pop_back();
		rejects([&] { xl_compat::read_hog(hog); });
		hog[2] = 'X';
		rejects([&] { xl_compat::read_hog(hog); });
		std::cout << "XL inventory contracts passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
