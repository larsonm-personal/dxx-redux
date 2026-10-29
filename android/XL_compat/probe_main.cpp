#include "probe.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>

namespace
{
void write_new(const std::filesystem::path &path, const std::string &bytes)
{
	std::ofstream out(path, std::ios::binary);
	out.write(bytes.data(), static_cast<std::streamsize>(bytes.size()));
	out.close();
	if (!out) throw std::runtime_error("cannot write " + path.string());
}
} // namespace

int main(int argc, char **argv)
{
	const bool source_runtime = argc > 1 && std::string(argv[1]) == "--source-runtime-probe";
	if (argc != 4 || (!source_runtime && std::string(argv[1]) != "--incomplete-presentation-probe")) {
		std::cerr << "Usage: xl_probe_fixture {--incomplete-presentation-probe|--source-runtime-probe} input.rl2 NEW-output-directory\n"
		             "Developer fixture only; not a supported mission conversion\n";
		return 2;
	}
	try {
		const auto directory = std::filesystem::path(argv[3]);
		if (std::filesystem::exists(directory)) throw std::runtime_error("probe output directory must not exist");
		std::ifstream input(argv[2], std::ios::binary | std::ios::ate);
		if (!input || input.tellg() < 0 || input.tellg() > 64 * 1024 * 1024) throw std::runtime_error("invalid input or input larger than 64 MiB");
		const auto size = input.tellg();
		input.seekg(0);
		const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		if (input.bad() || bytes.size() != static_cast<std::size_t>(size)) throw std::runtime_error("input read failed");
		const auto probe = source_runtime ? xl_compat::prepare_source_runtime_probe(bytes) : xl_compat::prepare_gameplay_probe(bytes);
		if (!std::filesystem::create_directory(directory)) throw std::runtime_error("cannot create new probe directory");
		// Write the limitations first so any partially written fixture is identified
		write_new(directory / "UNVERIFIED.txt", "Developer gameplay probe\nNOT verified playable or faithful\n"
		                                        "Embedded XL lighting is preserved in xlprobe.source.json but is NOT rendered by the stock loader\n"
		                                        "Asset dependencies, gameplay behavior and reference parity remain unverified\n"
		                                        "Do not publish or add this fixture to the supported mission catalog\n");
		write_new(directory / "xlprobe.source.json", xl_compat::inventory_json(probe.source));
		write_new(directory / "xlprobe.rl2", std::string(probe.stock_level.begin(), probe.stock_level.end()));
		std::filesystem::create_directory(directory / "missions");
		write_new(directory / "missions" / "xlprobe.mn2", "name = XL PROBE - UNVERIFIED\ntype = normal\nnum_levels = 1\nxlprobe.rl2\n");
		std::cout << "Created unverified gameplay probe; presentation is incomplete\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
