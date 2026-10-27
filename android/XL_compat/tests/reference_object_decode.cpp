// Storage adapters for the extracted XL reader, with Redux used only for comparison
#include "inventory.h"
#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iostream>
#include <iterator>
#include <stdexcept>
#include <vector>
#define TT_MASTER    31
#define TT_COUNTDOWN 25
#define TT_MESSAGE   29
#define TT_SOUND     30
#define TF_DISABLED  4
#define TF_PERMANENT 8
#define TF_AUTOPLAY  128
#define X2I(value)   ((value) >> 16)
using fix = std::int32_t;
#define X2F(value)        (float(value) / 65536.0f)
#define I2X(value)        ((value) * 65536)
#define TT_TELEPORT       14
#define TT_SPAWN_BOT      26
#define OBJ_PLAYER        4
#define OBJ_ROBOT         2
#define OBJ_REACTOR       9
#define OF_EXPLODING      2
#define OF_SHOULD_BE_DEAD 1
#define OF_DESTROYED      4
#define OF_ARMAGEDDON     8
struct CObject {
	struct {
		int nType = OBJ_ROBOT, nId = 0, nFlags = 0;
		fix xShield = 65536;
	} info;
	fix maximum = 65536;
	float Damage();
} sample_object;
fix RobotDefaultShield(CObject *object)
{
	return object->maximum;
}
fix ReactorStrength()
{
	throw std::runtime_error("Unqualified reactor damage oracle branch");
}
struct OraclePlayer {
	fix Shield()
	{
		throw std::runtime_error("Unqualified player damage oracle branch");
	}
} sample_player;
#define PLAYER(index) sample_player
#define OBJECT(index) (&sample_object)
void check(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
void Error(const char *message)
{
	throw std::runtime_error(message);
}
struct CFile {
	xl_compat::Bytes bytes;
	std::size_t position;
	int ReadByte()
	{
		return bytes.at(position++);
	}
	int ReadUShort()
	{
		const auto low = ReadByte();
		return low | (ReadByte() << 8);
	}
	int ReadShort()
	{
		const int value = ReadUShort();
		return value < 32768 ? value : value - 65536;
	}
	int ReadInt()
	{
		const auto low = std::uint32_t(ReadUShort());
		return static_cast<std::int32_t>(low | (std::uint32_t(ReadUShort()) << 16));
	}
	int ReadFix()
	{
		return ReadInt();
	}
	void Seek(std::size_t offset, int origin)
	{
		check(origin == SEEK_CUR && offset <= bytes.size() - position, "Reference seek out of bounds");
		position += offset;
	}
};
struct CTriggerTargets {
	int segments[10]{}, sides[10]{};
	void Read(CFile &file)
	{
		for (auto &value : segments) value = file.ReadShort();
		for (auto &value : sides) value = file.ReadShort();
	}
};
struct CTrigger : CTriggerTargets {
	int type = 0, flags = 0, value = 0, times[2]{}, m_nLinks = 0;
	struct {
		int nObject = -1, nPlayer = -1, nChannel = -1, tOperated = -1;
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
	void SetValue(int v)
	{
		value = v;
	}
	void SetTime(int index, int v)
	{
		times[index] = v;
	}
	void SetFlags(int bits)
	{
		flags |= bits;
	}
	void ClearFlags(int bits)
	{
		flags &= ~bits;
	}
	bool Flagged(int bits) const
	{
		return (flags & bits) != 0;
	}
	void Read(CFile &, int32_t);
	int32_t DoExecObjTrigger(int16_t, int32_t);
};
std::vector<CTrigger> object_triggers;
#define OBJTRIGGERS object_triggers
struct {
	int fileinfoVersion;
} gameTopFileInfo;
struct {
	struct {
		int m_nTriggers[2]{};
		bool Create(int count, bool attached)
		{
			check(attached && count > 0 && count < 10000, "Unexpected reference allocation");
			object_triggers.assign(count, CTrigger{});
			return true;
		}
	} trigData;
} gameData;
#include "xl_reference_object_read.h"

void compare_level(const xl_compat::Bytes &bytes)
{
	const auto inventory = xl_compat::inspect_level(bytes);
	const auto trigger = std::find_if(inventory.sections.begin(), inventory.sections.end(), [](const auto &s) { return s.name == "triggers"; });
	check(trigger != inventory.sections.end() && inventory.object_triggers > 0, "Missing positive object section");
	auto end = bytes.size();
	for (const auto &section : inventory.sections)
		if (section.offset > trigger->offset) end = std::min(end, static_cast<std::size_t>(section.offset));
	CFile file{ bytes, static_cast<std::size_t>(trigger->offset) + inventory.triggers.size() * 52 };
	gameTopFileInfo.fileinfoVersion = inventory.game_version;
	check(ReadObjectSection(file) == 0 && file.position == end, "Reference consumed a different object section extent");
	check(object_triggers.size() == inventory.object_trigger_records.size(), "Reference object count differs");
	for (std::size_t i = 0; i < object_triggers.size(); ++i) {
		const auto &reference = object_triggers[i];
		const auto &decoded = inventory.object_trigger_records[i];
		const auto &action = decoded.action;
		check(reference.m_info.nObject == decoded.object && reference.type == action.type && reference.flags == action.flags && reference.m_nLinks == static_cast<int>(action.targets.size()), "Reference object identity or action differs");
		const int time = action.type == TT_COUNTDOWN || action.type == TT_MESSAGE || action.type == TT_SOUND ? action.time : -1;
		check(reference.value == (action.type == TT_MASTER ? 0 : action.value) && reference.times[0] == time && reference.times[1] == -1, "Reference object normalization differs");
		for (std::size_t target = 0; target < action.targets.size(); ++target)
			check(reference.segments[target] == action.targets[target][0] && reference.sides[target] == action.targets[target][1], "Reference object targets differ");
	}
	std::cout << "Matched " << object_triggers.size() << " decoded object records and owner references for " << inventory.level_version << '/' << inventory.game_version << '\n';
}

int main(int argc, char **argv)
{
	try {
		if (argc == 3 && std::string(argv[1]) == "--events") {
			std::ifstream samples(argv[2]);
			int type, value, flags, damage, shields, maximum, fired, changed, count = 0;
			while (samples >> type >> value >> flags >> damage >> shields >> maximum >> fired >> changed) {
				CTrigger trigger;
				trigger.type = type;
				trigger.value = value;
				trigger.flags = flags;
				sample_object.info.xShield = shields;
				sample_object.maximum = maximum;
				const bool selected = trigger.DoExecObjTrigger(0, damage) != 0;
				check((selected && !(flags & TF_DISABLED)) == bool(fired) && trigger.value == changed, "Reference object event selection or consumed value differs");
				++count;
			}
			check(samples.eof() && count == 1456, "Missing or malformed object event samples");
			std::cout << "Matched " << count << " production object event samples against pinned selection and shield math\n";
			return 0;
		}
		check(argc == 2, "Requires source level or HOG");
		std::ifstream input(argv[1], std::ios::binary);
		check(bool(input), "Cannot open source level");
		xl_compat::Bytes bytes{ std::istreambuf_iterator<char>(input), std::istreambuf_iterator<char>() };
		if (bytes.size() >= 3 && bytes[0] == 'D' && bytes[1] == 'H' && bytes[2] == 'F') {
			int count = 0;
			for (const auto &member : xl_compat::read_hog(bytes)) {
				if (member.name.size() < 4 || member.name.substr(member.name.size() - 4) != ".rl2") continue;
				compare_level(member.bytes);
				++count;
			}
			check(count > 0, "No original levels compared");
		} else compare_level(bytes);
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
