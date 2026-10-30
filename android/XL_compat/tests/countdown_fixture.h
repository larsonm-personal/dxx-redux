#ifndef DXX_XL_COUNTDOWN_FIXTURE_H
#define DXX_XL_COUNTDOWN_FIXTURE_H
#include "cloak_fixture.h"
#include "trigger_capacity_fixture.h"
#include "runtime_layout_fixture.h"
#include "hog_fixture.h"
#include "../admission.h"
#include <fstream>
#include <filesystem>
#include <iterator>
namespace xl_test
{
inline xl_compat::Bytes countdown_level(const xl_compat::Bytes &original)
{
	auto bytes = expand_triggers(add_cloak_pairs(original, 1), 8);
	const auto source = xl_compat::inspect_level(bytes);
	const auto patch = [&](std::size_t at, int value, int width = 4) { layout_patch(bytes, at, std::uint32_t(value), width); };
	const auto table = source.sections[3].offset;
	const auto &front = source.walls[source.walls.size() - 2];
	const int times[] = { 99, 1000, 0, -42, -1, 44, 55, 14 };
	for (unsigned i = 0; i < 8; ++i) {
		const auto at = table + i * 52;
		std::fill(bytes.begin() + at, bytes.begin() + at + 52, 0);
		patch(at, i == 4 ? 31 : 25, 1);
		patch(at + 1, i == 1 ? 2 : i == 5 ? 128
		                       : i == 6   ? 4
		                       : i == 7   ? 1
		                                  : 0,
		      1);
		patch(at + 2, 1, 1);
		patch(at + 4, i == 4 ? 0 : 5 * 65536);
		patch(at + 8, times[i]);
		// Countdown targets are inert, including otherwise invalid geometry
		patch(at + 12, i == 4 ? front.segment : -300, 2);
		patch(at + 32, i == 4 ? front.side : 42, 2);
	}
	patch(source.sections[1].offset + (source.walls.size() - 2) * 25 + 20, 0, 1);
	std::vector<int> owners;
	for (unsigned i = 0; i < source.objects.size() && owners.size() < 2; ++i)
		if (source.objects[i].type == 7) owners.push_back(int(i));
	if (owners.size() != 2 || source.object_triggers) throw std::runtime_error("Countdown fixture needs two unchanged powerup owners");
	xl_compat::Bytes payload;
	for (unsigned i = 0; i < owners.size(); ++i) {
		layout_put(payload, 25, 1);
		layout_put(payload, i ? 128 : 0, 2);
		layout_put(payload, 1, 1);
		layout_put(payload, 0, 1);
		layout_put(payload, 5 * 65536, 4);
		layout_put(payload, i ? 22 : 120, 4);
		const auto at = payload.size();
		payload.resize(at + 40);
		layout_patch(payload, at, std::uint32_t(-300), 2);
		layout_patch(payload, at + 20, 42, 2);
	}
	for (int owner : owners) layout_put(payload, owner, 2);
	const auto object_table = table + 8 * 52;
	patch(object_table, int(owners.size()));
	bytes.insert(bytes.begin() + object_table + 4, payload.begin(), payload.end());
	for (unsigned i = 0; i < source.sections.size(); ++i)
		if (source.sections[i].offset >= object_table + 4) patch(source.game_offset + 35 + i * 12, source.sections[i].offset + int(payload.size()));
	const auto check = xl_compat::inspect_level(bytes);
	if (check.triggers.size() != 8 || check.object_trigger_records.size() != 2 || !xl_compat::runtime_blockers(check).empty())
		throw std::runtime_error("Countdown fixture failed detected admission");
	return bytes;
}
inline void write_countdown_fixture(const std::filesystem::path &original, const std::filesystem::path &directory)
{
	std::ifstream file(original, std::ios::binary);
	const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	const auto members = xl_compat::read_hog(bytes);
	const auto found = std::find_if(members.begin(), members.end(), [](const auto &row) { return row.name == "neptunxl.rl2"; });
	if (found == members.end()) throw std::runtime_error("Original countdown control geometry missing");
	const auto level = countdown_level(found->bytes);
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	for (const std::string name : { "count23", "count27", "badcount", "badobject", "badbase" }) {
		auto current = name == "count27" ? runtime_layout_fixture(level, 27).bytes : level;
		const auto source = xl_compat::inspect_level(current);
		if (name == "badcount") layout_patch(current, source.sections[3].offset + 8, 32768);
		if (name == "badobject") layout_patch(current, source.sections[3].offset + 8 * 52 + 4 + 9, 32768);
		if (name == "badbase") layout_patch(current, source.flicker_offset - 8, 10923);
		hog_member(output, name + ".rl2", current, false);
		for (const auto &member : members) {
			if (member.name.rfind("neptunxl.", 0) != 0) continue;
			const auto ext = member.name.substr(8);
			if (ext != ".rl2" && ext != ".mn2") hog_member(output, name + ext, member.bytes, false);
		}
	}
	std::filesystem::create_directories(directory);
	std::ofstream hog(directory / "xcount.hog", std::ios::binary);
	hog.write(reinterpret_cast<const char *>(output.data()), std::streamsize(output.size()));
	std::ofstream descriptor(directory / "xcount.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL countdowns\nnum_levels = 2\ncount23.rl2\ncount27.rl2\n";
	if (!hog || !descriptor) throw std::runtime_error("Cannot write isolated countdown fixture");
}
} // namespace xl_test
#endif
