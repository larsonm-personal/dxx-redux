#ifndef DXX_XL_MODEL_RENDER_GEOMETRY_H
#define DXX_XL_MODEL_RENDER_GEOMETRY_H
#include "model_geometry.h"
#include <optional>
namespace xl_compat
{
struct RenderModelPart {
	int parent = -1;
	std::array<int32_t, 3> offset{};
	std::vector<std::array<float, 3>> face_vertices;
	std::array<float, 3> low{ 1e30f, 1e30f, 1e30f }, high{ -1e30f, -1e30f, -1e30f };
	unsigned faces = 0, defined_vertices = 0;
};
struct RenderModelSphere {
	std::array<int32_t, 3> center{};
	int32_t radius = 0;
};
struct RenderModelSphereCache {
	unsigned parts = 0, faces = 0, vertices = 0;
	std::array<RenderModelSphere, 2> entries{};
};
// POF render construction visits both sort branches in fixed file order and
// numbers submodels by traversal, independently of animation-angle indices
class ModelRenderGeometry
{
	const uint8_t *data;
	std::size_t length, budget;
	std::set<std::size_t> active;
	std::vector<std::optional<std::array<float, 3>>> vertices;
	void need(std::size_t at, std::size_t size) const
	{
		if (at > length || size > length - at) throw std::runtime_error("POF render geometry is truncated");
	}
	unsigned word(std::size_t at) const
	{
		need(at, 2);
		return unsigned(data[at]) | unsigned(data[at + 1]) << 8;
	}
	int32_t integer(std::size_t at) const
	{
		need(at, 4);
		const uint32_t value = uint32_t(word(at)) | uint32_t(word(at + 2)) << 16;
		return value <= INT32_MAX ? int32_t(value) : -1 - int32_t(~value);
	}
	void walk(std::size_t at, unsigned part, unsigned depth)
	{
		if (depth > 64 || !active.insert(at).second) throw std::runtime_error("POF render geometry is recursive");
		const auto begin = at;
		for (;;) {
			if (!budget--) throw std::runtime_error("POF render geometry exceeds traversal budget");
			const auto op = word(at);
			if (!op) break;
			if (op == 1 || op == 7) {
				const auto count = word(at + 2), first = op == 7 ? word(at + 4) : 0;
				const auto points = at + (op == 7 ? 8 : 4);
				need(points, std::size_t(count) * 12);
				if (first + count > 65535) throw std::runtime_error("POF render vertex count overflows source storage");
				vertices.resize(std::max(vertices.size(), std::size_t(first + count)));
				for (unsigned i = 0; i < count; ++i) {
					std::array<float, 3> point;
					for (unsigned axis = 0; axis < 3; ++axis) point[axis] = float(integer(points + i * 12 + axis * 4)) / 65536.0f;
					vertices[first + i] = point;
				}
				parts[part].defined_vertices += count;
				at = points + count * 12;
			} else if (op == 2 || op == 3) {
				const auto count = word(at + 2);
				const auto size = 30 + (count | 1) * 2 + (op == 3 ? count * 12 : 0);
				need(at, size);
				if (++face_count > 65535 || face_vertex_count + count > 65535) throw std::runtime_error("POF render face counts overflow source storage");
				++parts[part].faces;
				face_vertex_count += count;
				for (unsigned i = 0; i < count; ++i) {
					const auto index = word(at + 30 + i * 2);
					if (index >= vertices.size() || !vertices[index]) throw std::runtime_error("POF render face uses an undefined vertex");
					parts[part].face_vertices.push_back(*vertices[index]);
					for (unsigned axis = 0; axis < 3; ++axis) {
						parts[part].low[axis] = std::min(parts[part].low[axis], (*vertices[index])[axis]);
						parts[part].high[axis] = std::max(parts[part].high[axis], (*vertices[index])[axis]);
					}
				}
				at += size;
			} else if (op == 4) {
				need(at, 32);
				walk(at + word(at + 28), part, depth + 1);
				walk(at + word(at + 30), part, depth + 1);
				at += 32;
			} else if (op == 6) {
				need(at, 20);
				if (parts.size() >= 64) throw std::runtime_error("POF render submodels exceed hitbox capacity");
				RenderModelPart child;
				child.parent = int(part);
				for (unsigned axis = 0; axis < 3; ++axis) child.offset[axis] = integer(at + 4 + axis * 4);
				parts.push_back(child);
				walk(at + word(at + 16), unsigned(parts.size() - 1), depth + 1);
				at += 20;
			} else if (op == 5 || op == 8) {
				const auto size = op == 5 ? 36 : 4;
				need(at, size);
				at += size;
			} else throw std::runtime_error("Unknown POF render opcode");
		}
		active.erase(begin);
	}

  public:
	std::vector<RenderModelPart> parts{ 1 };
	unsigned face_count = 0, face_vertex_count = 0;
	ModelRenderGeometry(const uint8_t *bytes, std::size_t size) : data(bytes), length(size), budget(size * 4 + 1)
	{
		if (!bytes || size < 2 || size > 64 * 1024 * 1024) throw std::runtime_error("Invalid POF render payload size");
		walk(0, 0, 0);
	}
	unsigned vertex_count() const
	{
		return unsigned(vertices.size());
	}
	std::vector<ModelHitboxBounds> hitbox_bounds() const
	{
		const auto fixed = [](double value) {
			if (!std::isfinite(value) || value < INT32_MIN || value > INT32_MAX) throw std::runtime_error("POF render bounds overflow");
			return int32_t(value);
		};
		std::vector<ModelHitboxBounds> result(parts.size() + 1);
		result[0].low.fill(INT32_MAX);
		result[0].high.fill(-INT32_MAX);
		for (std::size_t i = 0; i < parts.size(); ++i) {
			const auto &part = parts[i];
			auto &box = result[i + 1];
			if (!part.faces || part.parent >= int(i)) throw std::runtime_error("POF render part has no qualified face bounds");
			for (unsigned axis = 0; axis < 3; ++axis) {
				box.low[axis] = fixed(double(part.low[axis]) * 65536.0);
				box.high[axis] = fixed(double(part.high[axis]) * 65536.0);
				box.offset[axis] = fixed(double(part.offset[axis]) + (part.parent < 0 ? 0 : result[part.parent + 1].offset[axis]));
				box.half_size[axis] = fixed(double(int64_t(box.high[axis]) - box.low[axis]) / 2.0);
				result[0].low[axis] = std::min(result[0].low[axis], fixed(double(box.low[axis]) + box.offset[axis]));
				result[0].high[axis] = std::max(result[0].high[axis], fixed(double(box.high[axis]) + box.offset[axis]));
			}
		}
		// RenderModel::Size stores the full combined span, unlike submodel halves
		for (unsigned axis = 0; axis < 3; ++axis) result[0].half_size[axis] = fixed(double(result[0].high[axis]) - result[0].low[axis]);
		return result;
	}
	RenderModelSphere bounding_sphere(RenderModelSphereCache *cache = nullptr, int type = -1) const
	{
		if (type < -1 || type > 1 || (type >= 0 && !cache)) throw std::runtime_error("Invalid POF sphere cache profile");
		if (type >= 0 && cache->parts == parts.size() && cache->faces == face_count && cache->vertices == face_vertex_count) return cache->entries[type];
		const auto bounds = hitbox_bounds();
		using Point = std::array<float, 3>;
		std::vector<Point> vertices;
		for (std::size_t i = 0; i < parts.size(); ++i)
			for (auto point : parts[i].face_vertices) {
				for (unsigned axis = 0; axis < 3; ++axis) point[axis] += float(bounds[i + 1].offset[axis]) / 65536.0f;
				vertices.push_back(point);
			}
		if (vertices.size() < 2) throw std::runtime_error("POF sphere requires at least two face vertices");
		std::sort(vertices.begin(), vertices.end());
		// Source passes count-1 to FilterVertices after sorting the entire array
		vertices.pop_back();
		vertices.erase(std::unique(vertices.begin(), vertices.end()), vertices.end());
		if (vertices.size() > 4096) throw std::runtime_error("POF sphere exceeds pair-comparison budget");
		const auto difference = [](const Point &a, const Point &b) { return Point{ a[0] - b[0], a[1] - b[1], a[2] - b[2] }; };
		const auto magnitude = [](const Point &v) { return float(std::sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2])); };
		float radius = 0;
		for (std::size_t i = 0; i + 1 < vertices.size(); ++i)
			for (std::size_t j = i + 1; j < vertices.size(); ++j) radius = std::max(radius, magnitude(difference(vertices[i], vertices[j])));
		radius /= 2;
		Point center;
		for (unsigned axis = 0; axis < 3; ++axis) center[axis] = float((int64_t(bounds[0].low[axis]) + bounds[0].high[axis]) / 2) / 65536.0f;
		for (const auto &point : vertices) {
			const auto delta = difference(point, center);
			const float distance = magnitude(delta);
			if (radius < distance) {
				const float scale = (distance - radius) / distance;
				for (unsigned axis = 0; axis < 3; ++axis) center[axis] += delta[axis] * scale;
			}
		}
		for (const auto &point : vertices) radius = std::max(radius, magnitude(difference(point, center)));
		const auto fixed = [](float value) {
			const double scaled = double(value) * 65536.0;
			if (!std::isfinite(scaled) || scaled < INT32_MIN || scaled > INT32_MAX) throw std::runtime_error("POF sphere exceeds fixed-point range");
			return int32_t(scaled);
		};
		RenderModelSphere result{ { fixed(center[0]), fixed(center[1]), fixed(center[2]) }, fixed(radius) };
		if (type >= 0) {
			cache->parts = unsigned(parts.size());
			cache->faces = face_count;
			cache->vertices = face_vertex_count;
			cache->entries[type] = result;
		}
		return result;
	}
};
} // namespace xl_compat
#endif
