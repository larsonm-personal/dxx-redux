#include "../capacity.h"
#include "../../app/src/main/cpp/shared/wall_trigger_index.h"
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <vector>

struct CFile {
	std::vector<uint8_t> bytes;
	std::size_t at = 0;
	int8_t ReadByte()
	{
		return int8_t(bytes.at(at++));
	}
	uint16_t ReadUShort()
	{
		const auto low = uint8_t(ReadByte());
		return uint16_t(low | unsigned(uint8_t(ReadByte())) << 8);
	}
	int32_t ReadInt()
	{
		const auto low = ReadUShort();
		return int32_t(low | uint32_t(ReadUShort()) << 16);
	}
	int32_t ReadFix()
	{
		return ReadInt();
	}
};
struct {
	int fileinfoVersion;
} gameTopFileInfo;
struct CWall {
	int32_t nSegment = 0, nSide = 0, hps = 0, nLinkedWall = 0;
	uint8_t nType = 0, state = 0, nTrigger = 0, keys = 0;
	uint16_t flags = 0;
	int8_t nClip = 0, controllingTrigger = 0, cloakValue = 0, bVolatile = 0;
	void Read(CFile &);
};
static bool warned;
#define TXT_SAVE_CORRUPT "corrupt"
static void Warning(const char *)
{
	warned = true;
}
struct CSaveGameManager {
	CFile m_cf;
	int32_t ReadBoundedInt(int32_t, int32_t *);
};
#include "xl_reference_trigger_capacity.h"
static_assert(MAX_TRIGGERS == DXX_XL_MAX_TRIGGERS && MAX_OBJ_TRIGGERS == DXX_XL_MAX_TRIGGERS, "XL trigger capacities differ from pinned source");

int main()
{
	try {
		unsigned cases = 0;
		for (int version : { 33, 34, 36, 40 })
			for (unsigned id = 0; id <= 255; ++id) {
				gameTopFileInfo.fileinfoVersion = version;
				const bool modern = version >= 37;
				CFile file{ std::vector<uint8_t>(modern ? 25 : 24) };
				file.bytes[modern ? 20 : 19] = uint8_t(id);
				file.bytes[modern ? 23 : 22] = uint8_t(id);
				CWall wall;
				wall.Read(file);
				const int expected = wall.nTrigger == 255 ? -1 : wall.nTrigger;
				if (wall.nTrigger != id || wall_trigger_index(int8_t(id)) != expected ||
				    wall_trigger_index(wall.controllingTrigger) != expected || file.at != file.bytes.size())
					throw std::runtime_error("Signed native trigger identity differs from source wall reader");
				++cases;
			}
		for (int32_t count : { INT32_MIN, -1, 0, 1, 99, 100, 101, 127, 128, 253, 254, 255, INT32_MAX }) {
			CSaveGameManager manager;
			for (unsigned byte = 0; byte < 4; ++byte) manager.m_cf.bytes.push_back(uint8_t(uint32_t(count) >> (8 * byte)));
			int32_t installed = 73;
			warned = false;
			const bool invalid = count < 0 || count > DXX_XL_MAX_TRIGGERS;
			if (bool(manager.ReadBoundedInt(MAX_TRIGGERS, &installed)) != invalid || warned != invalid || installed != (invalid ? 73 : count))
				throw std::runtime_error("Saved trigger bound differs from pinned source");
			++cases;
		}
		std::cout << cases << " extracted source wall-ID and saved trigger-bound controls passed\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
