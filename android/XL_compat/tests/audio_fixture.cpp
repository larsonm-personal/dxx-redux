// Controlled sound actions on unchanged private Neptune geometry; synthetic PCM
#include "inventory.h"
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
	out.write(reinterpret_cast<const char *>(bytes.data()), std::streamsize(bytes.size()));
	require(bool(out), "Cannot write named-audio fixture");
}
Bytes wave(unsigned rate, unsigned channels, unsigned frames)
{
	Bytes bytes{ 'R', 'I', 'F', 'F' };
	xl_test::layout_put(bytes, 36 + frames * channels * 2, 4);
	for (char c : std::string("WAVEfmt ")) bytes.push_back(c);
	xl_test::layout_put(bytes, 16, 4);
	xl_test::layout_put(bytes, 1, 2);
	xl_test::layout_put(bytes, channels, 2);
	xl_test::layout_put(bytes, rate, 4);
	xl_test::layout_put(bytes, rate * channels * 2, 4);
	xl_test::layout_put(bytes, channels * 2, 2);
	xl_test::layout_put(bytes, 16, 2);
	for (char c : std::string("data")) bytes.push_back(c);
	xl_test::layout_put(bytes, frames * channels * 2, 4);
	for (unsigned frame = 0; frame < frames; ++frame)
		for (unsigned channel = 0; channel < channels; ++channel) {
			const auto value = int((frame * (channel ? 53 : 37) + (channel ? 3 : 0)) % 511) * 100 - 25500;
			xl_test::layout_put(bytes, std::uint16_t(value), 2);
		}
	return bytes;
}
Bytes actions(const Bytes &original)
{
	auto bytes = xl_test::expand_triggers(original, 10);
	const auto source = xl_compat::inspect_level(bytes);
	require(source.object_trigger_records.empty() && source.walls.size() > 1, "Unexpected audio fixture geometry");
	const auto table = source.sections[3].offset;
	const auto patch = [&](std::size_t at, int value, int size = 4) { xl_test::layout_patch(bytes, at, std::uint32_t(value), size); };
	const int ids[]{ 1, 2, 3, 1, 2, 0, 4, 1, 3, 1 };
	const int times[]{ 2, 0, -1, 1, 1, -1, 1000, 1, 3, 1 };
	for (int i = 0; i < 10; ++i) {
		const auto at = table + i * 52;
		std::fill(bytes.begin() + at, bytes.begin() + at + 52, 0);
		patch(at, i == 5 ? 31 : 30, 1);
		patch(at + 4, ids[i] * 65536 + (i == 0 ? 32768 : 0));
		patch(at + 8, times[i]);
		patch(at + 1, i == 3 ? 2 : i == 4 ? 128
		                       : i == 7   ? 4
		                       : i == 8   ? 8
		                                  : 0,
		      1);
		if (i == 5 || i == 6) {
			patch(at + 2, 1, 1);
			patch(at + 12, i == 5 ? source.walls[0].segment : -1, 2);
			patch(at + 32, i == 5 ? source.walls[0].side : -1, 2);
		}
	}
	patch(source.sections[1].offset + 20, 0, 1);
	std::vector<int> owners;
	for (std::size_t i = 0; i < source.objects.size() && owners.size() < 2; ++i)
		if (source.objects[i].type == 7) owners.push_back(int(i));
	require(owners.size() == 2, "Audio fixture lacks object-removal owners");
	Bytes payload;
	for (unsigned i = 0; i < owners.size(); ++i) {
		xl_test::layout_put(payload, 30, 1);
		xl_test::layout_put(payload, i ? 128 : 0, 2);
		xl_test::layout_put(payload, 0, 1);
		xl_test::layout_put(payload, 0, 1);
		xl_test::layout_put(payload, (i ? 2 : 1) * 65536, 4);
		xl_test::layout_put(payload, i ? 0 : 2, 4);
		payload.resize(payload.size() + 40);
	}
	for (int owner : owners) xl_test::layout_put(payload, owner, 2);
	const auto at = table + 10 * 52;
	patch(at, int(owners.size()));
	bytes.insert(bytes.begin() + at + 4, payload.begin(), payload.end());
	for (std::size_t i = 0; i < source.sections.size(); ++i)
		if (source.sections[i].offset >= at + 4) patch(source.game_offset + 35 + i * 12, source.sections[i].offset + int(payload.size()));
	const auto result = xl_compat::inspect_level(bytes);
	require(result.triggers.size() == 10 && result.object_trigger_records.size() == 2, "Audio fixture lost authored actions");
	return bytes;
}

Bytes constant_wave(int left, int right)
{
	auto bytes = wave(44100, 2, 22050);
	for (unsigned frame = 0; frame < 22050; ++frame) {
		xl_test::layout_patch(bytes, 44 + frame * 4, std::uint16_t(left), 2);
		xl_test::layout_patch(bytes, 46 + frame * 4, std::uint16_t(right), 2);
	}
	return bytes;
}

void scopes(const Bytes &level, const std::vector<xl_compat::Member> &members, const std::filesystem::path &directory)
{
	Bytes archive{ 'D', 'H', 'F' };
	for (const std::string name : { "scope23", "scope27" }) {
		xl_test::hog_member(archive, name + ".rl2", name == "scope27" ? xl_test::runtime_layout_fixture(level, 27).bytes : level, false);
		xl_test::hog_member(archive, name + ".snd", text("1:shared.wav\n2:scoped.wav\n3:mission.wav\n4:tiny.wav"), false);
		for (const auto &member : members) {
			if (member.name.size() <= 8 || member.name.substr(0, 8) != "neptunxl") continue;
			const auto extension = member.name.substr(8);
			if (extension == ".rl2" || extension == ".mn2" || extension == ".snd") continue;
			xl_test::hog_member(archive, name + extension, member.bytes, false);
		}
	}
	xl_test::hog_member(archive, "shared.wav", constant_wave(1000, -2000), false);
	xl_test::hog_member(archive, "tiny.wav", wave(44100, 2, 1), false);
	write(directory / "xscope.hog", archive);
	write(directory / "xscope.mn2", text("d2x-name = Derived XL sound scopes\nnum_levels = 3\nscope23.rl2\nscope23.rl2\nscope27.rl2\nnum_secrets = 1\nscope23.rl2,1\n"));
	const auto root = directory.parent_path() / "mods" / "xscope" / "sounds";
	std::filesystem::create_directories(root);
	write(root / "scoped.wav", constant_wave(8000, -4000));
	write(root / "mission.wav", constant_wave(1500, -1500));
	write(root / "shared.wav", text("Malformed lower-priority mission file must not shadow the HOG"));
	for (const std::string folder : { "level00", "level01", "level02", "slevel01" }) {
		std::filesystem::create_directories(root / folder);
		const int left = folder == "level01" ? 2500 : folder == "level02" ? -3000
		                                          : folder == "slevel01"  ? 11000
		                                                                  : 12345;
		const int right = folder == "level01" ? -7500 : folder == "level02" ? 9000
		                                            : folder == "slevel01"  ? -5000
		                                                                    : -12345;
		write(root / folder / "scoped.wav", constant_wave(left, right));
		write(root / folder / "shared.wav", text("Malformed lower-priority level file must not shadow the HOG"));
	}
	std::cout << "Derived XL sound scopes: HOG, normal/secret ordinals, duplicate filenames and mission fallback\n";
}

void fixture(const char *original, const std::filesystem::path &directory)
{
	std::ifstream input(original, std::ios::binary);
	require(bool(input), "Missing named-audio geometry source");
	const Bytes archive{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
	const auto members = xl_compat::read_hog(archive);
	const auto found = std::find_if(members.begin(), members.end(), [](const auto &row) { return row.name == "neptunxl.rl2"; });
	require(found != members.end(), "Missing original Neptune level");
	const auto source = xl_compat::inspect_level(found->bytes);
	require(source.level_version == 23 && source.game_version == 40 && source.segments.size() == 93 && source.objects.size() == 49, "Unexpected audio geometry source");
	const auto level = actions(found->bytes);
	const std::string sounds = "1:sound.wav\r\n2:long.wav\r\n3:mono.wav\r\n4:tiny.wav";
	Bytes output{ 'D', 'H', 'F' }, changed{ 'D', 'H', 'F' };
	for (const std::string name : { "audio23", "audio27", "nosnd", "badsnd", "badid", "badwave" }) {
		xl_test::hog_member(output, name + ".rl2", name == "audio27" ? xl_test::runtime_layout_fixture(level, 27).bytes : level, false);
		if (name == "audio23") xl_test::hog_member(changed, name + ".rl2", level, false);
		if (name != "nosnd") {
			const auto value = name == "badsnd" ? "1:First\n1:Duplicate" : name == "badid" ? "1:sound.wav"
			                                                           : name == "badwave" ? "1:broken.wav\n2:long.wav\n3:mono.wav\n4:tiny.wav"
			                                                                               : sounds;
			xl_test::hog_member(output, name + ".snd", text(value), false);
			if (name == "audio23") xl_test::hog_member(changed, name + ".snd", text(sounds + "\r\n99:Changed identity"), false);
		}
		for (const auto &member : members) {
			if (member.name.size() <= 8 || member.name.substr(0, 8) != "neptunxl") continue;
			const auto extension = member.name.substr(8);
			if (extension == ".rl2" || extension == ".mn2" || extension == ".snd") continue;
			xl_test::hog_member(output, name + extension, member.bytes, false);
			if (name == "audio23") xl_test::hog_member(changed, name + extension, member.bytes, false);
		}
	}
	for (auto *archive_out : { &output, &changed }) {
		xl_test::hog_member(*archive_out, "sound.wav", wave(44100, 2, 22050), false);
		xl_test::hog_member(*archive_out, "long.wav", wave(44100, 2, 44100), false);
		xl_test::hog_member(*archive_out, "mono.wav", wave(22050, 1, 11025), false);
		xl_test::hog_member(*archive_out, "tiny.wav", wave(44100, 2, 1), false);
		xl_test::hog_member(*archive_out, "broken.wav", text("not a RIFF WAVE"), false);
	}
	std::filesystem::create_directories(directory);
	write(directory / "xsound.hog", output);
	write(directory / "xsound.mn2", text("d2x-name = Derived XL sounds\nnum_levels = 2\naudio23.rl2\naudio27.rl2\n"));
	write(directory / "xschange.hog", changed);
	write(directory / "xschange.mn2", text("d2x-name = Changed XL sounds\nnum_levels = 1\naudio23.rl2\n"));
	scopes(level, members, directory);
	std::cout << "Derived XL sound packages: synthetic stereo/mono PCM, wall/object actions, 23/27 layouts and rejection controls\n";
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
