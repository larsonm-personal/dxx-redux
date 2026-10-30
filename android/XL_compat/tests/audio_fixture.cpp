// Controlled sound actions on unchanged private Neptune geometry; synthetic PCM
#include "inventory.h"
#include "hog_fixture.h"
#include "runtime_layout_fixture.h"
#include "legacy_audio_fixture.h"
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

Bytes ambient_objects(const Bytes &source)
{
	auto bytes = source;
	const auto level = xl_compat::inspect_level(source);
	const auto player = std::find_if(level.objects.begin(), level.objects.end(), [](const auto &object) { return object.type == 4; });
	require(player != level.objects.end() && level.level_version == 23 && level.objects.size() == 49, "Ambient fixture lost its original player/object layout");
	xl_test::layout_patch(bytes, level.game_offset + 35, unsigned(bytes.size()));
	xl_test::layout_patch(bytes, level.game_offset + 39, unsigned(level.objects.size() + 3));
	for (const auto &object : level.objects)
		bytes.insert(bytes.end(), source.begin() + object.source_offset, source.begin() + object.source_end);
	for (int index = 0; index < 3; ++index) {
		Bytes object(source.begin() + player->source_offset, source.begin() + player->source_offset + 80);
		object[0] = 20;
		object[1] = 7; // The real XL reader normalizes authored IDs to SOUND_ID
		object[2] = object[3] = object[5] = object[6] = 0;
		object[4] = 13;
		object[77] = object[78] = 255;
		object[79] = 0;
		xl_test::layout_patch(object, 9, std::uint32_t(player->position[0] + (index + 1) * 16384));
		xl_test::layout_patch(object, 17, std::uint32_t(player->position[2] + (index + 1) * 16384));
		xl_test::layout_patch(object, 57, 8192);
		const char *name = index == 0 ? "SOUND" : index == 1 ? "mono"
		                                                     : "long";
		const auto offset = object.size();
		object.resize(offset + 40);
		std::copy(name, name + std::char_traits<char>::length(name), object.begin() + offset);
		xl_test::layout_put(object, index == 0 ? 5 : index == 1 ? 10
		                                                        : 3,
		                    4);
		xl_test::layout_put(object, index == 1 ? 0 : 1, 1);
		bytes.insert(bytes.end(), object.begin(), object.end());
	}
	const auto action = [&](int index, int type, int flags, std::initializer_list<int> targets) {
		const auto at = level.sections[3].offset + index * 52;
		std::fill(bytes.begin() + at, bytes.begin() + at + 52, 0);
		xl_test::layout_patch(bytes, at, type, 1);
		xl_test::layout_patch(bytes, at + 1, flags, 1);
		xl_test::layout_patch(bytes, at + 2, unsigned(targets.size()), 1);
		xl_test::layout_patch(bytes, at + 8, 0xffffffff);
		int link = 0;
		for (int target : targets) {
			xl_test::layout_patch(bytes, at + 12 + link * 2, unsigned(target), 2);
			xl_test::layout_patch(bytes, at + 32 + link * 2, 0xffff, 2);
			++link;
		}
	};
	action(2, 32, 0, { 32767, -1, 50 });
	action(7, 33, 0, { 49, 50, 0 });
	xl_test::layout_patch(bytes, level.sections[3].offset + 7 * 52 + 34, 0xfffe, 2);
	xl_test::layout_patch(bytes, level.sections[3].offset + 7 * 52 + 16, level.walls[0].segment, 2);
	xl_test::layout_patch(bytes, level.sections[3].offset + 7 * 52 + 36, level.walls[0].side, 2);
	action(8, 32, 2 | 8 | 16, { 49, int(player - level.objects.begin()), 50 });
	action(9, 33, 0, { 50 });
	// Real powerup removals dispatch the two object-attached actions
	for (int i = 0; i < 2; ++i) {
		const auto at = level.sections[3].offset + 10 * 52 + 4 + i * 53;
		std::fill(bytes.begin() + at, bytes.begin() + at + 53, 0);
		xl_test::layout_patch(bytes, at, i ? 33 : 32, 1);
		xl_test::layout_patch(bytes, at + 1, 2 | 16 | (i ? 0 : 8), 2);
		xl_test::layout_patch(bytes, at + 3, i ? 3 : 4, 1);
		xl_test::layout_patch(bytes, at + 9, 0xffffffff);
		if (!i) {
			// Invalid indices compact; attached negative sides other than -1 survive
			for (int link = 0; link < 4; ++link) {
				const int target = link == 0 ? 32767 : link == 1 ? -1
				                                   : link == 2   ? 50
				                                                 : level.walls[0].segment;
				xl_test::layout_patch(bytes, at + 13 + link * 2, unsigned(target), 2);
				xl_test::layout_patch(bytes, at + 33 + link * 2, link == 3 ? level.walls[0].side : link == 2 ? 0xfffe
				                                                                                             : 0xffff,
				                      2);
			}
		} else {
			const int targets[]{ 50, int(player - level.objects.begin()), 49 };
			for (int link = 0; link < 3; ++link) {
				xl_test::layout_patch(bytes, at + 13 + link * 2, targets[link], 2);
				xl_test::layout_patch(bytes, at + 33 + link * 2, 0xffff, 2);
			}
		}
	}
	// Retain all four authored WAVs in their existing independent PCM-oracle order
	xl_test::layout_patch(bytes, level.sections[3].offset + 3 * 52 + 4, 3 * 65536);
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

void classic_packages(const Bytes &level, const std::vector<xl_compat::Member> &members, const std::filesystem::path &directory)
{
	Bytes archive{ 'D', 'H', 'F' };
	xl_test::hog_member(archive, "bank23.rl2", level, false);
	for (const auto &member : members) {
		if (member.name.size() <= 8 || member.name.substr(0, 8) != "neptunxl") continue;
		const auto extension = member.name.substr(8);
		if (extension != ".rl2" && extension != ".mn2") xl_test::hog_member(archive, "bank23" + extension, member.bytes, false);
	}
	for (const std::string name : { "xwav", "xwbad", "xsbank" }) {
		const std::string title = name == "xwav" ? "Derived XL WAV" : name == "xwbad" ? "Derived XL bad WAV"
		                                                                              : "Derived XL bank";
		write(directory / (name + ".hog"), archive);
		write(directory / (name + ".mn2"), text("d2x-name = " + title + "\nnum_levels = 1\nbank23.rl2\n"));
		const auto root = directory.parent_path() / "mods" / name;
		std::filesystem::create_directories(root / "sounds");
		if (name == "xsbank") write(root / (name + ".s22"), text("Unsupported selected bank"));
		else write(root / "sounds" / "laser01.wav", name == "xwav" ? constant_wave(3500, -6000) : text("Malformed selected WAV"));
	}
	std::cout << "Derived classic audio: owned WAV, malformed preferred WAV and selected custom-bank controls\n";
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
	for (const std::string name : { "audio23", "audio27", "amb23", "amb27", "nosnd", "badsnd", "badid", "badwave" }) {
		auto data = name.substr(0, 3) == "amb" ? ambient_objects(level) : level;
		if (name == "audio27" || name == "amb27") data = xl_test::runtime_layout_fixture(data, 27).bytes;
		xl_test::hog_member(output, name + ".rl2", data, false);
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
	write(directory / "xsound.mn2", text("d2x-name = Derived XL sounds\nnum_levels = 4\naudio23.rl2\naudio27.rl2\namb23.rl2\namb27.rl2\n"));
	write(directory / "xschange.hog", changed);
	write(directory / "xschange.mn2", text("d2x-name = Changed XL sounds\nnum_levels = 1\naudio23.rl2\n"));
	Bytes legacy{ 'D', 'H', 'F' };
	const auto ambient = ambient_objects(level);
	for (int version : { 33, 34, 36 }) {
		const auto name = "amb" + std::to_string(version);
		xl_test::hog_member(legacy, name + ".rl2", xl_test::legacy_audio_fixture(ambient, version), false);
		xl_test::hog_member(legacy, name + ".snd", text(sounds), false);
		for (const auto &member : members) {
			if (member.name.size() <= 8 || member.name.substr(0, 8) != "neptunxl") continue;
			const auto extension = member.name.substr(8);
			// Keep the embedded colors; legacy CLR overrides are an independent gate
			if (extension != ".rl2" && extension != ".mn2" && extension != ".snd" && extension != ".clr")
				xl_test::hog_member(legacy, name + extension, member.bytes, false);
		}
	}
	for (const auto &member : xl_compat::read_hog(output))
		if (member.name.size() > 4 && member.name.substr(member.name.size() - 4) == ".wav")
			xl_test::hog_member(legacy, member.name, member.bytes, false);
	write(directory / "xambold.hog", legacy);
	write(directory / "xambold.mn2", text("d2x-name = Derived legacy ambience\nnum_levels = 3\namb33.rl2\namb34.rl2\namb36.rl2\n"));
	scopes(level, members, directory);
	classic_packages(found->bytes, members, directory);
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
