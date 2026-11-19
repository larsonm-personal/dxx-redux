// Reference functions are extracted unchanged; this adapter supplies storage and I/O
#include "inventory.h"
#include "companion_lighting.h"
#include "hog.h"
#include <filesystem>
#include <map>
#include <cmath>
#include <cstring>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <algorithm>
constexpr int MAX_WALL_TEXTURES = 910, FILENAME_LEN = 256;
struct tRgbColord {
	double red, green, blue;
};
struct CFaceColor {
	char index = 0;
	float channels[4]{};
	float &Red()
	{
		return channels[0];
	}
	float &Green()
	{
		return channels[1];
	}
	float &Blue()
	{
		return channels[2];
	}
	float &Alpha()
	{
		return channels[3];
	}
};
struct BrightnessTable {
	xl_compat::TextureBrightnessTable values{};
	int32_t *Buffer()
	{
		return values.data();
	}
	int32_t operator[](int index) const
	{
		return values.at(index);
	}
};
struct ColorTable {
	CFaceColor values[MAX_WALL_TEXTURES];
	CFaceColor *Buffer()
	{
		return values;
	}
	CFaceColor *operator+(int i)
	{
		return values + i;
	}
};
struct {
	struct {
		bool bD2XLevel = true;
		bool bNostalgia = false;
	} app;
	struct {
		int nLightingMethod = 0;
		bool bColored = false;
	} render;
} gameStates;
struct {
	struct {
		struct {
			ColorTable textures;
		} color;
	} renderData;
	struct {
		int nLevelVersion = 23;
	} segData;
	struct {
		struct {
			BrightnessTable brightness;
		} tex;
	} pigData;
} gameData;
struct {
	struct {
		const char *szData[1]{ "" };
	} game;
} gameFolders;
static std::map<std::string, xl_compat::Bytes> companion_files;
struct CFile {
	xl_compat::Bytes bytes;
	std::size_t cursor = 0;
	static void ChangeFilenameExtension(char *out, const char *name, const char *extension)
	{
		const auto filename = std::filesystem::path(name).replace_extension(extension).string();
		if (filename.size() >= FILENAME_LEN) throw std::runtime_error("Reference filename too long");
		std::strcpy(out, filename.c_str());
	}
	bool Open(const char *name, const char *, const char *, int)
	{
		const auto entry = companion_files.find(name);
		if (entry == companion_files.end()) return false;
		bytes = entry->second;
		cursor = 0;
		return !bytes.empty();
	}
	void Close() {}
	int ReadByte()
	{
		return bytes.at(cursor++);
	}
	int32_t ReadInt()
	{
		uint32_t n = 0;
		for (int i = 0; i < 4; ++i) n |= uint32_t(ReadByte()) << (8 * i);
		int32_t signed_value;
		std::memcpy(&signed_value, &n, sizeof(n));
		return signed_value;
	}
	int Read(void *out, std::size_t size, int count)
	{
		const auto length = size * count;
		if (cursor > bytes.size() || length > bytes.size() - cursor) throw std::runtime_error("reference read exceeds fixture");
		std::memcpy(out, bytes.data() + cursor, length);
		cursor += length;
		return count;
	}
};
static void PrintLog(int, const char * = nullptr) {}
#define INIT_PROGRESS_LOOP(i, j, count) \
	do {                                \
		i = 0;                          \
		j = count;                      \
	} while (0)
int32_t IsLight(int32_t);
#ifdef __BYTE_ORDER__
#if __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define INTEL_INT(value) static_cast<int32_t>(__builtin_bswap32(value))
#endif
#endif
#ifndef INTEL_INT
#define INTEL_INT(value) (value)
#endif
#include "xl_reference_color_readers.h"
using xl_compat::Bytes;
using xl_compat::Member;

static std::vector<Member> read_archive(const char *path)
{
	std::ifstream stream(path, std::ios::binary);
	if (!stream) throw std::runtime_error(std::string("Cannot open HOG: ") + path);
	return xl_compat::read_hog(Bytes{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() });
}

static Bytes member(const std::vector<Member> &members, const std::string &name, bool required)
{
	const auto key = xl_compat::hog_key(name);
	const auto found = std::find_if(members.begin(), members.end(), [&](const auto &entry) { return xl_compat::hog_key(entry.name) == key; });
	if (found != members.end()) return found->bytes;
	if (required) throw std::runtime_error("Missing original member: " + name);
	return {};
}

int main(int argc, char **argv)
{
	try {
		if (argc != 2 && argc != 4) throw std::runtime_error("requires Neptune HOG, or mission HOG, level member and XL addon HOG");
		const std::filesystem::path filename(argc == 2 ? "neptunxl.rl2" : argv[2]);
		const auto members = read_archive(argv[1]);
		const auto level = member(members, filename.string(), true);
		const auto source = xl_compat::inspect_mission_level(level);
		const bool classic = source.level_version == 7 && source.game_version == 31;
		if (!classic && !(source.level_version == 17 && source.game_version == 36) && !xl_compat::modern_runtime_layout(source))
			throw std::runtime_error("Unsupported texture color encoding");
		const auto stem = filename.stem().string();
		const auto original_clr = member(members, stem + ".clr", argc == 2);
		const auto original_lgt = member(members, stem + ".lgt", argc == 4);
		Bytes default_clr, default_lgt;
		if (argc == 4) {
			const auto addon = read_archive(argv[3]);
			default_clr = member(addon, "descent2.clr", true);
			default_lgt = member(addon, "descent2.lgt", true);
		}
		xl_compat::TextureBrightnessTable expected_brightness{};
		const auto &lgt = original_lgt.empty() ? default_lgt : original_lgt;
		if (!lgt.empty()) expected_brightness = xl_compat::decode_texture_brightness(lgt.data(), lgt.size());
		const std::size_t embedded_size = classic ? 0 : MAX_WALL_TEXTURES * 13;
		if (source.game_offset < embedded_size || source.game_offset > level.size()) throw std::runtime_error("Invalid geometry boundary");
		gameStates.app.bD2XLevel = !classic;
		gameData.segData.nLevelVersion = source.level_version;
		for (int control = 0; control < 3; ++control) {
			gameData.renderData.color.textures = {};
			gameData.pigData.tex.brightness = {};
			companion_files = { { "descent2.lgt", default_lgt }, { "descent2.clr", default_clr }, { stem + ".lgt", original_lgt }, { stem + ".clr", original_clr } };
			LoadTextureBrightness("descent2", nullptr);
			LoadTextureColors("descent2", nullptr);
			LoadTextureBrightness(stem.c_str(), nullptr);
			auto &companion_bytes = companion_files.at(stem + ".clr");
			if (control == 1) companion_bytes.assign(MAX_WALL_TEXTURES * 13, 0x55);
			if (control == 2) companion_bytes.clear();
			xl_compat::TextureColorTable effective_clr{};
			if (classic) {
				const auto &effective = companion_bytes.empty() ? default_clr : companion_bytes;
				effective_clr = xl_compat::decode_texture_colors(effective.data(), effective.size());
			}
			LoadTextureColors(stem.c_str(), nullptr);
			InitTexColors();
			CFile mine{ Bytes(level.begin() + source.game_offset - embedded_size, level.begin() + source.game_offset) };
			LoadTexColorsCompiled(-1, mine);
			if (mine.cursor != embedded_size) throw std::runtime_error("Reference consumed wrong embedded table length");
			for (int i = 0; i < MAX_WALL_TEXTURES; ++i) {
				const auto &actual = gameData.renderData.color.textures.values[i];
				const auto expected_index = classic ? expected_brightness[i] : source.texture_colors.at(i).index;
				if (gameData.pigData.tex.brightness[i] != expected_brightness[i] || static_cast<uint8_t>(actual.index) != static_cast<uint8_t>(expected_index))
					throw std::runtime_error("Reference brightness or color index differs");
				for (unsigned channel = 0; channel < 3; ++channel) {
					const auto raw = classic ? effective_clr[i].channels[channel] : int32_t(std::round(source.texture_colors.at(i).rgb[channel] * 2147483647.0));
					if (actual.channels[channel] != float(raw) / float(0x7fffffff)) throw std::runtime_error("Reference RGB differs");
				}
			}
		}
		std::cout << filename.string() << " " << source.level_version << "/" << source.game_version
		          << ": 910 effective brightness, index and RGB entries match for original, conflicting and absent CLR\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
