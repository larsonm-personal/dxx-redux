// Private negative controls derived from an original mission; never a conversion
#include "inventory.h"
#include <algorithm>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
using xl_compat::Bytes;
static void write(const std::filesystem::path &path, const Bytes &bytes)
{
	std::ofstream out(path, std::ios::binary);
	out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	if (!out) throw std::runtime_error("cannot write fixture");
}
int main(int argc, char **argv)
{
	try {
		if (argc != 3) throw std::runtime_error("requires original Neptune HOG and new output directory");
		const std::filesystem::path directory(argv[2]);
		if (std::filesystem::exists(directory)) throw std::runtime_error("fixture output must not exist");
		std::ifstream in(argv[1], std::ios::binary);
		if (!in) throw std::runtime_error("cannot open original HOG");
		const Bytes bytes{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
		const auto originals = xl_compat::read_hog(bytes);
		std::filesystem::create_directories(directory);
		for (int variant = 0; variant < 3; ++variant) {
			auto members = originals;
			bool changed = false;
			for (auto &member : members) {
				if (variant == 2 && member.name == "neptunxl.clr") {
					member.bytes.resize(1);
					changed = true;
				}
				if (variant == 2 || member.name != "neptunxl.rl2") continue;
				const auto level = xl_compat::inspect_level(member.bytes);
				if (level.level_version != 23 || level.game_version != 40 || level.segments.size() != 93 || level.objects.size() != 49) throw std::runtime_error("unexpected source layout");
				if (variant == 0) {
					const auto colors = (level.vertices.size() + level.segments.size() * 6 + 910) * 13;
					const auto first = level.game_offset - colors - level.segments.size() * 13;
					member.bytes.at(first + 4) = xl_compat::segment_outdoors;
					const auto control = xl_compat::inspect_level(member.bytes);
					if (!std::any_of(control.unsupported.begin(), control.unsupported.end(), [](const auto &issue) {
						    return issue.feature == "segment_properties" && issue.detail == "16";
					    })) throw std::runtime_error("Outdoor segments are no longer a negative control; update the fixture and Android rejection expectation");
				} else member.bytes.at(level.objects.front().source_offset + 6) = 1;
				changed = true;
			}
			if (!changed) throw std::runtime_error("negative control did not mutate a requirement");
			Bytes hog{ 'D', 'H', 'F' };
			for (const auto &member : members) {
				const auto start = hog.size();
				hog.insert(hog.end(), member.name.begin(), member.name.end());
				hog.resize(start + 13, 0);
				for (int i = 0; i < 4; ++i) hog.push_back(static_cast<unsigned char>(member.bytes.size() >> (8 * i)));
				hog.insert(hog.end(), member.bytes.begin(), member.bytes.end());
			}
			const std::string stem = "npbad" + std::to_string(variant);
			write(directory / (stem + ".hog"), hog);
			const auto descriptor = "d2x-name = Neptune rejected " + std::to_string(variant) + "\nnum_levels = 1\nneptunxl.rl2\n";
			write(directory / (stem + ".mn2"), Bytes(descriptor.begin(), descriptor.end()));
		}
		std::cout << "Created three private negative controls: outdoor property, multiplayer object, malformed CLR\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
