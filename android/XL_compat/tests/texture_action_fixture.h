#ifndef DXX_XL_TEXTURE_ACTION_FIXTURE_H
#define DXX_XL_TEXTURE_ACTION_FIXTURE_H
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
inline xl_compat::Bytes texture_action_level(const xl_compat::Bytes &original)
{
	auto bytes = expand_triggers(add_cloak_pairs(original, 1), 12);
	auto source = xl_compat::inspect_level(bytes);
	const int front = int(source.walls.size()) - 2, back = front + 1;
	const auto patch = [&](std::size_t at, int value, int width = 4) { layout_patch(bytes, at, std::uint32_t(value), width); };
	// Insert an oriented overlay on the explicitly added control portal
	std::size_t cursor = source.mine_offset + 5 + source.vertices.size() * 12, insert = 0;
	for (const auto &room : source.segments) {
		cursor += 2;
		const auto children = bytes.at(cursor++);
		for (int side = 0; side < 6; ++side)
			if (children & (1 << side)) cursor += 2;
		cursor += 16;
		const auto walls = bytes.at(cursor++);
		for (int side = 0; side < 6; ++side)
			if (walls & (1 << side)) cursor += 2;
		for (int side = 0; side < 6; ++side) {
			if (room.children[side] != -1 && room.sides[side].wall < 0) continue;
			if (room.sides[side].wall == front) insert = cursor;
			cursor += bytes.at(cursor + 1) & 128 ? 28 : 26;
		}
	}
	if (!insert || (bytes.at(insert + 1) & 128)) throw std::runtime_error("Texture control portal already has an overlay");
	patch(insert, 0x8001, 2);
	bytes.insert(bytes.begin() + insert + 2, { 2, 0xc0 });
	patch(12, source.game_offset + 2);
	const auto rebase = [&](unsigned field) {
		std::uint32_t offset = 0;
		for (unsigned b = 0; b < 4; ++b) offset |= std::uint32_t(bytes.at(source.game_offset + 2 + field + b)) << (8 * b);
		if (std::int32_t(offset) > 0) patch(source.game_offset + 2 + field, int(offset) + 2);
	};
	rebase(27);
	for (unsigned i = 0; i < source.sections.size(); ++i) rebase(35 + i * 12);
	source = xl_compat::inspect_level(bytes);
	const auto table = source.sections[3].offset;
	const int values[] = { 0x00020001, 1, int(0xffff0004u), 0x00030001, 0x00020004, 0, 4, 0x00020001, 0x40020001, 4, -1, 4 };
	for (unsigned i = 0; i < 12; ++i) {
		const auto at = table + i * 52;
		std::fill(bytes.begin() + at, bytes.begin() + at + 52, 0);
		patch(at, i == 5 ? 31 : 19, 1);
		patch(at + 1, i == 6 ? 128 : i == 9 ? 4
		                         : i == 11  ? 3
		                                    : 0,
		      1);
		patch(at + 2, i == 10 ? 0 : i == 7 ? 2
		                                   : 1,
		      1);
		patch(at + 4, values[i]);
		patch(at + 8, i == 4 ? 6000 : -1);
		const int target = i == 6 ? back : front;
		patch(at + 12, source.walls[target].segment, 2);
		patch(at + 32, source.walls[target].side, 2);
		if (i == 7) {
			patch(at + 14, source.walls[back].segment, 2);
			patch(at + 34, source.walls[back].side, 2);
		}
	}
	patch(source.sections[1].offset + front * 25 + 20, 0, 1);
	std::vector<int> owners;
	for (unsigned i = 0; i < source.objects.size() && owners.size() < 2; ++i)
		if (source.objects[i].type == 7) owners.push_back(int(i));
	if (owners.size() != 2 || source.object_triggers) throw std::runtime_error("Texture fixture needs two unchanged powerup owners");
	xl_compat::Bytes payload;
	for (unsigned i = 0; i < owners.size(); ++i) {
		layout_put(payload, 19, 1);
		layout_put(payload, i ? 128 : 0, 2);
		layout_put(payload, 1, 1);
		layout_put(payload, 0, 1);
		layout_put(payload, i ? 0x00020004 : 4, 4);
		layout_put(payload, std::uint32_t(-1), 4);
		const auto start = payload.size();
		payload.resize(start + 40);
		layout_patch(payload, start, source.walls[front].segment, 2);
		layout_patch(payload, start + 20, source.walls[front].side, 2);
	}
	for (int owner : owners) layout_put(payload, owner, 2);
	const auto object_table = table + 12 * 52;
	patch(object_table, int(owners.size()));
	bytes.insert(bytes.begin() + object_table + 4, payload.begin(), payload.end());
	for (unsigned i = 0; i < source.sections.size(); ++i)
		if (source.sections[i].offset >= object_table + 4) patch(source.game_offset + 35 + i * 12, source.sections[i].offset + int(payload.size()));
	const auto check = xl_compat::inspect_level(bytes);
	if (check.triggers.size() != 12 || check.object_trigger_records.size() != 2 || !xl_compat::runtime_blockers(check).empty())
		throw std::runtime_error("Texture fixture failed detected admission");
	return bytes;
}

inline xl_compat::Bytes texture_action_pog(const std::array<unsigned, 4> &indices)
{
	xl_compat::Bytes result(12 + 4 * 20, 0);
	std::copy_n("DPOG", 4, result.begin());
	layout_patch(result, 4, 1);
	layout_patch(result, 8, 4);
	for (unsigned i = 0; i < indices.size(); ++i) {
		layout_patch(result, 12 + i * 2, indices[i], 2);
		const unsigned header = 20 + i * 18;
		result[header + 9] = result[header + 10] = 2;
		result[header + 12] = 128;
		layout_patch(result, header + 14, i * 16);
	}
	const unsigned char pixels[] = {
		20, 40, 220, 255, 20, 40, 220, 255, 20, 40, 220, 255, 20, 40, 220, 255,
		220, 20, 30, 255, 20, 220, 30, 255, 220, 200, 30, 255, 100, 20, 200, 255,
		120, 88, 128, 255, 120, 88, 128, 255, 120, 88, 128, 255, 120, 88, 128, 255,
		220, 80, 20, 255, 220, 80, 20, 255, 220, 80, 20, 255, 220, 80, 20, 255
	};
	result.insert(result.end(), std::begin(pixels), std::end(pixels));
	return result;
}

inline void write_texture_action_fixture(const std::filesystem::path &original, const std::filesystem::path &directory, const std::array<unsigned, 4> &indices)
{
	std::ifstream file(original, std::ios::binary);
	const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	const auto members = xl_compat::read_hog(bytes);
	const auto found = std::find_if(members.begin(), members.end(), [](const auto &row) { return row.name == "neptunxl.rl2"; });
	if (found == members.end()) throw std::runtime_error("Original Neptune texture-control geometry missing");
	const auto level = texture_action_level(found->bytes);
	const auto pog = texture_action_pog(indices);
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	for (const std::string name : { "tex23", "tex27", "noaction", "badbase", "badovl", "badlink", "badobject" }) {
		auto current = name == "tex27" ? runtime_layout_fixture(level, 27).bytes : level;
		const auto source = xl_compat::inspect_level(current);
		if (name == "noaction") {
			for (unsigned i = 0; i < source.triggers.size(); ++i)
				if (source.triggers[i].type == 19) current[source.sections[3].offset + i * 52] = 13;
			for (unsigned i = 0; i < source.object_trigger_records.size(); ++i)
				current[source.sections[3].offset + 12 * 52 + 4 + i * 53] = 13;
		}
		if (name == "badbase") layout_patch(current, source.sections[3].offset + 4, 65535);
		if (name == "badovl") layout_patch(current, source.sections[3].offset + 4, (910u << 16) | 1);
		if (name == "badlink") layout_patch(current, source.sections[3].offset + 32, 6, 2);
		if (name == "badobject") layout_patch(current, source.sections[3].offset + 12 * 52 + 4 + 5, 65535);
		hog_member(output, name + ".rl2", current, false);
		hog_member(output, name + ".pog", pog, false);
		for (const auto &member : members) {
			if (member.name.rfind("neptunxl.", 0) != 0) continue;
			const auto ext = member.name.substr(8);
			if (ext == ".rl2" || ext == ".pog" || ext == ".mn2") continue;
			hog_member(output, name + ext, member.bytes, false);
		}
	}
	std::filesystem::create_directories(directory);
	std::ofstream hog(directory / "xtexture.hog", std::ios::binary);
	hog.write(reinterpret_cast<const char *>(output.data()), std::streamsize(output.size()));
	std::ofstream descriptor(directory / "xtexture.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL textures\nnum_levels = 2\ntex23.rl2\ntex27.rl2\n";
	if (!hog || !descriptor) throw std::runtime_error("Cannot write isolated texture fixture");
}
} // namespace xl_test
#endif
