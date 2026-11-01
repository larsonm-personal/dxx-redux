#include "layout_fixture.h"
#include "hog_fixture.h"
#include "admission.h"
#include <filesystem>
#include <fstream>
#include <iostream>

int main(int argc, char **argv)
{
	try {
		if (argc != 3) throw std::runtime_error("Usage: xl_bubble_fixture ORIGINAL_PUMO_HOG OUTPUT_DIRECTORY");
		std::ifstream input(argv[1], std::ios::binary);
		if (!input) throw std::runtime_error("Cannot read original Pumo archive");
		const xl_compat::Bytes original{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		const auto members = xl_compat::read_hog(original);
		const auto member = [&](const char *name) -> const xl_compat::Bytes & {
			const auto found = std::find_if(members.begin(), members.end(), [&](const auto &item) { return item.name == name; });
			if (found == members.end()) throw std::runtime_error("Missing original bubble fixture member");
			return found->bytes;
		};
		const auto &hub = member("pm-hub.rl2"), &mine = member("pmlev3.rl2");
		const auto source = xl_compat::inspect_level(hub), bubble_source = xl_compat::inspect_level(mine);
		const auto &smoke = source.objects.at(1), &bubble = bubble_source.objects.at(316);
		const std::vector<int> payload{ 2, 2, 4, 2, 2, 9, 0, 0, 0, 100, 0, 2, 1 };
		if (!xl_compat::modern_runtime_layout(source) || source.segments.size() != 439 || smoke.segment != 59 ||
		    smoke.type != 20 || smoke.render != 11 || smoke.render_data.size() != 13 || smoke.render_data[11] != 0 ||
		    bubble.render != 11 || bubble.render_data != payload || bubble.segment != 4151 ||
		    !(bubble_source.segments.at(4151).properties & xl_compat::segment_water))
			throw std::runtime_error("Original Pumo bubble or hub profile differs");
		const std::size_t colors = (source.vertices.size() + source.segments.size() * 6 + 910) * 13;
		const std::size_t extras_size = source.segments.size() * 15;
		if (std::size_t(source.game_offset) < colors + extras_size) throw std::runtime_error("Missing hub segment metadata");
		const std::size_t extras = std::size_t(source.game_offset) - colors - extras_size;
		for (std::size_t i = 0; i < source.segments.size(); ++i)
			if (hub.at(extras + i * 15) != source.segments[i].function || hub.at(extras + i * 15 + 6) != source.segments[i].properties)
				throw std::runtime_error("Bubble fixture metadata offsets differ from decoded geometry");
		const std::filesystem::path directory(argv[2]);
		std::filesystem::create_directories(directory);
		for (bool wet : { true, false }) {
			auto derived_bytes = hub;
			// Copy the actual serialized payload; geometry, object indices and basis stay unchanged
			std::copy_n(mine.begin() + bubble.source_end - 31, 31, derived_bytes.begin() + smoke.source_end - 31);
			const auto water_offset = extras + std::size_t(smoke.segment) * 15 + 6;
			derived_bytes.at(water_offset) = (source.segments[smoke.segment].properties & ~xl_compat::segment_water) | (wet ? xl_compat::segment_water : 0);
			const auto derived = xl_compat::inspect_level(derived_bytes);
			if (derived.objects[1].render_data != payload || derived.vertices != source.vertices || derived.objects.size() != source.objects.size() ||
			    derived.objects[1].orientation != smoke.orientation || derived.objects[1].position != smoke.position || derived.walls.size() != source.walls.size())
				throw std::runtime_error("Bubble fixture changed geometry, orientation or payload");
			for (std::size_t i = 0; i < hub.size(); ++i)
				if (hub[i] != derived_bytes[i] && i != water_offset && (i < smoke.source_end - 31 || i >= smoke.source_end))
					throw std::runtime_error("Bubble fixture changed an unrelated source byte");
			if (!xl_compat::runtime_blockers(derived).empty()) throw std::runtime_error("Derived original-profile bubble remains unsupported");
			for (unsigned field = 0; field < 12; ++field) {
				if (field == 11) continue;
				auto unsupported = derived;
				++unsupported.objects[1].render_data[field];
				const auto blockers = xl_compat::runtime_blockers(unsupported);
				if (std::none_of(blockers.begin(), blockers.end(), [](const auto &issue) { return issue.feature == "effect_particles" && issue.record == 1; }))
					throw std::runtime_error("Unqualified bubble payload variation passed admission");
			}
			auto disabled = derived;
			disabled.objects[1].render_data[12] = 0;
			if (!xl_compat::runtime_blockers(disabled).empty()) throw std::runtime_error("Qualified disabled bubble failed admission");
			xl_compat::Bytes archive{ 'D', 'H', 'F' };
			for (const auto &item : members) xl_test::hog_member(archive, item.name, item.name == "pm-hub.rl2" ? derived_bytes : item.bytes, false);
			const std::string name = wet ? "pbubble" : "pbubdry";
			std::ofstream hog(directory / (name + ".hog"), std::ios::binary);
			hog.write(reinterpret_cast<const char *>(archive.data()), std::streamsize(archive.size()));
			std::ofstream descriptor(directory / (name + ".mn2"));
			descriptor << "d2x-name = Derived Pumo " << (wet ? "wet" : "dry") << " bubbles\nnum_levels = 1\npm-hub.rl2\n";
			if (!hog || !descriptor) throw std::runtime_error("Cannot write derived bubble mission");
			std::cout << "Derived " << name << ": source payload316 copied to hub object1, segment59 water=" << wet << ", layout=" << source.level_version << '/' << source.game_version << "; all other bytes preserved\n";
		}
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
