#include "camera_bindings.h"
#include <algorithm>
#include <iostream>

namespace camera_binding_reference
{
#define MAX_CAMERAS          100
#define MAX_SHADOWMAPS       0
#define TELEPORT_CAMERAS     1
#define DBG                  0
#define TT_CAMERA            16
#define TT_TELEPORT          14
#define EGI_FLAG(a, b, c, d) false
template <class T>
struct Array : std::vector<T> {
	bool Create(std::size_t count, const char *)
	{
		this->assign(count, T{});
		return true;
	}
	void Clear(int)
	{
		for (auto &value : *this) value = -1;
	}
	T *Buffer()
	{
		return this->data();
	}
	int Length() const
	{
		return int(this->size());
	}
};
struct CObject {
	int index = 0, type = 0;
	int Index() const
	{
		return index;
	}
};
struct CTrigger {
	struct {
		int nType = 0, nObject = -1;
	} m_info;
	int m_nLinks = 0;
	int m_segments[10]{}, m_sides[10]{};
};
struct CWall {
	int nTrigger = 255, nSegment = 0, nSide = 0;
};
Array<int> segments;
Array<CObject> objects;
Array<CWall> walls;
Array<CTrigger> wall_triggers, object_triggers;
#define SEGMENTS      segments
#define OBJECTS       objects
#define WALLS         walls
#define GEOTRIGGERS   wall_triggers
#define GEOTRIGGER(i) (&wall_triggers.at(i))
#define OBJTRIGGERS   object_triggers
#define OBJTRIGGER(i) ((i) >= 0 && (i) < object_triggers.Length() ? &object_triggers[i] : nullptr)
#define LEVEL_OBJECTS objects.size()
#define FORALL_OBJS(p)          \
	for (auto &entry : objects) \
		if (entry.type != 255 && ((p) = &entry))
struct {
	struct {
		int nSegments = 0;
	} segData;
	struct {
		int nWalls = 0;
	} wallData;
	struct {
		int m_nTriggers[2]{};
		struct Ref {
			int nFirst = 255, nCount = 0;
		};
		std::vector<Ref> objTriggerRefs;
	} trigData;
} gameData;
void PrintLog(int, const char * = nullptr) {}
#define _CQUICKSORT_H
template <class T>
struct CQuickSort {
	using comparator = int32_t (*)(const T *, const T *);
	void SortAscending(T *, int32_t, int32_t);
	void SortDescending(T *, int32_t, int32_t);
	void SortAscending(T *, int32_t, int32_t, comparator);
	void SortDescending(T *, int32_t, int32_t, comparator);
	void Swap(T *, T *);
	int32_t BinSearch(T *, int32_t, int32_t, T);
};
#include "xl_reference_quicksort.h"
std::vector<int> object_map;
struct CCamera {
	xl_compat::CameraBinding binding;
	int Create(int id, int segment, int side, int target_segment, int target_side, CObject *object, int shadow, int teleport)
	{
		if (shadow || teleport) throw std::runtime_error("Unexpected camera topology source branch");
		binding = { object ? object->Index() : -1, segment, side, target_segment, target_side, !object && segment == target_segment && side == target_side };
		if (object) object_map.at(object->Index()) = id;
		return 1;
	}
};
struct Stack : Array<CCamera> {
	int count = 0;
	bool Grow()
	{
		if (count == int(size())) return false;
		++count;
		return true;
	}
	int ToS() const
	{
		return count;
	}
	CCamera *Top()
	{
		return &at(count - 1);
	}
};
struct CCameraManager {
	Stack m_cameras;
	Array<int> m_faceCameras, m_objectCameras;
	void Destroy()
	{
		m_cameras.clear();
		m_cameras.count = 0;
	}
	void SetFaceCamera(int face, int id)
	{
		m_faceCameras.at(face) = id;
	}
	int Create();
};
#include "xl_reference_camera_bindings.h"

void compare(const xl_compat::Inventory &level)
{
	segments.assign(level.segments.size(), 0);
	objects.resize(level.objects.size());
	object_map.assign(objects.size(), -1);
	for (std::size_t i = 0; i < objects.size(); ++i) objects[i] = { int(i), level.objects[i].type };
	walls.clear();
	wall_triggers.clear();
	object_triggers.clear();
	const auto trigger = [](const xl_compat::Trigger &action, int owner) {
		CTrigger result;
		result.m_info.nType = action.type;
		result.m_info.nObject = owner;
		result.m_nLinks = int(action.targets.size());
		if (result.m_nLinks > 10) throw std::runtime_error("Camera fixture exceeds source links");
		for (int i = 0; i < result.m_nLinks; ++i) {
			result.m_segments[i] = action.targets[i][0];
			result.m_sides[i] = action.targets[i][1];
		}
		return result;
	};
	for (const auto &wall : level.walls) walls.push_back({ wall.trigger, wall.segment, wall.side });
	for (const auto &action : level.triggers) wall_triggers.push_back(trigger(action, -1));
	for (const auto &record : level.object_trigger_records) object_triggers.push_back(trigger(record.action, record.object));
	gameData.segData.nSegments = int(segments.size());
	gameData.wallData.nWalls = int(walls.size());
	gameData.trigData.m_nTriggers[0] = int(wall_triggers.size());
	gameData.trigData.m_nTriggers[1] = int(object_triggers.size());
	gameData.trigData.objTriggerRefs.assign(objects.size(), {});
	BuildObjTriggerRef();
	CCameraManager source;
	if (!source.Create()) throw std::runtime_error("Camera source topology creation failed");
	const auto portable = xl_compat::camera_bindings(level);
	if (portable.cameras.size() != std::size_t(source.m_cameras.count) || portable.faces != source.m_faceCameras || portable.objects != object_map)
		throw std::runtime_error("Camera binding count or face/object map differs from source");
	for (std::size_t i = 0; i < portable.cameras.size(); ++i) {
		const auto &a = portable.cameras[i], &b = source.m_cameras[i].binding;
		if (a.object != b.object || a.source_segment != b.source_segment || a.source_side != b.source_side || a.target_segment != b.target_segment || a.target_side != b.target_side || a.mirror != b.mirror)
			throw std::runtime_error("Camera binding order or view owner differs from source");
	}
}
} // namespace camera_binding_reference

void camera_binding_original_controls(const xl_compat::Inventory &level)
{
	camera_binding_reference::compare(level);
	const auto bindings = xl_compat::camera_bindings(level);
	if (!bindings.cameras.empty()) std::cout << "Pinned original camera bindings: " << level.level_name << ": " << bindings.cameras.size() << " monitor entries matched\n";
}
void camera_binding_source_controls()
{
	for (int sample = 0; sample < 256; ++sample) {
		xl_compat::Inventory level;
		level.segments.resize(12);
		level.objects.resize(8);
		for (auto &object : level.objects) object.type = 16;
		level.objects[7].type = 255;
		for (int i = 0; i < sample % 17; ++i) {
			xl_compat::Trigger action{ i % 4 == 0 ? 2 : 16, sample & 255, 327680, -1, {} };
			for (int j = 0; j < i % 4; ++j) action.targets.push_back({ (i + j) % 12, (i + j) % 6 });
			level.object_trigger_records.push_back({ action, (i * 3 + sample) % 9 - 1 });
		}
		level.triggers.push_back({ 16, sample, 0, -1, { { 0, 0 }, { 1, 1 }, { 2, 2 } } });
		xl_compat::Wall wall{};
		wall.trigger = 0;
		level.walls.push_back(wall);
		if (sample & 1) {
			wall.side = 2;
			level.walls.push_back(wall);
		}
		camera_binding_reference::compare(level);
	}
	xl_compat::Inventory boundary;
	boundary.segments.resize(20);
	boundary.objects.resize(1);
	boundary.objects[0].type = 16;
	for (int i = 0; i < 10; ++i) {
		xl_compat::Trigger action{ 16, 0, 0, -1, {} };
		for (int j = 0; j < 10; ++j) action.targets.push_back({ i, j % 6 });
		boundary.object_trigger_records.push_back({ action, 0 });
	}
	camera_binding_reference::compare(boundary);
	boundary.object_trigger_records.back().action.targets.pop_back();
	camera_binding_reference::compare(boundary);
	for (int variant = 0; variant < 5; ++variant) {
		auto bad = boundary;
		if (variant == 0) {
			bad.object_trigger_records.back().action.targets.push_back({ 0, 0 });
			bad.object_trigger_records.push_back({ { 16, 0, 0, -1, { { 0, 0 } } }, 0 });
		}
		if (variant == 1) bad.object_trigger_records[0].object = 1;
		if (variant == 2) bad.object_trigger_records[0].action.targets[0][0] = 20;
		if (variant == 3) bad.object_trigger_records[0].action.targets[0][1] = -1;
		if (variant == 4) bad.object_trigger_records[0].action.targets[0][1] = 6;
		bool rejected = false;
		try {
			xl_compat::camera_bindings(bad);
		} catch (const std::runtime_error &) {
			rejected = true;
		}
		if (!rejected) throw std::runtime_error("Invalid camera topology accepted");
	}
	std::cout << "Pinned camera topology: 258 wall/object bindings, duplicate targets, owner/type ties, flags, missing owners, mirror and 99/100-capacity cases matched; five invalid/overflow cases rejected\n";
}
