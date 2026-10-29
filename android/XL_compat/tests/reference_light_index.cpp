#include "inventory.h"
#include "hog.h"
#include <array>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
struct CFile {
	const xl_compat::Bytes &bytes;
	std::size_t position = 0;
	std::uint8_t ReadUByte()
	{
		return bytes.at(position++);
	}
	std::int8_t ReadByte()
	{
		const int value = ReadUByte();
		return static_cast<std::int8_t>(value < 128 ? value : value - 256);
	}
	std::int16_t ReadShort()
	{
		const auto low = ReadUByte();
		return static_cast<std::int16_t>(low | (ReadUByte() << 8));
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
		unsigned samples = 0, byte_differences = 0;
		for (bool packed : { false, true }) {
			gameStates.render.bD2XLights = packed;
			for (unsigned bits = 0; bits < 65536; ++bits) {
				xl_compat::Bytes raw{ 0, 0, static_cast<std::uint8_t>(bits), static_cast<std::uint8_t>(bits >> 8), 255, 255 };
				CFile file{ raw };
				CLightDeltaIndex index{};
				index.Read(file);
				const auto encoded = packed ? bits >> 3 : bits >> 8;
				const auto signed_result = packed ? ((bits >> 3) + ((bits & 128) ? 32 : 0)) & 8191 : ((bits >> 8) + ((bits & 32768) ? 7936 : 0));
				require(index.nSegment == 0 && index.nSide == (bits & 7) && index.count == signed_result && index.index == 65535 && file.position == 6, "Actual signed-byte source matrix differs");
				if (index.count != encoded) ++byte_differences;
				++samples;
			}
		}
		require(byte_differences == 65536, "Signed-byte divergence matrix changed");
		std::cout << samples << " source patterns checked; " << byte_differences << " differ from unsigned encoded counts because of the reference signed-byte reader\n";
		if (argc == 1) return 0;
		const bool modern = argc == 3 && std::string(argv[1]) == "--modern";
		require(argc == 2 || modern, "Expected original HOG or --modern HOG");
		std::ifstream input(argv[modern ? 2 : 1], std::ios::binary | std::ios::ate);
		require(bool(input), "Cannot read original HOG");
		const auto size = input.tellg();
		require(size >= 3, "Empty original HOG");
		const auto read = [&](std::uint64_t offset, unsigned char *out, std::size_t count) {
			require(offset <= std::uint64_t(size) && count <= std::uint64_t(size) - offset, "Original HOG read outside input");
			input.clear();
			input.seekg(static_cast<std::streamoff>(offset));
			input.read(reinterpret_cast<char *>(out), static_cast<std::streamsize>(count));
			require(bool(input), "Original HOG read failed");
		};
		unsigned valid = 0, rejected = 0, indices = 0, original_differences = 0, signed_overruns = 0;
		for (const auto &member : xl_compat::index_hog(std::uint64_t(size), read)) {
			const auto name = xl_compat::hog_key(member.name);
			if (name.size() < 4 || name.substr(name.size() - 4) != ".rl2") continue;
			require(member.size <= 64 * 1024 * 1024, "Original level exceeds reader bound");
			xl_compat::Bytes b(static_cast<std::size_t>(member.size));
			read(member.offset, b.data(), b.size());
			const auto game = dword(b, 12);
			require(modern ? (dword(b, 4) == 25 || dword(b, 4) == 27) && word(b, game + 2) == 40 : dword(b, 4) == 15 && word(b, game + 2) == 34, "Unexpected original format");
			const auto offset = dword(b, game + 35 + 7 * 12), count = dword(b, game + 39 + 7 * 12), deltas = dword(b, game + 39 + 8 * 12);
			gameStates.render.bD2XLights = true;
			CFile file{ b, offset };
			std::vector<std::array<int, 4>> rows;
			bool ranges_valid = true;
			for (unsigned n = 0; n < count; ++n) {
				CLightDeltaIndex index{};
				index.Read(file);
				const auto encoded_count = word(b, offset + n * 6 + 2) >> 3;
				rows.push_back({ index.nSegment, index.nSide, int(encoded_count), index.index });
				if (index.count != encoded_count) ++original_differences;
				if (index.index > deltas || index.count > deltas - index.index) ++signed_overruns;
				ranges_valid &= index.nSide < 6 && index.index <= deltas && encoded_count <= deltas - index.index;
				++indices;
			}
			if (ranges_valid) {
				const auto decoded = xl_compat::inspect_level(b);
				require(decoded.light_indices == rows, "Production indices differ from unsigned encoded records");
				if (!modern) require(member.name == "alarlof.rl2", "Unexpected valid original needs evidence update");
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
				std::cout << member.name << ": invalid authored light range remains rejected\n";
			}
		}
		if (!modern) require(valid == 1 && rejected == 14, "Incomplete original 15/34 audit");
		else require(valid + rejected > 0, "No modern original levels inspected");
		std::cout << indices << " original indices: " << valid << " valid levels, " << rejected << " rejected, " << original_differences << " source signed-count differences, " << signed_overruns << " source ranges overrun; no runtime qualification\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
