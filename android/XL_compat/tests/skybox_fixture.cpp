#include "layout_fixture.h"
#include "hog_fixture.h"
#include "admission.h"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char **argv)
{
	try {
		if (argc != 3) throw std::runtime_error("Usage: xl_skybox_fixture ORIGINAL_PUMO_HOG OUTPUT_DIRECTORY");
		std::ifstream input(argv[1], std::ios::binary);
		if (!input) throw std::runtime_error("Cannot read original Pumo archive");
		const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		auto members = xl_compat::read_hog(original);
		auto hub = std::find_if(members.begin(), members.end(), [](const auto &member) { return member.name == "pm-hub.rl2"; });
		if (hub == members.end()) throw std::runtime_error("Original hub member is missing");
		const auto source = xl_compat::inspect_level(hub->bytes);
		if (!xl_compat::modern_runtime_layout(source) || source.segments.size() != 439 || source.segments.at(154).function != 10 || source.segments[154].properties != 60)
			throw std::runtime_error("Unexpected authored Pumo hub layout");
		const std::size_t colors = (source.vertices.size() + source.segments.size() * 6 + 910) * 13;
		const std::size_t extras_size = source.segments.size() * 15;
		if (std::size_t(source.game_offset) < colors + extras_size) throw std::runtime_error("Missing authored segment metadata");
		const std::size_t extras = std::size_t(source.game_offset) - colors - extras_size;
		for (std::size_t i = 0; i < source.segments.size(); ++i)
			if (hub->bytes.at(extras + i * 15) != source.segments[i].function || hub->bytes.at(extras + i * 15 + 6) != source.segments[i].properties)
				throw std::runtime_error("Derived metadata offsets do not match decoded source");
		// Isolate outdoors behavior, retaining blocked/no-damage/light-fog properties
		hub->bytes.at(extras + 154 * 15 + 6) = 60 & ~xl_compat::segment_outdoors;
		const auto &smoke = source.objects.at(1);
		if (smoke.type != 20 || smoke.render != 11 || smoke.render_data.size() != 13 || smoke.render_data[11] != 0 || smoke.control || smoke.movement || smoke.multiplayer || hub->bytes.at(smoke.source_end - 2) != 0)
			throw std::runtime_error("Unexpected authored smoke owner");
		// A labelled fire substitute isolates smoke while retaining object indices
		hub->bytes.at(smoke.source_end - 2) = 3;
		const auto derived = xl_compat::inspect_level(hub->bytes);
		const auto blockers = xl_compat::runtime_blockers(derived);
		if (!blockers.empty()) throw std::runtime_error(xl_compat::blocker_message(blockers));
		if (derived.vertices != source.vertices || derived.objects.size() != source.objects.size() || derived.walls.size() != source.walls.size())
			throw std::runtime_error("Derived fixture changed authored geometry or indices");
		unsigned changed = 0;
		const auto original_members = xl_compat::read_hog(original);
		const auto before = std::find_if(original_members.begin(), original_members.end(), [](const auto &member) { return member.name == "pm-hub.rl2"; });
		for (std::size_t i = 0; i < hub->bytes.size(); ++i) changed += hub->bytes[i] != before->bytes.at(i);
		if (changed != 2) throw std::runtime_error("Skybox fixture must change exactly two authored bytes");
		xl_compat::Bytes archive{ 'D', 'H', 'F' };
		for (const auto &member : members) xl_test::hog_member(archive, member.name, member.bytes, false);
		const std::filesystem::path directory(argv[2]);
		std::filesystem::create_directories(directory);
		std::ofstream hog(directory / "pskybox.hog", std::ios::binary);
		hog.write(reinterpret_cast<const char *>(archive.data()), std::streamsize(archive.size()));
		std::ofstream descriptor(directory / "pskybox.mn2", std::ios::binary);
		descriptor << "d2x-name = Derived Pumo skybox\nnum_levels = 1\npm-hub.rl2\n";
		if (!hog || !descriptor) throw std::runtime_error("Cannot write derived skybox archive");
		std::cout << "Derived Pumo skybox: exactly two changed bytes (outdoors cleared, smoke replaced by fire); authored geometry/resources retained; original hub remains unqualified\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
