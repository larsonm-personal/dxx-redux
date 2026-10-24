#include "mission_label.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>

namespace
{
void write(const std::filesystem::path &path, const std::string &bytes)
{
	std::ofstream file(path, std::ios::binary);
	if (!file.write(bytes.data(), bytes.size())) throw std::runtime_error("fixture write failed");
}
std::string level(unsigned version)
{
	return std::string("LVLP", 4) + char(version) + std::string(3, '\0');
}
std::string entry(std::string name, const std::string &bytes)
{
	name.resize(13, '\0');
	for (unsigned i = 0; i < 4; ++i) name += char(bytes.size() >> (8 * i));
	return name + bytes;
}
} // namespace
int main()
{
	const auto root = std::filesystem::current_path() / "mission-label-fixture";
	std::filesystem::create_directories(root);
	const auto descriptor = root / "fixture.mn2";
	const auto hog = root / "fixture.hog";
	const auto loose = root / "SECRET.RL2";
	try {
		auto check = [&](bool expected, const char *message) {
			if (bool(xl_mission_requires_extensions(descriptor.string().c_str())) != expected)
				throw std::runtime_error(message);
		};
		write(descriptor, "name = D2X-XL in title only\nnum_levels = 1\nnormal.rl2\nnum_secrets = 1\nsecret.rl2,1\n");
		write(loose, level(8));
		write(hog, "DHF" + entry("normal.rl2", level(8)) + entry("unused.rl2", level(15)));
		check(false, "title and unreferenced XL member must not label stock mission");
		write(hog, "DHF" + entry("normal.rl2", level(8)) + entry("SECRET.RL2", level(15)));
		check(true, "referenced secret XL level must label mission");
		write(hog, "DHF" + entry("normal.rl2", level(8)));
		write(loose, level(15));
		check(true, "case-insensitive loose XL level must label mission");
		write(loose, level(8));
		check(false, "stock loose and archive levels must stay unlabelled");
		write(hog, "DHF" + entry("normal.rl2", level(15)).substr(0, 19));
		check(false, "truncated archive must not imply XL");
		write(hog, "D2X");
		check(true, "extended HOG must label mission");
		write(hog, "DHF");
		write(descriptor, "D2X-NAME = Generic mission\n");
		check(true, "XL descriptor must label mission");
		std::filesystem::remove(descriptor);
		std::filesystem::remove(hog);
		std::filesystem::remove(loose);
		std::filesystem::remove(root);
		std::cout << "XL mission labels passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
