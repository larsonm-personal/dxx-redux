#include "layout_fixture.h"
#include "../admission.h"
#include "../compatibility.h"
#include "../probe.h"
#include "hog_fixture.h"
#include <chrono>
#include <filesystem>
#include <fstream>
#include <functional>
#include <iostream>

namespace
{
void check(bool ok, const char *why)
{
	if (!ok) throw std::runtime_error(why);
}
void rejects(const xl_compat::Bytes &bytes, const char *detail = nullptr)
{
	try {
		xl_compat::inspect_level(bytes);
	} catch (const std::runtime_error &e) {
		check(!detail || std::string(e.what()).find(detail) != std::string::npos, "Malformed layout has the wrong rejection reason");
		return;
	}
	throw std::runtime_error("Malformed layout accepted");
}
bool issue(const std::vector<xl_compat::Issue> &issues, const char *feature)
{
	return std::any_of(issues.begin(), issues.end(), [&](const auto &value) { return value.feature == feature; });
}
void contract()
{
	int cases = 0;
	for (int version : { 23, 25, 27 }) {
		for (int producer : { -32768, -129, -128, -1, 0, 127, 128, 32767 }) {
			if (version == 23 && (producer < -128 || producer > 127)) continue;
			for (int shape = 0; shape < (version == 23 ? 1 : 3); ++shape) {
				const auto fixture = xl_test::layout_fixture(version, shape, producer, producer);
				const auto parsed = xl_compat::inspect_level(fixture.bytes);
				check(parsed.level_version == version && parsed.game_version == 40 && parsed.segments.size() == 1 && parsed.walls.size() == 1, "Modern layout header or section boundary changed");
				const auto &segment = parsed.segments[0];
				check(segment.producer == producer && segment.value == producer, "Signed producer/value was narrowed");
				check(segment.properties == 32 && segment.damage == std::array<int, 2>{ -1, 32767 } && segment.flags == 3 && segment.light == 65536, "Wide extras displaced properties, damage or light");
				check(segment.sides[5].texture == -1 && segment.sides[5].corners[0] != 255, "Open portal lost explicit corners");
				check(segment.sides[2].overlay == 0xc005 && segment.sides[2].uvl[0] == -32768 && segment.sides[2].uvl[2] == 65535, "Side overlay/UVL boundary changed");
				check(issue(parsed.unsupported, "side_geometry") == (shape != 0), "Nonstandard corner mapping was not diagnosed");
				check(issue(parsed.unsupported, "segment_geometry") == (shape == 2), "Deleted vertex slots were not diagnosed");
				check(parsed.level_name == "Layout control" && parsed.fog_settings.size() == (version == 27 ? 4u : 0u), "Fog displaced level name or model list");
				if (version == 27) check(parsed.fog_settings[3] == std::array<int, 4>{ 255, 0, 130, 255 }, "Fog byte values were changed");
				const auto json = xl_compat::inventory_json(parsed);
				check((json.find("\"fog_settings\"") != std::string::npos) == (version == 27), "Fog JSON presence differs from source layout");
				check((json.find("\"corners\"") != std::string::npos) == (version >= 25), "Explicit corner JSON presence differs from source layout");
				if (version >= 25) check(issue(xl_compat::runtime_blockers(parsed), "serialization"), "New decoder bypassed runtime qualification");
				const auto report = xl_compat::compatibility_json("layout.rl2", fixture.bytes);
				check(report.find("Level version " + std::to_string(version) + ", game data 40") != std::string::npos && report.find("detailed inspection rejected") == std::string::npos, "Browser lost decoded format evidence");
				check(report.find("Explicit side geometry") != std::string::npos && report.find("Fog volumes and settings") != std::string::npos && report.find("1 fog-bearing volumes") != std::string::npos, "Browser omitted geometry or fog requirements");
				if (version >= 25) check(report.find("runtime layout remains unqualified") != std::string::npos, "Browser implied new layout runtime support");
				++cases;
			}
		}
		const auto fixture = xl_test::layout_fixture(version);
		for (std::size_t end = fixture.game; end < fixture.walls; ++end) {
			auto truncated = fixture.bytes;
			// Remove later section dependencies so the bounded game-header reader,
			// including every fog/name byte, is the path that rejects truncation
			for (int section = 0; section < 10; ++section) {
				xl_test::layout_patch(truncated, fixture.game + 35 + section * 12, 0xffffffff);
				xl_test::layout_patch(truncated, fixture.game + 39 + section * 12, 0);
			}
			truncated.resize(end);
			rejects(truncated);
		}
		std::vector<std::size_t> cuts;
		for (const auto corners : fixture.corners)
			for (int byte = 0; byte < (version >= 25 ? 4 : 2); ++byte) cuts.push_back(corners + byte);
		for (auto at = fixture.extras; at <= fixture.colors; ++at) cuts.push_back(at);
		for (const auto cut : cuts) {
			// Retain and rebase a valid game header after a shortened mine section
			// so a global EOF/section-offset check cannot mask missing mine fields
			xl_compat::Bytes truncated(fixture.bytes.begin(), fixture.bytes.begin() + cut);
			truncated.insert(truncated.end(), fixture.bytes.begin() + fixture.game, fixture.bytes.end());
			xl_test::layout_patch(truncated, 12, static_cast<std::uint32_t>(cut));
			xl_test::layout_patch(truncated, cut + 47, static_cast<std::uint32_t>(fixture.walls - fixture.game + cut));
			xl_test::layout_patch(truncated, cut + 71, static_cast<std::uint32_t>(fixture.walls + 25 - fixture.game + cut));
			rejects(truncated, "truncated section");
		}
		for (int invalid : { 24, 26, 28 }) {
			auto bytes = fixture.bytes;
			xl_test::layout_patch(bytes, 4, invalid);
			rejects(bytes, "unsupported level version");
		}
		if (version < 25) continue;
		for (const int corner : { 8, 128, 254 }) {
			auto bytes = fixture.bytes;
			bytes[fixture.corners[0]] = static_cast<unsigned char>(corner);
			rejects(bytes, "invalid side corner");
		}
		auto bytes = fixture.bytes;
		bytes[fixture.corners[0]] = 255;
		rejects(bytes, "invalid side corner");
		bytes = fixture.bytes;
		xl_test::layout_patch(bytes, fixture.vertices + 7 * 2, 0xffff, 2);
		rejects(bytes, "side references deleted vertex");
		bytes = fixture.bytes;
		xl_test::layout_patch(bytes, fixture.vertices, 0xfff7, 2);
		rejects(bytes, "invalid vertex");
		bytes = fixture.bytes;
		xl_test::layout_patch(bytes, fixture.game + 2, 36, 2);
		rejects(bytes, "unsupported level/game version pair");
		// Every shortened side still consumes four corner slots and four UVLs,
		// including source-degenerate zero-, one- and two-corner sides
		for (int active = 0; active < 4; ++active) {
			bytes = fixture.bytes;
			for (int n = active; n < 4; ++n) bytes[fixture.corners[0] + n] = 255;
			const auto decoded = xl_compat::inspect_level(bytes);
			check(issue(decoded.unsupported, "side_geometry") && decoded.segments[0].sides[1].texture == 2, "Degenerate side lost record alignment or admission gate");
		}
	}
	std::cout << cases << " modern layout profiles and malformed boundary controls passed\n";
}
void write_fixture(const std::filesystem::path &directory)
{
	std::filesystem::create_directories(directory);
	const auto fixture = xl_test::layout_fixture(27);
	const auto write = [](const std::filesystem::path &path, const xl_compat::Bytes &bytes) {
		std::ofstream out(path, std::ios::binary);
		out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
		check(bool(out), "Cannot write layout CLI fixture");
	};
	write(directory / "layout.rl2", fixture.bytes);
	constexpr std::uint32_t padding = 65 * 1024 * 1024;
	for (const bool oversized_level : { false, true }) {
		xl_compat::Bytes header{ 'D', '2', 'X' };
		xl_test::hog_member(header, oversized_level ? "large.rl2" : "large.bin", {}, false);
		xl_test::layout_patch(header, 16, padding);
		std::ofstream out(directory / (oversized_level ? "large-level.hog" : "stream.hog"), std::ios::binary);
		out.write(reinterpret_cast<const char *>(header.data()), static_cast<std::streamsize>(header.size()));
		out.seekp(padding - 1, std::ios::cur);
		out.put(0);
		xl_compat::Bytes tail;
		xl_test::hog_member(tail, "layout.rl2", fixture.bytes, true);
		out.write(reinterpret_cast<const char *>(tail.data()), static_cast<std::streamsize>(tail.size()));
		check(bool(out), "Cannot write streaming CLI fixture");
	}
	std::filesystem::copy_file(directory / "stream.hog", directory / "truncated.hog", std::filesystem::copy_options::overwrite_existing);
	std::ofstream invalid(directory / "truncated.hog", std::ios::binary | std::ios::app);
	invalid.put('x');
	invalid.close();
	std::filesystem::copy_file(directory / "stream.hog", directory / "duplicate.hog", std::filesystem::copy_options::overwrite_existing);
	xl_compat::Bytes duplicate;
	xl_test::hog_member(duplicate, "LAYOUT.RL2", fixture.bytes, false);
	std::ofstream duplicate_out(directory / "duplicate.hog", std::ios::binary | std::ios::app);
	duplicate_out.write(reinterpret_cast<const char *>(duplicate.data()), static_cast<std::streamsize>(duplicate.size()));
	std::ofstream raw(directory / "oversized.rl2", std::ios::binary);
	raw.close();
	std::filesystem::resize_file(directory / "oversized.rl2", padding);
}
} // namespace
int main(int argc, char **argv)
{
	try {
		if (argc == 3 && std::string(argv[1]) == "--write") write_fixture(argv[2]);
		else if (argc == 3 && std::string(argv[1]) == "--age") std::filesystem::last_write_time(argv[2], std::filesystem::file_time_type::clock::now() - std::chrono::hours(24));
		else {
			check(argc == 1, "Unexpected modern layout arguments");
			contract();
		}
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
