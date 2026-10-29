// Labeled message controls derived from unchanged private Neptune geometry
#include "inventory.h"
#include "admission.h"
#include "hog_fixture.h"
#include "runtime_layout_fixture.h"
#include "trigger_capacity_fixture.h"
#include <filesystem>
#include <fstream>
#include <iostream>

namespace
{
using xl_compat::Bytes;
void require(bool value, const char *reason)
{
	if (!value) throw std::runtime_error(reason);
}
Bytes text(const std::string &value)
{
	return Bytes(value.begin(), value.end());
}
void write(const std::filesystem::path &path, const Bytes &bytes)
{
	std::ofstream out(path, std::ios::binary);
	out.write(reinterpret_cast<const char *>(bytes.data()), static_cast<std::streamsize>(bytes.size()));
	require(bool(out), "Cannot write message fixture");
}
Bytes actions(const Bytes &original)
{
	auto bytes = xl_test::expand_triggers(original, 10);
	auto source = xl_compat::inspect_level(bytes);
	require(source.object_trigger_records.empty() && source.walls.size() > 1, "Unexpected message fixture geometry");
	const auto table = source.sections[3].offset;
	const auto patch = [&](std::size_t at, int value, int size = 4) { xl_test::layout_patch(bytes, at, static_cast<std::uint32_t>(value), size); };
	for (int i = 0; i < 10; ++i) {
		const auto at = table + i * 52;
		std::fill(bytes.begin() + at, bytes.begin() + at + 52, 0);
		patch(at, i == 5 ? 31 : 29, 1);
		patch(at + 4, i == 5 ? 0 : (i + 1) * 65536 + (i == 0 ? 32768 : 0));
		patch(at + 8, i == 1 || i == 8 ? 3 : i == 2         ? 1
		                                 : i == 3 || i == 5 ? -1
		                                 : i == 6           ? 1000
		                                 : i == 7           ? 0
		                                                    : 100);
		patch(at + 1, i == 1 ? 2 : i == 4 ? 128
		                       : i == 8   ? 4
		                       : i == 9   ? 8
		                                  : 0,
		      1);
		if (i == 5 || i == 6) {
			patch(at + 2, 1, 1);
			patch(at + 12, i == 5 ? source.walls[0].segment : -1, 2);
			patch(at + 32, i == 5 ? source.walls[0].side : -1, 2);
		}
	}
	patch(source.sections[1].offset + 20, 0, 1);
	// The real object-removal hook uses a pre-existing powerup owner
	std::vector<int> owners;
	for (std::size_t i = 0; i < source.objects.size() && owners.size() < 2; ++i)
		if (source.objects[i].type == 7) owners.push_back(static_cast<int>(i));
	require(owners.size() == 2, "Message fixture lacks powerup event owners");
	Bytes payload;
	for (unsigned i = 0; i < owners.size(); ++i) {
		xl_test::layout_put(payload, 29, 1);
		xl_test::layout_put(payload, i ? 128 : 0, 2);
		xl_test::layout_put(payload, 0, 1);
		xl_test::layout_put(payload, 0, 1);
		xl_test::layout_put(payload, (11 + i) * 65536, 4);
		xl_test::layout_put(payload, 100, 4);
		payload.resize(payload.size() + 40);
	}
	for (int owner : owners) xl_test::layout_put(payload, owner, 2);
	const auto at = table + 10 * 52;
	patch(at, static_cast<int>(owners.size()));
	bytes.insert(bytes.begin() + at + 4, payload.begin(), payload.end());
	for (std::size_t i = 0; i < source.sections.size(); ++i)
		if (source.sections[i].offset >= at + 4) patch(source.game_offset + 35 + i * 12, source.sections[i].offset + static_cast<int>(payload.size()));
	source = xl_compat::inspect_level(bytes);
	require(source.triggers.size() == 10 && source.object_trigger_records.size() == 2, "Message fixture lost authored actions");
	const auto blockers = xl_compat::runtime_blockers(source);
	if (!blockers.empty()) throw std::runtime_error(xl_compat::blocker_message(blockers));
	return bytes;
}
void fixture(const char *original, const std::filesystem::path &directory)
{
	std::ifstream in(original, std::ios::binary);
	require(bool(in), "Missing message geometry source");
	const Bytes archive{ std::istreambuf_iterator<char>(in), std::istreambuf_iterator<char>() };
	const auto members = xl_compat::read_hog(archive);
	const auto found = std::find_if(members.begin(), members.end(), [](const auto &row) { return row.name == "neptunxl.rl2"; });
	require(found != members.end(), "Missing original Neptune level");
	const auto source = xl_compat::inspect_level(found->bytes);
	require(source.level_version == 23 && source.game_version == 40 && source.segments.size() == 93 && source.objects.size() == 49, "Unexpected message geometry source");
	const auto level = actions(found->bytes);
	std::string messages = "1:Wall message\\nSecond line\r\n2:Replacement\r\n3:Short message\r\n4:Modal message - close to continue\r\n5:Autoplay\r\n7:Long duration\r\n8:Zero duration\r\n9:Disabled\r\n10:Repeat message\r\n11:Collected object message\r\n12:Object autoplay";
	Bytes output{ 'D', 'H', 'F' }, changed{ 'D', 'H', 'F' };
	for (const std::string name : { "msg23", "msg27", "nomsg", "badmsg", "badid", "badsnd" }) {
		auto current = name == "msg27" ? xl_test::runtime_layout_fixture(level, 27).bytes : level;
		if (name == "badsnd") current.at(xl_compat::inspect_level(current).sections[3].offset) = 30;
		xl_test::hog_member(output, name + ".rl2", current, false);
		if (name == "msg23") xl_test::hog_member(changed, name + ".rl2", current, false);
		if (name != "nomsg") {
			const auto value = name == "badmsg" ? "1:First\n1:Duplicate" : name == "badid" ? "1:Only first ID"
			                                                                               : messages;
			xl_test::hog_member(output, name + ".msg", text(value), false);
			if (name == "msg23") xl_test::hog_member(changed, name + ".msg", text(messages + "\r\n99:Changed resource"), false);
		}
		for (const auto &member : members) {
			if (member.name.size() <= 8 || member.name.substr(0, 8) != "neptunxl") continue;
			const auto extension = member.name.substr(8);
			if (extension == ".rl2" || extension == ".mn2" || extension == ".msg") continue;
			xl_test::hog_member(output, name + extension, member.bytes, false);
			if (name == "msg23") xl_test::hog_member(changed, name + extension, member.bytes, false);
		}
	}
	std::filesystem::create_directories(directory);
	write(directory / "xmessage.hog", output);
	write(directory / "xmessage.mn2", text("d2x-name = Derived XL messages\nnum_levels = 2\nmsg23.rl2\nmsg27.rl2\n"));
	write(directory / "xchanged.hog", changed);
	write(directory / "xchanged.mn2", text("d2x-name = Changed XL messages\nnum_levels = 1\nmsg23.rl2\n"));
	std::cout << "Derived XL message packages written: wall/object actions, 23/27 layouts and resource rejection controls\n";
}
} // namespace
int main(int argc, char **argv)
{
	try {
		require(argc == 3, "Expected original HOG and output directory");
		fixture(argv[1], argv[2]);
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
