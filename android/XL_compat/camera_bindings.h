#ifndef DXX_XL_CAMERA_BINDINGS_H
#define DXX_XL_CAMERA_BINDINGS_H
#include "inventory.h"
#include <numeric>
#include <stdexcept>
#include <utility>

namespace xl_compat
{
struct CameraBinding {
	int object = -1, source_segment = -1, source_side = -1;
	int target_segment = -1, target_side = -1;
	bool mirror = false;
};
struct CameraBindings {
	std::vector<CameraBinding> cameras;
	std::vector<int> faces, objects;
};
namespace camera_detail
{
// BuildObjTriggerRef uses an unstable owner/type quicksort, including tie swaps
inline void sort_records(std::vector<int> &order, const Inventory &level, int left, int right)
{
	int l = left, r = right;
	const auto key = [&](int i) {
		const auto &record = level.object_trigger_records[order[i]];
		return std::make_pair(record.object, record.action.type);
	};
	const auto median = key((l + r) / 2);
	do {
		while (key(l) < median) ++l;
		while (median < key(r)) --r;
		if (l <= r) {
			if (l < r) std::swap(order[l], order[r]);
			++l;
			--r;
		}
	} while (l <= r);
	if (l < right) sort_records(order, level, l, right);
	if (left < r) sort_records(order, level, left, r);
}
} // namespace camera_detail

// Ordinary cameras only; teleporter cameras require separate rendering qualification
inline CameraBindings camera_bindings(const Inventory &level)
{
	CameraBindings result;
	result.faces.assign(level.segments.size() * 6, -1);
	result.objects.assign(level.objects.size(), -1);
	const auto face = [&](int segment, int side) {
		if (segment < 0 || std::size_t(segment) >= level.segments.size() || side < 0 || side >= 6)
			throw std::runtime_error("Camera binding references an invalid monitor face");
		return segment * 6 + side;
	};
	const auto add = [&](int object, int segment, int side, const Trigger &trigger) {
		for (const auto &target : trigger.targets) {
			const int target_face = face(target[0], target[1]);
			// Fail closed rather than silently dropping authored monitors at source capacity
			if (result.cameras.size() == 100) throw std::runtime_error("Camera binding exceeds source capacity");
			const int id = int(result.cameras.size());
			result.cameras.push_back({ object, segment, side, target[0], target[1], object < 0 && segment == target[0] && side == target[1] });
			result.faces[target_face] = id;
			if (object >= 0) result.objects[object] = id;
		}
	};
	for (const auto &wall : level.walls) {
		if (wall.trigger < 0 || std::size_t(wall.trigger) >= level.triggers.size()) continue;
		const auto &trigger = level.triggers[wall.trigger];
		if (trigger.type != 16) continue;
		face(wall.segment, wall.side);
		add(-1, wall.segment, wall.side, trigger);
	}
	std::vector<int> order(level.object_trigger_records.size());
	std::iota(order.begin(), order.end(), 0);
	if (!order.empty()) camera_detail::sort_records(order, level, 0, int(order.size()) - 1);
	for (const int index : order) {
		const auto &record = level.object_trigger_records[index];
		if (record.object < 0 || record.action.type != 16) continue;
		if (std::size_t(record.object) >= level.objects.size()) throw std::runtime_error("Camera binding owner is outside the level");
		if (level.objects[record.object].type == 255) continue;
		add(record.object, -1, -1, record.action);
	}
	return result;
}
} // namespace xl_compat
#endif
