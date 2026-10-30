#ifndef DXX_XL_SEISMIC_FIXTURE_H
#define DXX_XL_SEISMIC_FIXTURE_H
#include "countdown_fixture.h"
namespace xl_test
{
inline void write_seismic_fixture(const std::filesystem::path &original, const std::filesystem::path &directory)
{
	std::ifstream file(original, std::ios::binary);
	const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>() };
	const auto members = xl_compat::read_hog(bytes);
	const auto found = std::find_if(members.begin(), members.end(), [](const auto &row) { return row.name == "neptunxl.rl2"; });
	if (found == members.end()) throw std::runtime_error("Original seismic control geometry missing");
	auto level = countdown_level(found->bytes);
	const auto source = xl_compat::inspect_level(level);
	const auto table = source.sections[3].offset;
	for (unsigned i = 0; i < source.triggers.size(); ++i) {
		if (i == 4) continue;
		layout_patch(level, table + i * 52, 36, 1);
		layout_patch(level, table + i * 52 + 4, i == 2 ? std::uint32_t(-65536) : 3 * 65536);
	}
	// One exact original Pumo action alongside long, default and disabled cases
	layout_patch(level, table + 7 * 52 + 1, 3, 1);
	layout_patch(level, table + 7 * 52 + 2, 0, 1);
	layout_patch(level, table + 7 * 52 + 8, 3);
	for (unsigned i = 0; i < 2; ++i) layout_patch(level, table + 8 * 52 + 4 + i * 53, 36, 1);
	xl_compat::Bytes output{ 'D', 'H', 'F' };
	for (const std::string name : { "quake23", "quake27", "badquake", "badqobj" }) {
		auto current = name == "quake27" ? runtime_layout_fixture(level, 27).bytes : level;
		const auto info = xl_compat::inspect_level(current);
		if (name == "badquake") layout_patch(current, info.sections[3].offset + 8, 32768);
		if (name == "badqobj") layout_patch(current, info.sections[3].offset + 8 * 52 + 4 + 9, 32768);
		hog_member(output, name + ".rl2", current, false);
		for (const auto &member : members) {
			if (member.name.rfind("neptunxl.", 0) != 0) continue;
			const auto ext = member.name.substr(8);
			if (ext != ".rl2" && ext != ".mn2") hog_member(output, name + ext, member.bytes, false);
		}
	}
	std::filesystem::create_directories(directory);
	std::ofstream hog(directory / "xseismic.hog", std::ios::binary);
	hog.write(reinterpret_cast<const char *>(output.data()), std::streamsize(output.size()));
	std::ofstream descriptor(directory / "xseismic.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL seismic\nnum_levels = 2\nquake23.rl2\nquake27.rl2\n";
	if (!hog || !descriptor) throw std::runtime_error("Cannot write isolated seismic fixture");
}
} // namespace xl_test
#endif
