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
Bytes fixture(int version = 15)
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
	put(b, 0, 1);
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
	check(v.vertices.size() == 328 && v.segments.size() == 110 && v.walls.size() == 54 && v.objects.size() == 44, "original Observatory counts differ");
	check(v.mine_offset == 86 && v.game_offset == 43377 && v.sections.at(0).offset == 45741 && v.sections.at(1).offset == 50832, "original Observatory section boundaries differ");
	check(v.triggers.empty() && v.object_triggers == 0 && v.object_trigger_heads.empty() && v.light_indices.empty() && v.delta_lights.empty(), "original Observatory neutral sections differ");
	check(std::count_if(v.segments.begin(), v.segments.end(), [](const auto &segment) { return segment.function == 13; }) == 26, "no-damage properties were dropped");
	check(v.unsupported.size() == 27 && v.unknown.size() == 40, "unexpected original Observatory structural findings");
	for (const auto &issue : v.unsupported)
		check(issue.feature == "embedded_lighting" || (issue.feature == "segment_function" && issue.detail == "13"), "unexpected original Observatory unsupported feature");
	for (const auto &issue : v.unknown)
		check(issue.feature == "segment_flags" || issue.feature == "object_dependencies" || issue.feature == "runtime_semantics" || issue.feature == "assets", "unconsumed original Observatory section data");
	check(std::count_if(v.unknown.begin(), v.unknown.end(), [](const auto &issue) { return issue.feature == "segment_flags"; }) == 37 &&
	          v.segments.at(0).flags == 65 && v.segments.at(4).flags == 202 && v.segments.at(106).flags == 255,
	      "unaudited original segment flags were discarded");
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
