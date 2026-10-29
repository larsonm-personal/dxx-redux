#include "runtime_layout_fixture.h"
#include "hog_fixture.h"
#include "admission.h"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char **argv)
{
	try {
		if (argc != 3) throw std::runtime_error("Usage: xl_fog_fixture ORIGINAL_NEPTUNE_HOG OUTPUT_DIRECTORY");
		std::ifstream input(argv[1], std::ios::binary);
		if (!input) throw std::runtime_error("Cannot open original fog geometry control");
		const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		const auto members = xl_compat::read_hog(bytes);
		const auto level = std::find_if(members.begin(), members.end(), [](const auto &member) { return member.name == "neptunxl.rl2"; });
		if (level == members.end()) throw std::runtime_error("Missing Neptune geometry control");
		const auto source = xl_compat::inspect_level(level->bytes);
		if (source.level_version != 23 || source.game_version != 40 || source.segments.size() != 93 || source.objects.size() != 49) throw std::runtime_error("Unexpected fog geometry control");
		xl_compat::Bytes archive{ 'D', 'H', 'F' };
		const char *names[]{ "fw27", "fl27", "ff27", "fd27", "mix27", "zero27", "dry27" };
		const int properties[]{ 1, 2, 32, 64, -1, -1, 0 };
		const int settings[4][4]{ { 34, 112, 200, 5 }, { 209, 71, 13, 3 }, { 53, 117, 221, 6 }, { 161, 89, 209, 2 } };
		for (int variant = 0; variant < 7; ++variant) {
			auto fixture = xl_test::runtime_layout_fixture(level->bytes, 27);
			for (std::size_t i = 0; i < source.segments.size(); ++i) {
				const int fog = properties[variant] < 0 ? properties[i % 4] : properties[variant];
				fixture.bytes.at(fixture.extras[i] + 6) = static_cast<std::uint8_t>(fog | xl_compat::segment_no_damage);
			}
			for (int type = 0; type < 4; ++type)
				for (int component = 0; component < 4; ++component)
					fixture.bytes.at(fixture.fog + type * 4 + component) = static_cast<std::uint8_t>(variant == 5 && component == 3 ? 0 : settings[type][component]);
			const auto inspected = xl_compat::inspect_level(fixture.bytes);
			if (!xl_compat::runtime_blockers(inspected).empty()) throw std::runtime_error("Derived fog fixture has independent blockers");
			const std::string name = names[variant];
			xl_test::hog_member(archive, name + ".rl2", fixture.bytes, false);
			for (const auto &member : members) {
				if (member.name.size() <= 8 || member.name.substr(0, 8) != "neptunxl" || member.name == "neptunxl.rl2" || member.name == "neptunxl.mn2") continue;
				xl_test::hog_member(archive, name + member.name.substr(8), member.bytes, false);
			}
		}
		const std::filesystem::path directory(argv[2]);
		std::filesystem::create_directories(directory);
		std::ofstream hog(directory / "fogctrl.hog", std::ios::binary);
		hog.write(reinterpret_cast<const char *>(archive.data()), static_cast<std::streamsize>(archive.size()));
		std::ofstream descriptor(directory / "fogctrl.mn2", std::ios::binary);
		descriptor << "d2x-name = Derived XL fog\nnum_levels = 7\n";
		for (const char *name : names) descriptor << name << ".rl2\n";
		if (!hog || !descriptor) throw std::runtime_error("Cannot write fog control archive/descriptor");
		std::cout << "Created seven derived fog controls; original mission remains independently gated\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
