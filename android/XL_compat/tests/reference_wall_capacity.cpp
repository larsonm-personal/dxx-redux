#include "../capacity.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

struct CFile {
	std::vector<uint8_t> bytes;
	unsigned at = 0;
	uint8_t ReadUByte()
	{
		return bytes.at(at++);
	}
	uint16_t ReadUShort()
	{
		const auto low = ReadUByte();
		return uint16_t(low | unsigned(ReadUByte()) << 8);
	}
};
struct {
	struct {
		int nLevelVersion;
	} segData;
} gameData;
class CSide
{
  public:
	uint16_t m_nWall = 0;
	void ReadWallNum(CFile &, bool);
};
#include "xl_reference_wall_capacity.h"
static_assert(MAX_WALLS == DXX_XL_MAX_WALLS, "Native XL wall capacity differs from pinned source");

int main()
{
	try {
		unsigned tested = 0;
		for (int version : { 12, 13, 15, 17, 23 })
			for (unsigned id : { 0u, 1u, 254u, 255u, 256u, 2046u, 2047u, 65535u })
				for (bool present : { false, true }) {
					gameData.segData.nLevelVersion = version;
					CFile file{ { uint8_t(id), uint8_t(id >> 8) } };
					CSide side;
					side.ReadWallNum(file, present);
					const unsigned width = present ? (version >= 13 ? 2 : 1) : 0;
					const unsigned expected = present ? (version >= 13 ? id : id & 255) : 65535;
					if (side.m_nWall != expected || file.at != width)
						throw std::runtime_error("Pinned wall identity/width/sentinel mismatch");
					++tested;
				}
		std::cout << tested << " extracted source wall width/identity/sentinel cases passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
