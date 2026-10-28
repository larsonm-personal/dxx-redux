#ifndef DXX_XL_MODEL_GEOMETRY_H
#define DXX_XL_MODEL_GEOMETRY_H

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <set>
#include <stdexcept>
#include <vector>

namespace xl_compat
{
// Bounded equivalent of the POF path in GetPolyModelMinMax / CPolyModel::Size
// Read the original little-endian payload before native alignment/endian fixups
class ModelGeometry
{
	struct Box {
		std::array<int64_t, 3> low{ INT32_MAX, INT32_MAX, INT32_MAX };
		std::array<int64_t, 3> high{ INT32_MIN, INT32_MIN, INT32_MIN };
		std::array<int64_t, 3> offset{};
		bool points = false;
	};
	const uint8_t *data;
	std::size_t length, budget;
	unsigned submodel_count;
	std::vector<Box> boxes{ 1 };
	std::set<std::size_t> active;

	void need(std::size_t at, std::size_t count) const
	{
		if (at > length || count > length - at) throw std::runtime_error("HXM model geometry is truncated");
	}
	unsigned word(std::size_t at) const
	{
		need(at, 2);
		return unsigned(data[at]) | unsigned(data[at + 1]) << 8;
	}
	int32_t integer(std::size_t at) const
	{
		need(at, 4);
		const uint32_t value = uint32_t(data[at]) | uint32_t(data[at + 1]) << 8 |
		                       uint32_t(data[at + 2]) << 16 | uint32_t(data[at + 3]) << 24;
		return value <= INT32_MAX ? int32_t(value) : -1 - int32_t(~value);
	}
	void walk(std::size_t at, std::size_t box, unsigned depth)
	{
		if (depth > 64 || !active.insert(at).second) throw std::runtime_error("HXM model geometry contains recursive calls");
		const auto start = at;
		for (;;) {
			if (!budget--) throw std::runtime_error("HXM model geometry exceeds traversal budget");
			const auto op = word(at);
			if (!op) break;
			switch (op) {
				case 1:
				case 7: {
					const auto count = word(at + 2);
					const auto first = at + (op == 1 ? 4 : 8);
					need(first, std::size_t(count) * 12);
					auto &bounds = boxes[box];
					for (unsigned point = 0; point < count; ++point)
						for (unsigned axis = 0; axis < 3; ++axis) {
							const int64_t value = integer(first + point * 12 + axis * 4);
							// Independent updates also define a one-point or monotone cloud
							bounds.low[axis] = std::min(bounds.low[axis], value);
							bounds.high[axis] = std::max(bounds.high[axis], value);
						}
					bounds.points |= count != 0;
					at = first + std::size_t(count) * 12;
					break;
				}
				case 2:
				case 3: {
					const auto count = word(at + 2);
					const auto size = 30 + (count | 1) * 2 + (op == 3 ? count * 12 : 0);
					need(at, size);
					at += size;
					break;
				}
				case 4:
					need(at, 32);
					walk(at + word(at + 28), box, depth + 1);
					walk(at + word(at + 30), box, depth + 1);
					at += 32;
					break;
				case 5:
					need(at, 36);
					at += 36;
					break;
				case 6: {
					need(at, 20);
					if (word(at + 2) >= submodel_count) throw std::runtime_error("HXM model animation references missing submodel");
					if (boxes.size() >= 10) throw std::runtime_error("HXM model geometry exceeds submodel capacity");
					Box child;
					for (unsigned axis = 0; axis < 3; ++axis) {
						child.offset[axis] = boxes[box].offset[axis] + integer(at + 4 + axis * 4);
						if (child.offset[axis] < INT32_MIN || child.offset[axis] > INT32_MAX)
							throw std::runtime_error("HXM model submodel offset overflows");
					}
					const auto index = boxes.size();
					boxes.push_back(child);
					walk(at + word(at + 16), index, depth + 1);
					at += 20;
					break;
				}
				case 8:
					need(at, 4);
					at += 4;
					break;
				default:
					throw std::runtime_error("HXM model geometry has an unknown opcode");
			}
		}
		active.erase(start);
	}

  public:
	// Capacity matches the native MAX_SUBMODELS; the production caller passes its header count
	ModelGeometry(const uint8_t *bytes, std::size_t size, unsigned count = 10) : data(bytes), length(size), budget(size * 32), submodel_count(count)
	{
		if (!bytes || size > 64 * 1024 * 1024 || !count || count > 10) throw std::runtime_error("HXM model geometry size/count is invalid");
		walk(0, 0, 0);
	}
	int32_t custom_radius() const
	{
		Box overall;
		for (const auto &box : boxes) {
			if (!box.points) continue;
			overall.points = true;
			for (unsigned axis = 0; axis < 3; ++axis) {
				const auto low = box.low[axis] + box.offset[axis], high = box.high[axis] + box.offset[axis];
				if (low < INT32_MIN || high > INT32_MAX) throw std::runtime_error("HXM model geometry coordinate overflows");
				overall.low[axis] = std::min(overall.low[axis], low);
				overall.high[axis] = std::max(overall.high[axis], high);
			}
		}
		if (!overall.points) throw std::runtime_error("HXM model geometry has no vertices");
		std::array<double, 3> half{};
		for (unsigned axis = 0; axis < 3; ++axis) {
			const auto span = overall.high[axis] - overall.low[axis];
			if (span < 0 || span > INT32_MAX) throw std::runtime_error("HXM model geometry extent overflows");
			half[axis] = double(span / 2);
		}
		// Preserve the pinned XL Size arithmetic, including its linear z term
		const double radius = std::sqrt(half[0] * half[0] + half[1] * half[1] + half[2] + half[2]);
		if (!std::isfinite(radius) || radius > INT32_MAX) throw std::runtime_error("HXM model radius overflows");
		return int32_t(radius);
	}
};
} // namespace xl_compat
#endif
