#ifndef DXX_XL_LEGACY_AUDIO_FIXTURE_H
#define DXX_XL_LEGACY_AUDIO_FIXTURE_H
#include "layout_fixture.h"
#include <cstring>

namespace xl_test
{
// Test-only encodings of the same dry geometry and ambient actions. This does
// not convert missions or qualify an original legacy campaign
inline xl_compat::Bytes legacy_audio_fixture(const xl_compat::Bytes &original, int game_version)
{
	const auto source = xl_compat::inspect_level(original);
	const int version = game_version == 36 ? 17 : 15;
	if (source.level_version != 23 || source.game_version != 40 ||
	    (game_version != 33 && game_version != 34 && game_version != 36) || !source.equipment.empty())
		throw std::runtime_error("Legacy audio fixture requires the bounded 23/40 source");
	const auto color_count = source.vertices.size() + source.segments.size() * 6 + 910;
	const auto colors = source.game_offset - color_count * 13;
	const auto extras = colors - source.segments.size() * 13;
	xl_compat::Bytes out(original.begin(), original.begin() + extras);
	layout_patch(out, 4, version);
	const auto put = [&](int value, int width) { layout_put(out, std::uint32_t(value), width); };
	for (const auto &segment : source.segments) {
		if (segment.function > 6 || segment.properties || segment.damage != std::array<int, 2>{} || (segment.flags & ~3))
			throw std::runtime_error("Legacy audio fixture cannot remap modern segment semantics");
		put(segment.function, 1);
		put(segment.producer, 1);
		put(segment.value, 1);
		put(segment.flags, 1);
		put(segment.light, 4);
	}
	const auto textures = colors + (color_count - 910) * 13;
	out.insert(out.end(), original.begin() + colors, original.begin() + textures);
	if (version == 15) {
		for (const auto &color : source.texture_colors) {
			put(color.index, 1);
			for (double value : color.rgb) {
				std::uint64_t bits;
				static_assert(sizeof(bits) == sizeof(value));
				std::memcpy(&bits, &value, sizeof(bits));
				for (unsigned byte = 0; byte < 8; ++byte) out.push_back(std::uint8_t(bits >> (8 * byte)));
			}
		}
	} else out.insert(out.end(), original.begin() + textures, original.begin() + source.game_offset);
	const auto game = out.size();
	const int header = version == 15 ? 143 : 155;
	layout_patch(out, 12, std::uint32_t(game));
	out.insert(out.end(), original.begin() + source.game_offset, original.begin() + source.game_offset + header);
	layout_patch(out, game + 2, game_version, 2);
	layout_patch(out, game + 4, header);
	out.insert(out.end(), original.begin() + source.game_offset + 155, original.end());
	const auto rebase = [&](std::size_t field, int old) {
		if (old <= 0) return;
		if (old < source.game_offset + 155 || std::size_t(old) > original.size())
			throw std::runtime_error("Legacy audio fixture section lies outside its source");
		layout_patch(out, game + field, std::uint32_t(old - source.game_offset - 155 + game + header));
	};
	std::uint32_t player = 0;
	for (unsigned i = 0; i < 4; ++i) player |= std::uint32_t(original.at(source.game_offset + 27 + i)) << (8 * i);
	rebase(27, std::int32_t(player));
	for (unsigned i = 0; i < (version == 15 ? 9u : 10u); ++i) rebase(35 + i * 12, source.sections[i].offset);
	// Append replacement tables; unrelated payload remains byte-for-byte intact
	layout_patch(out, game + 35, std::uint32_t(out.size()));
	for (const auto &object : source.objects) {
		if (object.multiplayer || (object.render >= 11 && object.render != 13))
			throw std::runtime_error("Legacy audio fixture contains an unhandled object payload");
		out.insert(out.end(), original.begin() + object.source_offset, original.begin() + object.source_offset + 6);
		const auto end = object.source_end - (object.render == 13 ? 1 : 0);
		out.insert(out.end(), original.begin() + object.source_offset + 7, original.begin() + end);
	}
	layout_patch(out, game + 47, std::uint32_t(out.size()));
	layout_patch(out, game + 55, 24);
	for (unsigned i = 0; i < source.walls.size(); ++i) {
		if (source.walls[i].flags > 255) throw std::runtime_error("Legacy audio fixture would narrow wall flags");
		const auto at = source.sections[1].offset + i * 25;
		out.insert(out.end(), original.begin() + at, original.begin() + at + 18);
		out.insert(out.end(), original.begin() + at + 19, original.begin() + at + 25);
	}
	layout_patch(out, game + 35 + 7 * 12, std::uint32_t(out.size()));
	for (const auto &light : source.light_indices) {
		if (game_version == 33 && light[2] > 255) throw std::runtime_error("Legacy audio fixture would narrow light counts");
		put(light[0], 2);
		put(game_version == 33 ? light[1] : light[1] | ((light[2] & 31) << 3), 1);
		put(game_version == 33 ? light[2] : light[2] >> 5, 1);
		put(light[3], 2);
	}
	layout_patch(out, game + 71, std::uint32_t(out.size()));
	const auto triggers = source.sections[3].offset;
	const auto actions = triggers + source.triggers.size() * 52 + 4;
	out.insert(out.end(), original.begin() + triggers, original.begin() + actions + source.object_trigger_records.size() * 53);
	for (const auto &record : source.object_trigger_records) {
		put(-1, 2);
		put(-1, 2);
		put(record.object, 2);
	}
	if (game_version < 36) {
		for (int i = 0; i < 700; ++i) {
			int head = -1;
			for (unsigned j = 0; j < source.object_trigger_records.size(); ++j)
				if (source.object_trigger_records[j].object == i) head = int(j);
			put(head, 2);
		}
	} else {
		put(int(source.object_trigger_records.size()), 2);
		for (unsigned i = 0; i < source.object_trigger_records.size(); ++i) {
			put(source.object_trigger_records[i].object, 2);
			put(int(i), 2);
		}
	}
	const auto result = xl_compat::inspect_level(out);
	if (result.level_version != version || result.game_version != game_version || result.vertices != source.vertices ||
	    result.objects.size() != source.objects.size() || result.object_trigger_records.size() != source.object_trigger_records.size() ||
	    result.light_indices != source.light_indices)
		throw std::runtime_error("Legacy audio encoding changed geometry, object counts or light records");
	return out;
}
} // namespace xl_test
#endif
