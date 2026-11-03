#ifndef DXX_XL_OOF_GEOMETRY_H
#define DXX_XL_OOF_GEOMETRY_H
#include "oof_mesh.h"

namespace xl_compat
{
// Native collision/size consumers use the exact static OOF vertices, not a substitute model
// Rendering remains owned by the OOF renderer and its original materials
inline std::vector<uint8_t> oof_native_geometry(const OofMesh &mesh)
{
	using Point = std::array<int32_t, 3>;
	std::map<Point, unsigned> indices;
	std::vector<Point> points;
	std::vector<std::array<unsigned, 3>> triangles;
	std::vector<OofVector> normals;
	for (const auto &face : mesh.faces) {
		if (!face.collision) continue;
		for (std::size_t i = 0; i < face.triangles.size(); i += 3) {
			std::array<unsigned, 3> triangle{};
			for (unsigned corner = 0; corner < 3; ++corner) {
				Point point{};
				for (unsigned axis = 0; axis < 3; ++axis) point[axis] = int32_t(face.triangles.at(i + corner).position[axis] * 65536);
				const auto inserted = indices.emplace(point, unsigned(points.size()));
				if (inserted.second) points.push_back(point);
				triangle[corner] = inserted.first->second;
			}
			triangles.push_back(triangle);
			normals.push_back(face.normal);
		}
	}
	if (points.empty()) throw std::runtime_error("OOF native geometry has no collision vertices");
	if (points.size() > 1000) {
		// DEFPOINTS replaces the native scratch vertex bank; emit bounded batches
		// without dropping triangles or reducing the authored collision geometry
		std::vector<uint8_t> result;
		OofMesh batch;
		std::size_t corners = 0;
		const auto flush = [&]() {
			if (!corners) return;
			auto payload = oof_native_geometry(batch);
			payload.resize(payload.size() - 2);
			if (result.size() + payload.size() + 2 > 16 * 1024 * 1024)
				throw std::runtime_error("OOF native geometry exceeds payload capacity");
			result.insert(result.end(), payload.begin(), payload.end());
			batch.faces.clear();
			corners = 0;
		};
		for (const auto &face : mesh.faces) {
			if (!face.collision) continue;
			for (std::size_t i = 0; i < face.triangles.size(); i += 3) {
				if (corners + 3 > 999) flush();
				OofRenderFace triangle;
				triangle.normal = face.normal;
				for (unsigned corner = 0; corner < 3; ++corner) triangle.triangles.push_back(face.triangles.at(i + corner));
				batch.faces.push_back(std::move(triangle));
				corners += 3;
			}
		}
		flush();
		result.insert(result.end(), 2, 0);
		return result;
	}
	std::vector<uint8_t> bytes;
	const auto word = [&](unsigned value) { bytes.push_back(uint8_t(value)); bytes.push_back(uint8_t(value >> 8)); };
	const auto integer = [&](int32_t value) { const auto bits = uint32_t(value); word(bits); word(bits >> 16); };
	word(1);
	word(unsigned(points.size()));
	for (const auto &point : points)
		for (auto axis : point) integer(axis);
	for (std::size_t i = 0; i < triangles.size(); ++i) {
		word(2);
		word(3);
		for (auto axis : points[triangles[i][0]]) integer(axis);
		for (auto axis : normals[i]) {
			if (axis < -32767 || axis > 32767) throw std::runtime_error("OOF normal exceeds native bounds");
			integer(int32_t(axis * 65536));
		}
		word(0);
		for (auto index : triangles[i]) word(index);
	}
	word(0);
	return bytes;
}
} // namespace xl_compat
#endif
