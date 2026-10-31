#ifndef DXX_XL_PARTICLE_BUFFER_H
#define DXX_XL_PARTICLE_BUFFER_H
#include <array>
#include <cmath>
#include <cstddef>
#include <vector>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
// Source buffers retain particle image/render type and emissive policy
class FireParticleBuffers
{
	using Point = std::array<float, 3>;
	struct Buffer {
		std::vector<std::size_t> entries;
		Point center{};
		float radius = 0, distance = 0;
		int kind = -1;
		bool emissive = false;
	};
	std::array<Buffer, 3> buffers;
	Point eye;
	static bool compatible(const Buffer &buffer, int kind, bool emissive)
	{
		// Of the admitted image types, source shader compatibility includes smoke
		return buffer.kind == kind && (buffer.emissive == emissive || kind == 1);
	}
	static float distance(const Point &a, const Point &b)
	{
		const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
		return std::sqrt(x * x + y * y + z * z);
	}
	template <class Draw>
	void flush(Buffer &buffer, Draw &draw)
	{
		if (buffer.entries.empty()) return;
		if (buffer.kind >= 0 && buffer.entries.size() >= 2) draw(buffer.entries);
		buffer.entries.clear();
		buffer.center = {};
		buffer.radius = buffer.distance = 0;
		buffer.kind = -1;
		buffer.emissive = false;
	}
	template <class Draw>
	void append(Buffer &buffer, std::size_t index, const Point &position, float radius, int kind, bool emissive, Draw &draw)
	{
		if (buffer.entries.size() == 25000 || !compatible(buffer, kind, emissive)) flush(buffer, draw);
		if (buffer.entries.empty()) {
			buffer.kind = kind;
			buffer.emissive = emissive;
		}
		buffer.entries.push_back(index);
		if (buffer.radius == 0) {
			buffer.center = position;
			buffer.radius = radius;
		} else {
			const float d = distance(position, buffer.center);
			float h = radius + d - buffer.radius;
			if (h > 0) {
				h /= 2;
				for (unsigned axis = 0; axis < 3; ++axis) {
					float v = position[axis] - buffer.center[axis];
					v /= d;
					v *= h;
					buffer.center[axis] += v;
				}
				buffer.radius += h;
			}
			// Preserve the source's additional averaging after radius expansion
			for (unsigned axis = 0; axis < 3; ++axis) buffer.center[axis] = (buffer.center[axis] + position[axis]) / 2;
		}
		const float d = distance(position, eye);
		if (buffer.distance < d) buffer.distance = d;
	}

  public:
	explicit FireParticleBuffers(const Point &viewer) : eye(viewer) {}
	template <class Draw>
	void add(std::size_t index, const Point &position, float radius, bool smoke, bool emissive, Draw draw)
	{
		add_kind(index, position, radius, smoke ? 16 : 5, emissive, draw);
	}
	template <class Draw>
	void add_kind(std::size_t index, const Point &position, float radius, int kind, bool emissive, Draw draw)
	{
		for (unsigned i = 0; i < buffers.size(); ++i) {
			auto &buffer = buffers[i];
			if (!compatible(buffer, kind, emissive)) continue;
			for (unsigned j = 0; j < buffers.size(); ++j)
				if (j != i && distance(buffers[j].center, position) < buffers[j].radius + radius) flush(buffers[j], draw);
			append(buffer, index, position, radius, kind, emissive, draw);
			return;
		}
		// A newly allocated/replaced source buffer bypasses overlap flushing
		unsigned selected = 0;
		for (unsigned i = 0; i < buffers.size(); ++i) {
			if (buffers[i].kind < 0) {
				selected = i;
				break;
			}
			if (buffers[i].entries.size() > buffers[selected].entries.size()) selected = i;
		}
		append(buffers[selected], index, position, radius, kind, emissive, draw);
	}
	template <class Draw>
	void flush(Draw draw)
	{
		for (unsigned i = 0; i < buffers.size(); ++i) {
			float farthest = 0;
			int selected = -1;
			for (unsigned j = 0; j < buffers.size(); ++j)
				if (!buffers[j].entries.empty() && farthest < buffers[j].distance) {
					farthest = buffers[j].distance;
					selected = int(j);
				}
			if (selected < 0) break;
			flush(buffers[unsigned(selected)], draw);
		}
	}
};
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
