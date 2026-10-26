// Reference functions are extracted unchanged; this adapter supplies storage and I/O
#include "inventory.h"
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
	int index = 0;
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
} gameData;
struct {
	struct {
		const char *szData[1]{ "" };
	} game;
} gameFolders;
static xl_compat::Bytes companion_bytes;
struct CFile {
	xl_compat::Bytes bytes;
	std::size_t cursor = 0;
	static void ChangeFilenameExtension(char *out, const char *, const char *)
	{
		std::strcpy(out, "fixture.clr");
	}
	bool Open(const char *, const char *, const char *, int)
	{
		bytes = companion_bytes;
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
	void Read(void *out, std::size_t size, int count)
	{
		const auto length = size * count;
		if (cursor > bytes.size() || length > bytes.size() - cursor) throw std::runtime_error("reference read exceeds fixture");
		std::memcpy(out, bytes.data() + cursor, length);
		cursor += length;
	}
};
static void PrintLog(int, const char * = nullptr) {}
#define INIT_PROGRESS_LOOP(i, j, count) \
	do {                                \
		i = 0;                          \
		j = count;                      \
	} while (0)
#include "xl_reference_color_readers.h"
int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("requires original Neptune HOG");
		std::ifstream stream(argv[1], std::ios::binary);
		if (!stream) throw std::runtime_error("cannot open HOG");
		const xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>() };
		const auto members = xl_compat::read_hog(bytes);
		const auto member = [&](const char *name) -> const xl_compat::Bytes & {
			const auto found = std::find_if(members.begin(), members.end(), [&](const auto &m) { return m.name == name; });
			if (found == members.end()) throw std::runtime_error("missing original member");
			return found->bytes;
		};
		const auto &level = member("neptunxl.rl2");
		const auto inventory = xl_compat::inspect_level(level);
		const auto original_clr = member("neptunxl.clr");
		const auto embedded = inventory.game_offset - MAX_WALL_TEXTURES * 13;
		for (int control = 0; control < 3; ++control) {
			companion_bytes = original_clr;
			if (control == 1) std::fill(companion_bytes.begin(), companion_bytes.end(), 0x55);
			if (control == 2) companion_bytes.clear();
			gameData.renderData.color.textures = {};
			LoadTextureColors("neptunxl.rl2", nullptr);
			CFile mine{ xl_compat::Bytes(level.begin() + embedded, level.begin() + inventory.game_offset) };
			LoadTexColorsCompiled(-1, mine);
			if (mine.cursor != MAX_WALL_TEXTURES * 13) throw std::runtime_error("embedded reader did not consume full table");
			for (int texture = 0; texture < MAX_WALL_TEXTURES; ++texture)
				for (int channel = 0; channel < 3; ++channel) {
					const float expected = float(std::round(inventory.texture_colors[texture].rgb[channel] * 2147483647.0)) / float(0x7fffffff);
					if (gameData.renderData.color.textures.values[texture].channels[channel] != expected) throw std::runtime_error("embedded color differs after CLR load");
				}
		}
		std::cout << "Pinned XL color readers: all 910 embedded colors replace original, conflicting and absent CLR; load order source-pinned\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
