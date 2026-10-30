#include "ambient_activation_receipt.h"
#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <stdexcept>
#include <string>
namespace ambient_activation_reference
{
#define OBJ_EFFECT   20
#define PARTICLE_ID  0
#define LIGHTNING_ID 1
#define SOUND_ID     2
struct CTrigger {
	int type = 0, flags = 0, value = 0, m_nLinks = 0;
	int16_t m_segments[10]{}, m_sides[10]{};
	struct {
		int tOperated = 1000;
	} m_info;
	int &Type()
	{
		return type;
	}
	int &Value()
	{
		return value;
	}
	int GetValue() const
	{
		return value;
	}
	bool Flagged(int mask) const
	{
		return (flags & mask) != 0;
	}
	bool Flagged(int mask, int expected) const
	{
		return (flags & mask) == expected;
	}
	void SetFlags(int mask)
	{
		flags |= mask;
	}
	void ClearFlags(int mask)
	{
		flags &= ~mask;
	}
	int32_t DoEnableTrigger();
	int32_t DoDisableTrigger();
	void PostFlags();
};
std::vector<CTrigger> triggers;
std::map<std::pair<int, int>, int> wall_indices;
struct Segment {
	int index = 0;
	CTrigger *Trigger(int side)
	{
		const auto found = wall_indices.find({ index, side });
		return found == wall_indices.end() || found->second < 0 || unsigned(found->second) >= triggers.size() ? nullptr : &triggers[unsigned(found->second)];
	}
};
std::map<int, Segment> segments;
Segment *segment(int index)
{
	return &segments.at(index);
}
#define SEGMENT(index) segment(index)
struct CObject {
	struct {
		int nType = 0, nId = 0;
	} info;
	struct {
		struct {
			int bEnabled = 0;
		} particleInfo, lightningInfo, soundInfo;
	} rType;
};
std::map<int, CObject> objects;
CObject *object(int index)
{
	const auto found = objects.find(index);
	return found == objects.end() ? nullptr : &found->second;
}
#define OBJECT(index) object(index)
struct {
	void Enable(CObject *)
	{
		throw std::runtime_error("Ambient activation entered unqualified lightning dispatch");
	}
} lightningManager;
#include "xl_reference_ambient_activation.h"

void compare(const xl_ambient_activation_receipt::Case &receipt)
{
	triggers.clear();
	objects.clear();
	segments.clear();
	wall_indices.clear();
	for (const auto &row : receipt.before_triggers) {
		CTrigger trigger;
		trigger.type = row[0];
		trigger.flags = row[1];
		trigger.value = row[2];
		triggers.push_back(trigger);
	}
	for (const auto &row : receipt.before_objects) {
		CObject object;
		object.info = { row[1], row[2] };
		object.rType.soundInfo.bEnabled = row[3];
		if (!objects.emplace(row[0], object).second) throw std::runtime_error("Duplicate ambient activation object");
	}
	for (const auto &row : receipt.walls) {
		segments[row[0]].index = row[0];
		wall_indices[{ row[0], row[1] }] = row[2];
	}
	CTrigger attached;
	if (receipt.wall < -1 || (receipt.wall >= 0 && unsigned(receipt.wall) >= triggers.size())) throw std::runtime_error("Invalid ambient action index");
	auto &action = receipt.wall >= 0 ? triggers[unsigned(receipt.wall)] : attached;
	action.type = receipt.action[0];
	action.flags = receipt.action[1];
	action.value = receipt.action[2];
	action.m_nLinks = int(receipt.links.size());
	for (unsigned i = 0; i < receipt.links.size(); ++i) {
		action.m_segments[i] = int16_t(receipt.links[i][0]);
		action.m_sides[i] = int16_t(receipt.links[i][1]);
	}
	if (action.type == 32) action.DoEnableTrigger();
	else if (action.type == 33) action.DoDisableTrigger();
	else throw std::runtime_error("Unexpected ambient activation type");
	// Operate ignores the target handler's return and still applies these flags
	action.PostFlags();
	const std::array<int, 4> result{ action.type, action.flags, action.value, action.m_info.tOperated >= 0 };
	if (result != receipt.result) throw std::runtime_error("Ambient activation post-flags or permanent reset differ from source");
	std::vector<std::array<int, 3>> after_triggers;
	for (const auto &trigger : triggers) after_triggers.push_back({ trigger.type, trigger.flags, trigger.value });
	std::vector<std::array<int, 4>> after_objects;
	for (const auto &row : receipt.before_objects) {
		const auto &object = objects.at(row[0]);
		after_objects.push_back({ row[0], object.info.nType, object.info.nId, object.rType.soundInfo.bEnabled });
	}
	if (after_triggers != receipt.after_triggers || after_objects != receipt.after_objects)
		throw std::runtime_error("Ambient activation changed different objects or wall triggers than source");
}
} // namespace ambient_activation_reference
void ambient_activation_runtime_controls(const char *path)
{
	std::ifstream input(path);
	std::string tag;
	unsigned version = 0, ordinal = 0, cases = 0, attached = 0;
	if (!(input >> tag >> version) || tag != "XL_AMBIENT_ACTIVATION" || version != 1) throw std::runtime_error("Invalid ambient activation header");
	while (input >> tag >> ordinal) {
		if (tag == "end") break;
		if (tag != "case" || ordinal != cases) throw std::runtime_error("Ambient activation cases missing or reordered");
		const auto receipt = xl_ambient_activation_receipt::read(input);
		ambient_activation_reference::compare(receipt);
		attached += receipt.wall == -1;
		++cases;
	}
	if (tag != "end" || ordinal != cases || cases != 6 || attached != 2 || input >> tag) throw std::runtime_error("Ambient activation coverage incomplete");
	std::cout << "PASS: 6 loaded wall/attached ambient actions match actual source activation, early stop, post-flags and permanent reset\n";
}
