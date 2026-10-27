#include "inventory.h"
#include "probe.h"
#include "compatibility.h"
#include "admission.h"

#include <functional>
#include <fstream>
#include <filesystem>
#include <algorithm>
#include <cstring>
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
Bytes fixture(int version = 15, int segment_flags = 0, int segment_count = 1)
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
	put(b, segment_count, 2);
	for (int i = 0; i < 24; ++i) put(b, i * 65536, 4);
	for (int segment = 0; segment < segment_count; ++segment) {
		put(b, 255, 1);
		put(b, 255, 1);
		put(b, 0, 1);
		for (int i = 0; i < 8; ++i) put(b, i, 2);
		put(b, 0, 1);
		for (int side = 0; side < 6; ++side) {
			put(b, side + 1, 2);
			for (int i = 0; i < 12; ++i) put(b, i, 2);
		}
	}
	for (int segment = 0; segment < segment_count; ++segment) {
		put(b, 0, 1);
		put(b, 255, 1);
		put(b, 255, 1);
		put(b, segment_flags, 1);
		if (version == 23) b.resize(b.size() + 5);
		put(b, 65536, 4);
	}
	for (int i = 0; i < 8 + segment_count * 6; ++i) {
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
	put(b, version == 15 ? 33 : version == 23 ? 40
	                                          : 36,
	    2);
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

std::string feature_row(const std::string &json, const char *id);

void generator_assignment_contract(const char *directory = nullptr)
{
	for (int version : { 15, 17, 23 }) {
		auto bytes = fixture(version, 0, 5);
		const auto base = xl_compat::inspect_level(bytes);
		const auto properties = base.mine_offset + 5 + 8 * 12 + 5 * 176;
		const int stride = version == 23 ? 13 : 8;
		for (const auto &entry : std::array<std::array<int, 3>, 4>{ { { 0, 4, 2 }, { 1, 1, -1 }, { 2, 4, 0 }, { 3, 4, 1 } } }) {
			patch(bytes, properties + entry[0] * stride, entry[1], 1);
			patch(bytes, properties + entry[0] * stride + 1, entry[2], 1);
		}
		patch(bytes, base.game_offset + 35 + 6 * 12, static_cast<unsigned>(bytes.size()));
		patch(bytes, base.game_offset + 39 + 6 * 12, 3);
		patch(bytes, base.game_offset + 43 + 6 * 12, 20);
		for (int row = 0; row < 3; ++row) {
			put(bytes, 1u << row, 4);
			put(bytes, 0x80000000u >> row, 4);
			put(bytes, (row + 1) * 65536, 4);
			put(bytes, (row + 4) * 65536, 4);
			put(bytes, 0xffff, 2);
			put(bytes, 32767, 2);
		}
		const auto parsed = xl_compat::inspect_level(bytes);
		check(parsed.matcens.size() == 3 && parsed.matcens[0][4] == -1 && parsed.matcens[0][5] == 32767, "authored generator fields normalized during inspection");
		check(feature_row(xl_compat::compatibility_json("generators.rl2", bytes), "generators").find("\"status\": \"partial\"") != std::string::npos, "bounded matcens incorrectly claim complete generator support");
		const auto translated = xl_compat::prepare_gameplay_probe(bytes).stock_level;
		const auto read = [&](std::size_t offset, unsigned width = 4) {
			std::uint32_t value = 0;
			for (unsigned i = 0; i < width; ++i) value |= std::uint32_t(translated.at(offset + i)) << (i * 8);
			return value;
		};
		const auto records = read(read(12) + 35 + 6 * 12);
		for (int native = 0; native < 3; ++native) {
			const int source = std::array<int, 3>{ 2, 0, 1 }[native];
			for (int field = 0; field < 4; ++field)
				check(read(records + native * 20 + field * 4) == std::uint32_t(parsed.matcens[source][field]), "XL source-to-native generator contents swapped");
			check(read(records + native * 20 + 16, 2) == unsigned(native ? native + 1 : 0), "translated generator segment differs");
			check(read(records + native * 20 + 18, 2) == unsigned(native ? native + 1 : 0), "translated generator station differs");
		}
		if (directory) {
			std::filesystem::create_directories(directory);
			std::ofstream output(std::filesystem::path(directory) / ("generators-" + std::to_string(version) + ".rl2"), std::ios::binary);
			output.write(reinterpret_cast<const char *>(bytes.data()), bytes.size());
		}
		for (int malformed = 0; malformed < 4; ++malformed) {
			auto invalid = bytes;
			const auto offset = properties + (malformed == 3 ? 4 : 3) * stride;
			if (malformed == 0) patch(invalid, offset + 1, 0, 1);
			if (malformed == 1) patch(invalid, offset + 1, 255, 1);
			if (malformed == 2) patch(invalid, offset + 1, 3, 1);
			if (malformed == 3) {
				patch(invalid, offset, 4, 1);
				patch(invalid, offset + 1, 1, 1);
			}
			const auto blockers = xl_compat::runtime_blockers(xl_compat::inspect_level(invalid));
			check(xl_compat::blocker_message(blockers).find("generator assignment") != std::string::npos, "invalid generator assignment lacks useful rejection");
			check(feature_row(xl_compat::compatibility_json("generators.rl2", invalid), "generators").find("\"status\": \"blocked\"") != std::string::npos, "generator browser category hides assignment blocker");
			rejects([&] { xl_compat::prepare_gameplay_probe(invalid); });
		}
	}
	std::cout << "XL generator assignment: three layouts, source permutation, ignored authored links, station mapping and malformed controls passed\n";
}

std::string feature_row(const std::string &json, const char *id)
{
	const auto begin = json.find(std::string("\"id\": \"") + id + "\"");
	check(begin != std::string::npos, "feature row missing");
	return json.substr(begin, json.find('}', begin) - begin);
}
void teleport_inventory_contract()
{
	for (const int version : { 15, 17, 23 }) {
		auto bytes = fixture(version);
		const auto inventory = xl_compat::inspect_level(bytes);
		const auto section = std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const xl_compat::Section &s) { return s.name == "triggers"; });
		check(section != inventory.sections.end(), "missing fixture trigger section");
		const auto offset = static_cast<std::size_t>(section->offset);
		bytes.insert(bytes.begin() + offset, 52, 0);
		// Only the trigger section has a payload in this fixture
		const auto game = inventory.game_offset;
		patch(bytes, game + 75, 1);
		patch(bytes, offset, 14, 1);
		patch(bytes, offset + 2, 1, 1);
		patch(bytes, offset + 8, 0xffffffff);
		check(xl_compat::inspect_level(bytes).triggers.size() == 1, "teleport record lost");
		check(xl_compat::runtime_blockers(xl_compat::inspect_level(bytes)).empty(), "immediate teleport blocked by feature admission");
		auto boost = bytes;
		patch(boost, offset, 15, 1);
		check(!xl_compat::runtime_blockers(xl_compat::inspect_level(boost)).empty(), "teleport support admitted speed boost");
		patch(boost, offset + 8, 1000);
		patch(boost, offset + 1, 8, 1);
		const auto blocked_timing = xl_compat::compatibility_json("boost.rl2", boost);
		check(feature_row(blocked_timing, "delayed_wall_actions").find("\"status\": \"blocked\"") != std::string::npos && feature_row(blocked_timing, "stateful_trigger_flags").find("\"status\": \"blocked\"") != std::string::npos, "scheduler browser rows hide unsupported action requirements");
		for (const auto field : { offset + 12, offset + 32 }) {
			auto invalid = bytes;
			patch(invalid, field, 0xffff, 2);
			rejects([&] { xl_compat::inspect_level(invalid); });
			patch(invalid, field, field == offset + 12 ? 1 : 6, 2);
			rejects([&] { xl_compat::inspect_level(invalid); });
		}
		for (const int time : { -900001, -900000, -100, -99, 0, 99, 100, 900000, 900001, (-2147483647 - 1) }) {
			auto delayed = bytes;
			patch(delayed, offset + 8, static_cast<std::uint32_t>(time));
			const auto parsed = xl_compat::inspect_level(delayed);
			const bool found = std::any_of(parsed.unsupported.begin(), parsed.unsupported.end(), [](const xl_compat::Issue &issue) { return issue.feature == "teleport_delay"; });
			const auto magnitude = time < 0 ? -std::int64_t(time) : std::int64_t(time);
			check(!found, "implemented teleport scheduling remains blocked");
			const auto row = feature_row(xl_compat::compatibility_json("delay.rl2", delayed), "delayed_wall_actions");
			check(row.find(magnitude >= 100 && magnitude <= 900000 ? "\"count\": 1" : "\"count\": 0") != std::string::npos, "delay requirement count differs from reference boundaries");
			check(xl_compat::runtime_blockers(parsed).empty() == !found, "runtime admission ignored teleport scheduler requirement");
		}
	}
}

void object_trigger_inventory_contract(const char *directory = nullptr)
{
	for (const int version : { 15, 17, 23 }) {
		auto bytes = fixture(version);
		const auto base = xl_compat::inspect_level(bytes);
		const auto game = base.game_offset;
		const auto old_trigger = std::find_if(base.sections.begin(), base.sections.end(), [](const xl_compat::Section &s) { return s.name == "triggers"; })->offset;
		bytes.resize(old_trigger);
		patch(bytes, game + 35, static_cast<std::uint32_t>(bytes.size()));
		patch(bytes, game + 39, 2);
		for (int i = 0; i < 2; ++i) {
			const auto start = bytes.size();
			bytes.resize(start + (version == 23 ? 80 : 79));
			patch(bytes, start, 2, 1);
		}
		const auto trigger = bytes.size();
		patch(bytes, game + 71, static_cast<std::uint32_t>(trigger));
		put(bytes, 3, 4);
		for (const int type : { 14, 0, 31 }) {
			put(bytes, type, 1);
			put(bytes, type == 14 ? 1024 | 8 : 16, 2);
			put(bytes, 1, 1);
			put(bytes, 0, 1);
			put(bytes, 5 * 65536, 4);
			put(bytes, 1000, 4);
			for (int i = 0; i < 10; ++i) put(bytes, 0, 2);
			put(bytes, type == 14 ? 0xffff : 0, 2);
			for (int i = 1; i < 10; ++i) put(bytes, 0, 2);
		}
		const auto owners = bytes.size();
		for (const int owner : { 1, 0, -1 }) {
			if (version != 23) {
				put(bytes, 32767, 2);
				put(bytes, 0xfffe, 2);
			}
			put(bytes, static_cast<std::uint32_t>(owner), 2);
		}
		const auto tail = bytes.size();
		if (version == 15) {
			put(bytes, 1, 2);
			for (int i = 1; i < 700; ++i) put(bytes, 0xffff, 2);
		} else if (version == 17) {
			put(bytes, 2, 2);
			for (const int value : { 0, 1, 1, 0 }) put(bytes, value, 2);
		}
		const auto decoded = xl_compat::inspect_level(bytes);
		check(decoded.object_triggers == 3 && decoded.object_trigger_records.size() == 3, "object trigger count/records lost");
		check(decoded.object_trigger_records[0].object == 1 && decoded.object_trigger_records[1].object == 0 && decoded.object_trigger_records[2].object == -1, "object owner order changed");
		check(decoded.object_trigger_records[0].action.flags == 1032 && decoded.object_trigger_records[0].action.targets[0][1] == -1, "wide flags or inert object teleport side lost");
		check(decoded.object_trigger_records[2].action.type == 31 && decoded.object_trigger_records[2].action.value == 5 * 65536 && decoded.object_trigger_records[2].action.time == 1000, "inspection normalized authored object fields");
		check(decoded.object_trigger_records[1].previous == (version == 23 ? -1 : 32767) && decoded.object_trigger_records[1].next == (version == 23 ? -1 : -2), "legacy inert links changed");
		check(decoded.object_trigger_heads.size() == (version == 15 ? 700u : 0u) && decoded.object_trigger_sparse_heads.size() == (version == 17 ? 2u : 0u), "reference layout changed");
		const auto report = xl_compat::compatibility_json("objects.rl2", bytes);
		check(feature_row(report, "attached_trigger_count").find("\"count\": 3") != std::string::npos, "decoded object count remains unknown");
		check(feature_row(report, "object_triggers").find("\"status\": \"blocked\"") != std::string::npos && !xl_compat::runtime_blockers(decoded).empty(), "parsing enabled unverified object events");
		check(xl_compat::inventory_json(decoded).find("\"object_trigger_records\"") != std::string::npos, "decoded object action evidence missing");
		auto eligible = bytes;
		for (int i = 0; i < 3; ++i) {
			patch(eligible, trigger + 4 + i * 53, 14, 1);
			patch(eligible, trigger + 5 + i * 53, 8, 2);
		}
		const auto admitted = xl_compat::prepare_gameplay_probe(eligible);
		check(admitted.source.object_trigger_records.size() == 3, "probe discarded admitted object source records");
		const auto admitted_report = xl_compat::compatibility_json("objects.rl2", eligible);
		check(feature_row(admitted_report, "attached_trigger_count").find("\"status\": \"partial\"") != std::string::npos, "native checkpoint claimed complete object qualification");
		auto chain = admitted.source;
		chain.object_trigger_records.resize(1);
		chain.object_triggers = 1;
		chain.object_trigger_records[0].action.type = 31;
		chain.object_trigger_records[0].action.targets = { { 0, 0 } };
		chain.walls.resize(2);
		chain.walls[0].trigger = 0;
		chain.walls[1].trigger = 1;
		chain.segments[0].sides[0].wall = 0;
		chain.segments[0].sides[1].wall = 1;
		chain.triggers = { { 31, 0, 0, -1, { { 0, 1 } } }, { 14, 0, 0, -1, { { 0, 0 } } } };
		check(xl_compat::runtime_blockers(chain).empty(), "immediate nested object master blocked");
		chain.triggers[1].time = 1000;
		check(xl_compat::blocker_message(xl_compat::runtime_blockers(chain)).find("delayed wall action") != std::string::npos, "delayed object descendant lost its explicit lifecycle blocker");
		chain.triggers[1].time = -1;
		chain.triggers[1].targets.push_back({ 0, 1 });
		check(!xl_compat::runtime_blockers(chain).empty(), "object chain admitted duplicate-segment retry deadlock");
		chain.triggers[0].targets = { { 0, 0 } };
		check(xl_compat::runtime_blockers(chain).empty(), "bounded immediate cyclic object chain rejected");
		for (const int type : { 15, 26, 29 }) {
			auto bad = eligible;
			patch(bad, trigger + 4, type, 1);
			rejects([&] { xl_compat::prepare_gameplay_probe(bad); });
		}
		for (const int target : { -1, -32768 }) {
			auto spawn = eligible;
			patch(spawn, trigger + 4, 26, 1);
			patch(spawn, trigger + 17, static_cast<std::uint32_t>(target), 2);
			const auto prepared = xl_compat::prepare_gameplay_probe(spawn);
			check(prepared.source.object_trigger_records[0].action.targets[0][0] == target, "arbitrary spawn selection sentinel lost");
			patch(spawn, trigger + 7, 2, 1);
			patch(spawn, trigger + 19, 32767, 2);
			check(xl_compat::prepare_gameplay_probe(spawn).source.object_trigger_records[0].action.targets[1][0] == 32767, "inert extra spawn link was interpreted as geometry");
			auto owner = prepared.source;
			owner.objects[1].type = 7;
			check(xl_compat::blocker_message(xl_compat::runtime_blockers(owner)).find("robot boss owner") != std::string::npos, "spawn action accepted a nonrobot owner");
		}
		for (const int flags : { 32, 64, 96, 255 }) {
			auto supported = eligible;
			patch(supported, trigger + 5, flags, 2);
			check(xl_compat::prepare_gameplay_probe(supported).source.object_trigger_records[0].action.flags == flags, "implemented object flag bits were rejected or lost");
		}
		for (const int flags : { 256, 32768 }) {
			auto bad = eligible;
			patch(bad, trigger + 5, flags, 2);
			rejects([&] { xl_compat::prepare_gameplay_probe(bad); });
		}
		auto retry_deadlock = eligible;
		patch(retry_deadlock, trigger + 7, 2, 1);
		rejects([&] { xl_compat::prepare_gameplay_probe(retry_deadlock); });
		auto capacity = admitted.source;
		capacity.object_trigger_records.resize(255, capacity.object_trigger_records[0]);
		capacity.object_triggers = 255;
		check(xl_compat::runtime_blockers(capacity).empty(), "bounded object index endpoint was rejected");
		capacity.object_trigger_records.resize(256, capacity.object_trigger_records[0]);
		capacity.object_triggers = 256;
		check(!xl_compat::runtime_blockers(capacity).empty(), "object trigger capacity silently exceeded reference index width");
		auto player_owner = admitted.source;
		player_owner.objects[1].type = 4;
		check(!xl_compat::runtime_blockers(player_owner).empty(), "unqualified player object ownership admitted");
		if (directory) {
			std::filesystem::create_directories(directory);
			std::ofstream file(std::filesystem::path(directory) / (std::to_string(version) + ".rl2"), std::ios::binary);
			file.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
			file.close();
			check(bool(file), "cannot export object reference fixture");
		}
		rejects([&] { xl_compat::prepare_gameplay_probe(bytes); });
		for (const auto value : { 0x7fffffffU, 0xffffffffU }) {
			auto bad = bytes;
			patch(bad, trigger, value);
			rejects([&] { xl_compat::inspect_level(bad); });
		}
		for (const auto value : { 2, -2 }) {
			auto bad = bytes;
			patch(bad, owners + (version == 23 ? 0 : 4), static_cast<std::uint32_t>(value), 2);
			rejects([&] { xl_compat::inspect_level(bad); });
		}
		for (const auto field : { trigger + 4 + 13, trigger + 4 + 53 + 33 }) {
			auto bad = bytes;
			patch(bad, field, 0xffff, 2);
			rejects([&] { xl_compat::inspect_level(bad); });
		}
		for (const auto size : { trigger + 4, owners - 1, tail - 1, bytes.size() - 1 }) {
			auto bad = bytes;
			bad.resize(size);
			rejects([&] { xl_compat::inspect_level(bad); });
		}
		auto crossing_section = bytes;
		patch(crossing_section, game + 83, static_cast<std::uint32_t>(owners + 1));
		patch(crossing_section, game + 87, 1);
		rejects([&] { xl_compat::inspect_level(crossing_section); });
		if (version == 17) {
			for (const auto value : { 0xffffU, 32767U }) {
				auto bad = bytes;
				patch(bad, tail, value, 2);
				rejects([&] { xl_compat::inspect_level(bad); });
			}
		}
		bytes.push_back(0);
		rejects([&] { xl_compat::inspect_level(bytes); });
	}
}

void feature_contract()
{
	auto bytes = fixture();
	auto declared = bytes;
	patch(declared, declared.size() - 4, 1);
	const auto declaration_report = xl_compat::compatibility_json("declaration.rl2", declared);
	check(feature_row(declaration_report, "attached_trigger_count").find("\"count\": null") != std::string::npos, "unvalidated object declaration became inspected count");
	check(declaration_report.find("object trigger count exceeds section extent") != std::string::npos, "truncated object declaration was not rejected");
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
	check(row.find("\"count\": 3") != std::string::npos && row.find("\"status\": \"supported\"") != std::string::npos, "immediate wall teleport feature count or support differs");
	check(json.find("\"status\": \"blocked\"") == json.find("\"status\":"), "teleport support hid unsupported boost/camera requirements");
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

void trigger_chain_inventory_contract()
{
	for (int version : { 15, 17, 23 }) {
		const auto original = fixture(version);
		const auto inventory = xl_compat::inspect_level(original);
		const auto &section = *std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const auto &s) { return s.name == "triggers"; });
		for (int type : { 31, 32, 33 }) {
			auto bytes = original;
			bytes.insert(bytes.begin() + section.offset, 52, 0);
			patch(bytes, inventory.game_offset + 75, 1);
			patch(bytes, section.offset, type, 1);
			patch(bytes, section.offset + 2, 1, 1);
			patch(bytes, section.offset + 8, 0xffffffff);
			const auto decoded = xl_compat::inspect_level(bytes);
			check(xl_compat::runtime_blockers(decoded).empty(), "immediate chain action rejected");
			if (type == 31)
				for (int value : { -65536, 0, 1, 65535, 65536, 131072 })
					for (int flags : { 0, 8, 128, 136 }) {
						auto authored = bytes;
						patch(authored, section.offset + 4, value);
						patch(authored, section.offset + 1, flags, 1);
						const auto translated = xl_compat::prepare_gameplay_probe(authored).stock_level;
						const auto read = [&](std::size_t offset) {
							std::uint32_t result = 0;
							for (unsigned i = 0; i < 4; ++i) result |= std::uint32_t(translated.at(offset + i)) << (i * 8);
							return static_cast<std::int32_t>(result);
						};
						const auto game = read(12), trigger = read(game + 71);
						const int expected = version < 23 ? 0 : value > 0 ? value / 65536
						                                                  : value;
						const int expected_flags = version == 23 && value > 0 ? (flags | 4) & (flags & 128 ? ~8 : ~0) : flags;
						check(read(trigger + 4) == expected && translated.at(trigger + 1) == expected_flags, "versioned master source counter normalization differs");
					}
			const auto row = feature_row(xl_compat::compatibility_json("chain.rl2", bytes), type == 31 ? "master_triggers" : "trigger_switches");
			check(row.find("\"status\": \"supported\"") != std::string::npos && row.find("\"count\": 1") != std::string::npos, "browser does not expose supported chain action");
			for (int flags : { 32, 64 }) {
				auto supported = bytes;
				patch(supported, section.offset + 1, flags, 1);
				check(xl_compat::runtime_blockers(xl_compat::inspect_level(supported)).empty(), "inert chain flag rejected");
			}
			for (int flags : { 8, 16, 32, 64, 96, 128, 24, 136, 152, 159, 255 }) {
				auto supported = bytes;
				patch(supported, section.offset + 1, flags, 1);
				check(xl_compat::runtime_blockers(xl_compat::inspect_level(supported)).empty(), "implemented stateful flags rejected");
			}
			for (int time : { -900001, -900000, -100, -99, 0, 99, 100, 900000, 900001, (-2147483647 - 1) }) {
				auto timed = bytes;
				patch(timed, section.offset + 8, static_cast<std::uint32_t>(time));
				check(xl_compat::runtime_blockers(xl_compat::inspect_level(timed)).empty(), "supported chain scheduling rejected");
			}
			for (auto field : { section.offset + 12, section.offset + 32 }) {
				auto bad = bytes;
				patch(bad, field, field == section.offset + 12 ? 1 : 6, 2);
				rejects([&] { xl_compat::inspect_level(bad); });
				patch(bad, field, 0xffff, 2);
				if (field == section.offset + 32 && type != 31) {
					const auto blocked = xl_compat::runtime_blockers(xl_compat::inspect_level(bad));
					check(!blocked.empty() && xl_compat::blocker_message(blocked).find("effect") != std::string::npos, "effect-object target not rejected helpfully");
				} else rejects([&] { xl_compat::inspect_level(bad); });
			}
		}
	}
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
	check(!xl_compat::prepare_gameplay_probe(bytes).stock_level.empty(), "audited 17/36 layout rejected");
	const auto compatibility = xl_compat::compatibility_json("fixture.rl2", bytes);
	check(compatibility.find("\"status\": \"eligible\"") != std::string::npos && compatibility.find("\"playable\": false") != std::string::npos, "17 inspection failed to distinguish eligibility from playability");
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
	check(with_equipment.equipment.size() == 1 && with_equipment.equipment[0] == std::array<int, 6>{}, "legacy equipment payload silently dropped");
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

void version_23_contract()
{
	const auto bytes = fixture(23);
	const auto inventory = xl_compat::inspect_level(bytes);
	check(inventory.level_version == 23 && inventory.game_version == 40, "23/40 versions");
	check(inventory.segments.at(0).properties == 0 && inventory.segments.at(0).damage == std::array<int, 2>{ 0, 0 }, "split properties lost");
	check(inventory.segments.at(0).light == 65536, "split properties shifted ambient light");
	check(!xl_compat::prepare_gameplay_probe(bytes).stock_level.empty(), "23/40 normal translation failed");
	check(!xl_compat::prepare_serialization_probe_23_40(bytes).stock_level.empty(), "23/40 qualification failed");
	rejects([&] { xl_compat::prepare_serialization_probe_23_40(fixture(17)); });
	const auto segment = inventory.mine_offset + 277;
	for (const int function : { 7, 13 }) {
		auto bad = bytes;
		patch(bad, segment, function, 1);
		const auto decoded = xl_compat::inspect_level(bad);
		check(std::any_of(decoded.unsupported.begin(), decoded.unsupported.end(), [](const auto &issue) { return issue.feature == "segment_function_v21"; }), "modern function confused with old water/no-damage");
		const auto report = xl_compat::compatibility_json("modern-function.rl2", bad);
		check(feature_row(report, "water_segments").find("\"count\": 0") != std::string::npos && feature_row(report, "no_damage_segments").find("\"count\": 0") != std::string::npos, "browser confuses modern functions with old properties");
		rejects([&] { xl_compat::prepare_serialization_probe_23_40(bad); });
	}
	for (const int property : { 1, 2, 4, 8, 16, 32, 64, 128 }) {
		auto bad = bytes;
		patch(bad, segment + 4, property, 1);
		check(xl_compat::inspect_level(bad).segments.at(0).properties == property, "property bit lost");
		if (property == 1 || property == 8) {
			const auto row = feature_row(xl_compat::compatibility_json("modern-property.rl2", bad), property == 1 ? "water_segments" : "no_damage_segments");
			check(row.find("\"count\": 1") != std::string::npos && row.find("\"status\": \"supported\"") != std::string::npos, "browser lost split no-damage support");
			check(!xl_compat::prepare_gameplay_probe(bad).stock_level.empty(), "modern no-damage was not translated");
			continue;
		}
		rejects([&] { xl_compat::prepare_serialization_probe_23_40(bad); });
	}
	for (const int damage : { -32768, -1, 1, 32767 }) {
		auto bad = bytes;
		patch(bad, segment + 5, damage, 2);
		patch(bad, segment + 7, damage, 2);
		check(xl_compat::inspect_level(bad).segments.at(0).damage == std::array<int, 2>{ damage, damage }, "signed damage changed");
		rejects([&] { xl_compat::prepare_serialization_probe_23_40(bad); });
	}
	for (int bits = 0; bits < 256; ++bits) {
		auto combined = bytes;
		patch(combined, segment + 4, bits, 1);
		const auto decoded = xl_compat::inspect_level(combined);
		check(xl_compat::runtime_blockers(decoded).empty() == (bits == 0 || bits == 1 || bits == 8 || bits == 9), "normal modern admission accepted unsupported property combination");
		check(xl_compat::runtime_blockers(decoded, true).empty() == (bits == 0 || bits == 1 || bits == 8 || bits == 9), "experimental water profile accepted unrelated property bits");
		if (bits == 1 || bits == 9) {
			check(!xl_compat::prepare_gameplay_probe(combined).stock_level.empty(), "normal modern water combination translation failed");
			for (int actor = 0; actor < 3; ++actor) {
				auto unsupported_actor = decoded;
				if (actor == 2) unsupported_actor.matcens.resize(1);
				else {
					xl_compat::Object object{};
					if (actor == 0) object.type = 2;
					else {
						object.contains_type = 2;
						object.contains_count = 1;
					}
					unsupported_actor.objects.push_back(object);
				}
				check(xl_compat::blocker_message(xl_compat::runtime_blockers(unsupported_actor)).find("water actors") != std::string::npos, "water robot/generator gate lost");
			}
		}
	}
	auto bad = bytes;
	patch(bad, inventory.game_offset + 2, 36, 2);
	rejects([&] { xl_compat::inspect_level(bad); });
	bad = bytes;
	bad.pop_back();
	rejects([&] { xl_compat::inspect_level(bad); });
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
		check(xl_compat::prepare_gameplay_probe(bytes).stock_level == out, "normal admission differs from qualified 17/36 translation");
		bytes.at(source.mine_offset + 277) = 12;
		rejects([&] { xl_compat::prepare_gameplay_probe(bytes); });
		rejects([&] { xl_compat::prepare_serialization_probe_17_36(bytes); });
	}
	rejects([&] { xl_compat::prepare_serialization_probe_17_36(fixture()); });
	std::cout << "17/36 layout qualification: normal admission, header removal, retained offsets, packed counts 0..8191; blocked-segment semantics remain rejected\n";
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
		check(blockers.empty(), "reserved flags blocked audited 17/36 serialization");
		check(xl_compat::prepare_gameplay_probe(bytes).source.segments.at(0).flags == flags, "normal translation lost reserved flags");
	}
	const auto legacy_bytes = fixture(15, 255);
	const auto legacy = xl_compat::inspect_level(legacy_bytes);
	check(legacy.observations.empty() && std::any_of(legacy.unknown.begin(), legacy.unknown.end(), [](const auto &i) { return i.feature == "segment_flags"; }), "17/36 audit widened another profile");
	rejects([&] { xl_compat::prepare_gameplay_probe(legacy_bytes); });
}

void original_neptune(const char *path)
{
	std::ifstream input(path, std::ios::binary);
	check(bool(input), "Neptune HOG open failed");
	const Bytes bytes((std::istreambuf_iterator<char>(input)), std::istreambuf_iterator<char>());
	const auto members = xl_compat::read_hog(bytes);
	check(members.size() == 4, "Neptune resource inventory changed");
	const auto level = std::find_if(members.begin(), members.end(), [](const auto &member) { return member.name == "neptunxl.rl2"; });
	check(level != members.end() && level->bytes.size() == 48977, "original Neptune level missing");
	const auto probe = xl_compat::prepare_serialization_probe_23_40(level->bytes);
	const auto &v = probe.source;
	check(v.segments.size() == 93 && v.objects.size() == 49 && v.walls.size() == 30 && v.triggers.size() == 1, "Neptune inventory changed");
	check(v.object_triggers == 0 && v.object_trigger_heads.empty(), "Neptune object trigger tail changed");
	check(xl_compat::prepare_gameplay_probe(level->bytes).stock_level == probe.stock_level, "normal Neptune translation differs from qualification");
	const auto read32 = [&](std::size_t offset) {
		std::uint32_t n = 0;
		for (int i = 0; i < 4; ++i) n |= std::uint32_t(probe.stock_level.at(offset + i)) << (8 * i);
		return n;
	};
	const auto game = read32(12);
	std::size_t cursor = read32(game + 35);
	for (const auto &object : v.objects) {
		for (auto source = object.source_offset; source < object.source_end; ++source) {
			if (source == object.source_offset + 6) continue;
			check(probe.stock_level.at(cursor++) == level->bytes.at(source), "translated object payload differs");
		}
	}
	check(cursor == read32(game + 47), "object record boundary differs");
	for (std::size_t i = 0; i < v.walls.size(); ++i) {
		for (std::size_t field = 0; field < 25; ++field) {
			if (field == 18) continue;
			check(probe.stock_level.at(cursor++) == level->bytes.at(v.sections.at(1).offset + i * 25 + field), "translated wall payload differs");
		}
	}
	for (const auto &object : v.objects) {
		auto bad = level->bytes;
		patch(bad, object.source_offset + 6, 1, 1);
		rejects([&] { xl_compat::prepare_serialization_probe_23_40(bad); });
	}
	for (std::size_t i = 0; i < v.walls.size(); ++i) {
		auto bad = level->bytes;
		patch(bad, v.sections.at(1).offset + i * 25 + 18, 1, 1);
		rejects([&] { xl_compat::prepare_serialization_probe_23_40(bad); });
	}
	std::cout << "Original Neptune 23/40 inspection and record translation passed; gameplay and resources remain unqualified\n";
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
	check(!xl_compat::has_registered_colors(v), "original zero tables incorrectly register baked ambient");
	check(report.find("method-0 ambient reconstruction required and implemented") != std::string::npos, "original ambient requirement missing from browser evidence");
	auto colored = v;
	colored.texture_colors[0].rgb = { 0.5, 0.5, 0.5 };
	check(!xl_compat::has_registered_colors(colored), "texture colors incorrectly suppress ambient rebuilding");
	colored.vertex_colors[0].rgb = { 1, 0, 1 };
	check(!xl_compat::has_registered_colors(colored), "endpoint colors incorrectly register ambient");
	colored.side_colors[0].rgb[0] = 0.5;
	check(xl_compat::has_registered_colors(colored), "fractional side color did not register ambient");
	colored.side_colors[0].rgb[0] = 0;
	colored.vertex_colors[0].rgb[0] = 0.5;
	check(xl_compat::has_registered_colors(colored), "fractional vertex color did not register ambient");
	check(feature_row(report, "no_damage_segments").find("\"count\": 26") != std::string::npos, "original no-damage UI count");
	check(feature_row(report, "no_damage_segments").find("\"status\": \"supported\"") != std::string::npos, "original no-damage UI support status");
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
	check(xl_compat::runtime_blockers(v).empty(), "supported original no-damage inventory was blocked");
	check(xl_compat::prepare_gameplay_probe(level->bytes).source.segments.size() == 110, "original no-damage translation failed");
	std::cout << "Original Observatory 17/36 inventory passed; 26 no-damage segments retained; normal feature translation eligible; runtime assets checked separately\n";
}
#include "effect_layout_contract.h"
} // namespace

int main(int argc, char **argv)
{
	try {
		if (argc == 2 && std::string(argv[1]) == "--effects") {
			effect_layout_contract();
			return 0;
		}
		if (argc == 3 && std::string(argv[1]) == "--effect-fixtures") {
			effect_layout_contract(argv[2]);
			return 0;
		}
		if (argc == 3 && std::string(argv[1]) == "--generator-fixtures") {
			generator_assignment_contract(argv[2]);
			return 0;
		}
		generator_assignment_contract();
		if (argc == 3 && std::string(argv[1]) == "--object-fixtures") {
			object_trigger_inventory_contract(argv[2]);
			return 0;
		}
		// Independent encoding controls: modern functions never imply old properties
		xl_compat::Segment property_segment{};
		const int legacy_properties[] = { 0, 0, 0, 0, 0, 0, 0, 1, 2, 0, 0, 0, 4, 8, 4, 0, 16 };
		for (int function = 0; function < 17; ++function) {
			property_segment.function = function;
			for (int bits = 0; bits < 256; ++bits) {
				property_segment.properties = bits;
				check(xl_compat::segment_property_bits(23, property_segment) == bits, "modern property/function independence lost");
				check(xl_compat::segment_property_bits(17, property_segment) == legacy_properties[function], "modern property field leaked into legacy interpretation");
			}
		}
		check(argc == 1 || argc == 2 || (argc == 3 && std::string(argv[1]) == "--neptune"), "expected optional original HOG path");
		if (argc == 2) original_observatory(argv[1]);
		if (argc == 3) original_neptune(argv[2]);
		version_17_contract();
		version_23_contract();
		serialization_17_contract();
		teleport_inventory_contract();
		trigger_chain_inventory_contract();
		object_trigger_inventory_contract();
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
		rejects([&] { xl_compat::prepare_gameplay_probe(water); });
		water.at(mine + 277) = 13;
		const auto protected_probe = xl_compat::prepare_gameplay_probe(water);
		check(protected_probe.source.segments.at(0).function == 13, "no-damage qualification lost owned source property");
		check(protected_probe.stock_level == xl_compat::prepare_gameplay_probe(bytes).stock_level, "no-damage qualification leaked into stock enum");

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
		check(feature_row(compatibility, "generators").find("\"status\": \"not_required\"") != std::string::npos, "fully decoded absence of generators remains unknown");
		for (const auto id : { "navigation", "wall_materials", "effect_objects", "resource_scope" }) {
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
		{
			auto equipment = fixture(23, 0, 2);
			const auto source = xl_compat::inspect_level(equipment);
			const auto properties = source.game_offset - (source.vertices.size() + source.segments.size() * 6 + 910 + source.segments.size()) * 13;
			for (int segment = 0; segment < 2; ++segment) {
				patch(equipment, properties + segment * 13, 11, 1);
				patch(equipment, properties + segment * 13 + 1, 1 - segment, 1);
			}
			const auto table = equipment.size();
			patch(equipment, source.game_offset + 35 + 9 * 12, static_cast<unsigned>(table));
			patch(equipment, source.game_offset + 39 + 9 * 12, 2);
			patch(equipment, source.game_offset + 43 + 9 * 12, 20);
			for (int row = 0; row < 2; ++row) {
				for (unsigned value : { 1u << (row + 1), 0u, 65536u, 327680u }) put(equipment, value, 4);
				put(equipment, 65535u, 2);
				put(equipment, 32767u, 2);
			}
			const auto admitted = xl_compat::prepare_gameplay_probe(equipment);
			check(admitted.source.equipment.size() == 2 && admitted.source.equipment[0][4] == -1 && admitted.source.segments[0].producer == 1, "equipment source assignment or raw records lost");
			check(xl_compat::inventory_json(admitted.source).find("\"equipment\"") != std::string::npos, "equipment records absent from inspection");
			auto bad_equipment = equipment;
			bad_equipment.pop_back();
			rejects([&] { xl_compat::inspect_level(bad_equipment); });
			bad_equipment = equipment;
			patch(bad_equipment, properties + 1, 0, 1);
			rejects([&] { xl_compat::prepare_gameplay_probe(bad_equipment); });
			for (unsigned id : { 7u, 8u, 24u, 46u, 48u, 49u }) {
				bad_equipment = equipment;
				patch(bad_equipment, table, id < 32 ? 1u << id : 0u);
				patch(bad_equipment, table + 4, id >= 32 ? 1u << (id - 32) : 0u);
				rejects([&] { xl_compat::prepare_gameplay_probe(bad_equipment); });
			}
			bad_equipment = equipment;
			patch(bad_equipment, table + 4, 0x80000000u);
			check(xl_compat::prepare_gameplay_probe(bad_equipment).source.equipment[0][1] == INT32_MIN, "inert equipment bits were interpreted or erased");
		}
		std::cout << "XL inventory contracts passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
