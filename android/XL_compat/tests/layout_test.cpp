#include "layout_fixture.h"
#include "runtime_layout_fixture.h"
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
				check(!issue(xl_compat::runtime_blockers(parsed), "serialization"), "Audited modern layout rejected by version alone");
				check(!xl_compat::runtime_blockers(parsed).empty(), "Fog/damage/shape requirements bypassed by layout support");
				if (version == 27) {
					auto missing = parsed;
					missing.fog_settings.clear();
					check(issue(xl_compat::runtime_blockers(missing), "authored fog"), "Missing authored fog table bypassed admission");
				}
				const auto report = xl_compat::compatibility_json("layout.rl2", fixture.bytes);
				check(report.find("Level version " + std::to_string(version) + ", game data 40") != std::string::npos && report.find("detailed inspection rejected") == std::string::npos, "Browser lost decoded format evidence");
				check(report.find("Explicit side geometry") != std::string::npos && report.find("Fog volumes and settings") != std::string::npos && report.find("1 fog-bearing volumes") != std::string::npos, "Browser omitted geometry or fog requirements");
				if (version >= 25) check(report.find("independent geometry, fog and gameplay requirements still apply") != std::string::npos, "Browser omitted independent runtime requirements");
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

void write_runtime_fixture(const std::filesystem::path &path, const std::filesystem::path &directory)
{
	std::ifstream input(path, std::ios::binary);
	check(bool(input), "Cannot open runtime layout source");
	const xl_compat::Bytes archive{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	const auto members = xl_compat::read_hog(archive);
	const auto found = std::find_if(members.begin(), members.end(), [](const auto &member) { return member.name == "neptunxl.rl2"; });
	check(found != members.end(), "Missing audited geometry source");
	auto base = found->bytes;
	const auto source = xl_compat::inspect_level(base);
	check(source.level_version == 23 && source.game_version == 40 && source.segments.size() == 93 && source.objects.size() == 49, "Unexpected runtime layout source");
	const auto extras = source.game_offset - (source.vertices.size() + source.segments.size() * 7 + 910) * 13;
	bool repair = false, protected_room = false;
	for (std::size_t i = 0; i < source.segments.size(); ++i) {
		if (!repair && source.segments[i].function == 1) {
			base.at(extras + i * 13) = 2;
			repair = true;
		} else if (!protected_room && !source.segments[i].function && !source.segments[i].properties) {
			base.at(extras + i * 13 + 4) = xl_compat::segment_no_damage;
			protected_room = true;
		}
	}
	check(repair && protected_room, "Source lacks repair/property control rooms");
	xl_compat::Bytes output{ 'D', 'H', 'F' }, reader{ 'D', 'H', 'F' };
	std::filesystem::create_directories(directory);
	std::ofstream descriptor(directory / "rtlayout.mn2", std::ios::binary);
	descriptor << "d2x-name = Derived XL layouts\nnum_levels = 4\nrt23.rl2\nrt25.rl2\nrt27.rl2\nfg27.rl2\n";
	for (int version : { 23, 25, 27 }) {
		auto fixture = version == 23 ? xl_test::RuntimeLayoutFixture{ base, {}, {}, 0 } : xl_test::runtime_layout_fixture(base, version);
		if (version >= 25) {
			for (std::size_t i = 0; i < fixture.extras.size(); ++i) {
				// Inert serialized fields must not alias a native station or robot maker
				const int function = fixture.bytes.at(fixture.extras[i]);
				xl_test::layout_patch(fixture.bytes, fixture.extras[i] + 1, function == 1 || function == 2 ? 499 : i % 2 ? 32767
				                                                                                                         : 32768,
				                      2);
				xl_test::layout_patch(fixture.bytes, fixture.extras[i] + 3, i % 2 ? 32768 : 32767, 2);
			}
		}
		const auto decoded = xl_compat::inspect_level(fixture.bytes);
		check(xl_compat::runtime_blockers(decoded).empty(), "Derived canonical runtime fixture remains blocked");
		const auto name = "rt" + std::to_string(version);
		xl_test::hog_member(output, name + ".rl2", fixture.bytes, false);
		xl_test::hog_member(reader, name + ".rl2", fixture.bytes, false);
		for (const auto &member : members) {
			if (member.name.size() <= 8 || member.name.substr(0, 8) != "neptunxl") continue;
			const auto extension = member.name.substr(8);
			if (extension == ".rl2" || extension == ".mn2" || extension == ".lgt" || extension == ".clr") continue;
			xl_test::hog_member(output, name + extension, member.bytes, false);
			if (version == 27) xl_test::hog_member(output, "fg27" + extension, member.bytes, false);
		}
		xl_compat::Bytes lgt, clr;
		for (int texture = 0; texture < 910; ++texture) {
			xl_test::layout_put(lgt, texture == 302 ? (version - 22) * 16384 : 0, 4);
			xl_test::layout_put(clr, 1, 1);
			for (int channel : { 0, 0, 2147483647 }) xl_test::layout_put(clr, channel, 4);
		}
		xl_test::hog_member(output, name + ".lgt", lgt, false);
		xl_test::hog_member(output, name + ".clr", clr, false);
		if (version == 27) {
			auto changed = fixture.bytes;
			changed.at(fixture.fog) ^= 255;
			xl_test::hog_member(output, "fg27.rl2", changed, false);
			xl_test::hog_member(reader, "fg27.rl2", changed, false);
			xl_test::hog_member(output, "fg27.lgt", lgt, false);
			xl_test::hog_member(output, "fg27.clr", clr, false);
			changed = fixture.bytes;
			changed.at(fixture.extras[0] + 6) |= 128;
			xl_test::hog_member(output, "badprop.rl2", changed, false);
			changed = fixture.bytes;
			const auto corners = fixture.corners[0][0];
			std::rotate(changed.begin() + corners, changed.begin() + corners + 1, changed.begin() + corners + 4);
			xl_test::hog_member(output, "badside.rl2", changed, false);
			changed.at(corners) = 8;
			xl_test::hog_member(output, "badcorn.rl2", changed, false);
		}
	}
	std::ofstream hog(directory / "rtlayout.hog", std::ios::binary);
	hog.write(reinterpret_cast<const char *>(output.data()), static_cast<std::streamsize>(output.size()));
	std::ofstream oracle(directory / "runtime-reader.hog", std::ios::binary);
	oracle.write(reinterpret_cast<const char *>(reader.data()), static_cast<std::streamsize>(reader.size()));
	check(bool(hog) && bool(descriptor) && bool(oracle), "Cannot write derived runtime layout package");
	std::cout << "Derived 23/25/27 canonical runtime package written; geometry unchanged, repair/property/light controls labeled\n";
}
} // namespace
int main(int argc, char **argv)
{
	try {
		if (argc == 4 && std::string(argv[1]) == "--runtime") write_runtime_fixture(argv[2], argv[3]);
		else if (argc == 3 && std::string(argv[1]) == "--write") write_fixture(argv[2]);
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
