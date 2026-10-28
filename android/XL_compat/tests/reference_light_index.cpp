#include "inventory.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
struct CFile {
	const xl_compat::Bytes &bytes;
	std::size_t position = 0;
	std::uint8_t ReadByte()
	{
		return bytes.at(position++);
	}
	std::int16_t ReadShort()
	{
		const auto low = ReadByte();
		return static_cast<std::int16_t>(low | (ReadByte() << 8));
	}
};
struct CLightDeltaIndex {
	std::int16_t nSegment;
	std::uint16_t nSide : 3;
	std::uint16_t count : 13;
	std::uint16_t index;
	void Read(CFile &);
};
struct {
	struct {
		bool bD2XLights = false;
	} render;
} gameStates;
#include "xl_reference_light_index.h"
static void require(bool ok, const char *why)
{
	if (!ok) throw std::runtime_error(why);
}
static unsigned word(const xl_compat::Bytes &b, std::size_t p)
{
	return unsigned(b.at(p)) | (unsigned(b.at(p + 1)) << 8);
}
static std::uint32_t dword(const xl_compat::Bytes &b, std::size_t p)
{
	return word(b, p) | (std::uint32_t(word(b, p + 2)) << 16);
}
int main(int argc, char **argv)
{
	try {
		unsigned samples = 0;
		for (bool packed : { false, true }) {
			gameStates.render.bD2XLights = packed;
			for (unsigned bits = 0; bits < 65536; ++bits) {
				xl_compat::Bytes raw{ 0, 0, static_cast<std::uint8_t>(bits), static_cast<std::uint8_t>(bits >> 8), 255, 255 };
				CFile file{ raw };
				CLightDeltaIndex index{};
				index.Read(file);
				require(index.nSegment == 0 && index.nSide == (bits & 7) && index.count == (packed ? bits >> 3 : bits >> 8) && index.index == 65535 && file.position == 6, "Source light-index bit/storage matrix differs");
				++samples;
			}
		}
		std::cout << samples << " extracted-source packed/legacy light-index bit patterns passed\n";
		if (argc == 1) return 0;
		require(argc == 2, "Expected original HOG");
		std::ifstream input(argv[1], std::ios::binary);
		xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		unsigned valid = 0, rejected = 0, indices = 0;
		for (const auto &member : xl_compat::read_hog(bytes)) {
			if (member.name.size() < 4 || member.name.substr(member.name.size() - 4) != ".rl2") continue;
			const auto &b = member.bytes;
			const auto game = dword(b, 12);
			require(dword(b, 4) == 15 && word(b, game + 2) == 34, "Unexpected original format");
			const auto offset = dword(b, game + 35 + 7 * 12), count = dword(b, game + 39 + 7 * 12), deltas = dword(b, game + 39 + 8 * 12);
			gameStates.render.bD2XLights = true;
			CFile file{ b, offset };
			std::vector<std::array<int, 4>> rows;
			bool ranges_valid = true;
			for (unsigned n = 0; n < count; ++n) {
				CLightDeltaIndex index{};
				index.Read(file);
				rows.push_back({ index.nSegment, index.nSide, index.count, index.index });
				ranges_valid &= index.nSide < 6 && index.index <= deltas && index.count <= deltas - index.index;
				++indices;
			}
			if (ranges_valid) {
				const auto decoded = xl_compat::inspect_level(b);
				require(decoded.light_indices == rows, "Production light indices differ from extracted source");
				require(member.name == "alarlof.rl2", "Unexpected valid original needs evidence update");
				++valid;
			} else {
				bool refused = false;
				try {
					xl_compat::inspect_level(b);
				} catch (const std::exception &e) {
					refused = std::string(e.what()).find("invalid packed light index") != std::string::npos;
				}
				require(refused, "Invalid source light range was admitted or rejected for another cause");
				++rejected;
			}
		}
		require(valid == 1 && rejected == 14, "Incomplete original 15/34 audit");
		std::cout << indices << " original light indices checked with extracted source: one valid level, fourteen rejected; no runtime qualification\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
