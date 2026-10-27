// Storage adapters only; counter operations come from the pinned XL source
#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <vector>
#define TF_DISABLED  4
#define TF_PERMANENT 8
#define TF_AUTOPLAY  128
#define TT_MASTER    31
#define TT_COUNTDOWN 25
#define TT_MESSAGE   29
#define TT_SOUND     30
#define X2I(value)   ((value) >> 16)
#define OBJ_EFFECT   20
#define PARTICLE_ID  0
#define LIGHTNING_ID 1
#define SOUND_ID     2
std::vector<int> calls;
bool expected_attached = false;
int expected_player = 2;
struct CFile {
	std::vector<std::uint8_t> bytes;
	std::size_t position = 0;
	int ReadByte()
	{
		return bytes.at(position++);
	}
	int ReadUShort()
	{
		const int low = ReadByte();
		return low | (ReadByte() << 8);
	}
	int ReadFix()
	{
		const auto low = std::uint32_t(ReadUShort());
		return static_cast<std::int32_t>(low | (std::uint32_t(ReadUShort()) << 16));
	}
};
struct CTriggerTargets {
	void Read(CFile &file)
	{
		file.position += 40;
	}
};
struct {
	int fileinfoVersion = 40;
} gameTopFileInfo;
struct CTrigger : CTriggerTargets {
	int m_nLinks = 1, type = TT_MASTER, flags = 0, value = 0, result = 0;
	int16_t m_segments[10]{}, m_sides[10]{};
	struct {
		int tOperated = -1, nObject = -1, nPlayer = -1, nChannel = -1;
		void Clear() {}
	} m_info;
	int &Type()
	{
		return type;
	}
	int GetValue() const
	{
		return value;
	}
	int &Value()
	{
		return value;
	}
	void SetValue(int v)
	{
		value = v;
	}
	void SetTime(int, int) {}
	bool Flagged(int mask) const
	{
		return (flags & mask) != 0;
	}
	void Read(CFile &, int32_t);
	void SetFlags(int f)
	{
		flags |= f;
	}
	void ClearFlags(int f)
	{
		flags &= ~f;
	}
	bool Flagged(int mask, int value) const
	{
		return (flags & mask) == value;
	}
	int Operate(int16_t object, int player, int shot, bool attached)
	{
		if (object != 7 || player != expected_player || shot != 0 || attached != expected_attached) throw std::runtime_error("master child arguments differ");
		calls.push_back(value);
		return result;
	}
	int32_t DoMasterTrigger(int16_t, int32_t, bool);
	int32_t DoEnableTrigger();
	int32_t DoDisableTrigger();
} targets[3];
struct Segment {
	CTrigger *Trigger(int side)
	{
		return side < 3 ? targets + side : nullptr;
	}
} segment;
#define SEGMENT(x) (&segment)
struct CObject {
	struct {
		int nType = -1, nId = -1;
	} info;
	struct {
		struct {
			int bEnabled = 0;
		} particleInfo, lightningInfo, soundInfo;
	} rType;
};
CObject *effect_object(int)
{
	throw std::runtime_error("unqualified effect-object branch reached");
}
#define OBJECT(x) effect_object(x)
struct Lightning {
	void Enable(CObject *)
	{
		throw std::runtime_error("effect branch reached");
	}
} lightningManager;
#include "xl_reference_trigger.h"

int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Requires runtime counter samples");
		std::ifstream samples(argv[1]);
		int value, disabled, operation, expected_value, expected_flags, count = 0;
		while (samples >> value >> disabled >> operation >> expected_value >> expected_flags) {
			targets[0].type = TT_MASTER;
			targets[0].value = value;
			targets[0].flags = disabled ? TF_DISABLED : 0;
			CTrigger action;
			if (operation == 32) action.DoEnableTrigger();
			else if (operation == 33) action.DoDisableTrigger();
			else throw std::runtime_error("Invalid sample action");
			if (targets[0].value != expected_value || targets[0].flags != expected_flags) throw std::runtime_error("Reference master counter mismatch");
			++count;
		}
		if (!samples.eof() || count != 40) throw std::runtime_error("Missing or malformed runtime samples");
		for (int version : { 33, 36, 40 })
			for (int value : { -65536, 0, 1, 65535, 65536, 131072 })
				for (int flags : { 0, 8, 128, 136 }) {
					CFile file;
					file.bytes.resize(52);
					file.bytes[0] = TT_MASTER;
					file.bytes[1] = static_cast<std::uint8_t>(flags);
					for (unsigned i = 0; i < 4; ++i) file.bytes[4 + i] = std::uint32_t(value) >> (i * 8);
					CTrigger read;
					gameTopFileInfo.fileinfoVersion = version;
					read.Read(file, 0);
					const int expected = version < 39 ? 0 : value > 0 ? value / 65536
					                                                  : value;
					const int expected_flags = version >= 39 && value > 0 ? (flags | TF_DISABLED) & (flags & TF_AUTOPLAY ? ~TF_PERMANENT : ~0) : flags;
					if (read.value != expected || read.flags != expected_flags || file.position != 52) throw std::runtime_error("Reference master source normalization changed");
				}
		CTrigger master;
		master.m_nLinks = 3;
		for (int i = 0; i < 3; ++i) {
			master.m_sides[i] = i;
			targets[i].value = i;
			targets[i].result = i == 1 ? -1 : 0;
		}
		if (master.DoMasterTrigger(7, 2, false) != -1 || calls != std::vector<int>{ 0, 1 }) throw std::runtime_error("Reference chain order or exit propagation changed");
		calls.clear();
		expected_attached = true;
		expected_player = -1;
		if (master.DoMasterTrigger(7, -1, true) != -1 || calls != std::vector<int>{ 0, 1 }) throw std::runtime_error("Reference object master context or link order changed");
		std::cout << "Matched 40 runtime master counter samples; reference source normalization, link order and exit propagation checked\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
