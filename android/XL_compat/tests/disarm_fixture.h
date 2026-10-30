#ifndef DXX_XL_DISARM_FIXTURE_H
#define DXX_XL_DISARM_FIXTURE_H
#include "countdown_fixture.h"
namespace xl_test
{
inline xl_compat::Bytes disarm_level(const xl_compat::Bytes &original, int model, int radius, int strength)
{
	auto bytes = original;
	const auto geometry = xl_compat::inspect_level(original);
	if (geometry.level_version != 23 || geometry.game_version != 40 || geometry.segments.size() < 24 || geometry.objects.empty()) throw std::runtime_error("Unexpected disarming fixture geometry");
	xl_compat::Bytes added;
	for (int segment = 20; segment < 24; ++segment) {
		// Ordinary robot 0 in the audited 23/40 serialization, away from the spawn
		for (int value : { 2, 0, 1, 1, 1, 0, 0 }) layout_put(added, value, 1);
		layout_put(added, segment, 2);
		std::array<int, 3> center{};
		for (int c = 0; c < 3; ++c) {
			std::int64_t total = 0;
			for (int vertex : geometry.segments[segment].vertices) total += geometry.vertices[vertex][c];
			center[c] = int(total / 8);
			layout_put(added, center[c], 4);
		}
		for (int i = 0; i < 9; ++i) layout_put(added, i % 4 == 0 ? 65536 : 0, 4);
		layout_put(added, radius, 4);
		layout_put(added, strength, 4);
		for (int value : center) layout_put(added, value, 4);
		for (int value : { -1, -1, 0 }) layout_put(added, value, 1);
		for (int i = 0; i < 15; ++i) layout_put(added, i == 6 ? 65536 : 0, 4);
		layout_put(added, 0, 2);
		layout_put(added, 0, 2);
		layout_put(added, 0x81, 1); // AIB_NORMAL
		for (int i = 1; i < 12; ++i) layout_put(added, 0, 1);
		for (int i = 0; i < 4; ++i) layout_put(added, i == 0 ? -1 : 0, 2);
		layout_put(added, model, 4);
		for (int i = 0; i < 30; ++i) layout_put(added, 0, 2);
		layout_put(added, 0, 4);
		layout_put(added, -1, 4);
	}
	const auto end = geometry.objects.back().source_end;
	bytes.insert(bytes.begin() + end, added.begin(), added.end());
	for (unsigned i = 1; i < geometry.sections.size(); ++i)
		if (geometry.sections[i].offset >= int(end)) layout_patch(bytes, geometry.game_offset + 35 + i * 12, geometry.sections[i].offset + unsigned(added.size()));
	layout_patch(bytes, geometry.game_offset + 39, unsigned(geometry.objects.size()) + 4);
	bytes = countdown_level(bytes);
	const auto source = xl_compat::inspect_level(bytes);
	std::vector<int> robots;
	for (unsigned i = 0; i < source.objects.size() && robots.size() < 4; ++i)
		if (source.objects[i].type == 2) robots.push_back(int(i));
	if (robots.size() != 4) throw std::runtime_error("Disarming fixture needs four ordinary robots");
	const auto patch = [&](std::size_t at, int value, int width = 4) { layout_patch(bytes, at, std::uint32_t(value), width); };
	const auto table = source.sections[3].offset;
	const auto &front = source.walls[source.walls.size() - 2];
	const int targets[] = { robots[0], robots[1], robots[1], front.segment, robots[2], robots[0], source.object_trigger_records[0].object, robots[0] };
	const int flags[] = { 0, 2, 16, 0, 128, 4, 0, 8 };
	for (unsigned i = 0; i < 8; ++i) {
		const auto at = table + i * 52;
		std::fill(bytes.begin() + at, bytes.begin() + at + 52, 0);
		patch(at, i == 3 ? 31 : 34, 1);
		patch(at + 1, flags[i], 1);
		patch(at + 2, i == 6 || i == 7 ? 2 : 1, 1);
		patch(at + 8, i == 2 ? 6000 : -1);
		patch(at + 12, targets[i], 2);
		patch(at + 32, i == 3 ? front.side : -1, 2);
		if (i == 6 || i == 7) {
			patch(at + 14, i == 6 ? 32767 : robots[0], 2);
			patch(at + 34, i == 6 ? 42 : -32768, 2);
		}
	}
	for (unsigned i = 0; i < 2; ++i) {
		const auto at = table + 8 * 52 + 4 + i * 53;
		patch(at, 34, 1);
		patch(at + 13, robots[i ? 3 : 1], 2);
		patch(at + 33, -1, 2);
	}
	const auto check = xl_compat::inspect_level(bytes);
	if (check.triggers.size() != 8 || check.object_trigger_records.size() != 2 || !xl_compat::runtime_blockers(check).empty())
		throw std::runtime_error("Disarming fixture failed detected admission");
	return bytes;
}
inline void write_disarm_fixture(const std::filesystem::path &original, const std::filesystem::path &directory, int model, int radius, int strength)
{
	std::ifstream file(original, std::ios::binary);
	const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	const auto members = xl_compat::read_hog(bytes);
	const auto found = std::find_if(members.begin(), members.end(), [](const auto &row) { return row.name == "neptunxl.rl2"; });
	if (found == members.end()) throw std::runtime_error("Original disarming control geometry missing");
	const auto level = disarm_level(found->bytes, model, radius, strength);
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	for (const std::string name : { "disarm23", "disarm27", "badtarget" }) {
		auto current = name == "disarm27" ? runtime_layout_fixture(level, 27).bytes : level;
		if (name == "badtarget") layout_patch(current, xl_compat::inspect_level(current).sections[3].offset, 35, 1);
		hog_member(output, name + ".rl2", current, false);
		for (const auto &member : members) {
			if (member.name.rfind("neptunxl.", 0) != 0) continue;
			const auto ext = member.name.substr(8);
			if (ext != ".rl2" && ext != ".mn2") hog_member(output, name + ext, member.bytes, false);
		}
	}
	std::filesystem::create_directories(directory);
	std::ofstream hog(directory / "xdisarm.hog", std::ios::binary);
	hog.write(reinterpret_cast<const char *>(output.data()), std::streamsize(output.size()));
	std::ofstream descriptor(directory / "xdisarm.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL disarming\nnum_levels = 2\ndisarm23.rl2\ndisarm27.rl2\n";
	if (!hog || !descriptor) throw std::runtime_error("Cannot write isolated disarming fixture");
}
} // namespace xl_test
#endif
