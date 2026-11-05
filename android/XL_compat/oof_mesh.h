#ifndef DXX_XL_OOF_MESH_H
#define DXX_XL_OOF_MESH_H
#include "oof.h"
#include <limits>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
struct OofRenderVertex {
	OofVector position{}, normal{};
	std::array<float, 2> uv{};
	std::array<float, 4> color{ 1, 1, 1, 1 };
};
struct OofRenderFace {
	int submodel = -1, texture = -1;
	bool collision = true;
	OofVector normal{};
	std::vector<OofRenderVertex> triangles;
};
struct OofMesh {
	std::vector<OofRenderFace> faces;
	OofVector low{}, high{};
	std::map<int, OofVector> offsets;
	std::map<int, OofVector> spinning_parts;
};

// Keep face order and material boundaries; alpha faces must not be regrouped by texture
inline OofMesh compile_oof_mesh(const OofModel &model, bool flip_v)
{
	OofMesh mesh;
	mesh.low.fill((std::numeric_limits<float>::max)());
	mesh.high.fill(-(std::numeric_limits<float>::max)());
	std::size_t vertex_budget = 1024 * 1024;
	for (const auto &chunk : model.additional_chunks)
		if (chunk.type != "PINF" && !(chunk.type == "SPCL" && model.has_special_points) &&
		    !(chunk.type == "RANI" && model.has_rotation_tracks && model.rotation_tracks.size() == model.submodels.size()))
			throw std::runtime_error("OOF runtime does not support chunk " + chunk.type);
	bool points = false;
	for (const auto &entry : model.submodels) {
		const auto &part = entry.second;
		// New metadata admission is limited to the opaque static reference profile
		if ((model.has_special_points || model.has_rotation_tracks) &&
		    std::any_of(part.alpha.begin(), part.alpha.end(), [](float value) { return value != 1.0f; }))
			throw std::runtime_error("OOF metadata profile requires opaque vertex alpha");
		if (!part.properties.empty() || !part.facing_lists.empty())
			throw std::runtime_error("OOF runtime requires property/facing-list support for " + part.name);
		OofVector offset{};
		int parent = part.index;
		for (std::size_t depth = 0; parent != -1; ++depth) {
			const auto found = model.submodels.find(parent);
			if (found == model.submodels.end() || depth >= model.submodels.size()) throw std::runtime_error("Invalid OOF mesh hierarchy");
			for (unsigned axis = 0; axis < 3; ++axis) offset[axis] += found->second.offset[axis];
			parent = found->second.parent == parent ? -1 : found->second.parent;
		}
		mesh.offsets.emplace(part.index, offset);
		for (const auto &vertex : part.vertices) {
			points = true;
			for (unsigned axis = 0; axis < 3; ++axis) {
				const float value = vertex[axis] + offset[axis];
				if (!std::isfinite(value) || value < -32767 || value > 32767) throw std::runtime_error("OOF geometry exceeds native fixed-point bounds");
				mesh.low[axis] = (std::min) (mesh.low[axis], value);
				mesh.high[axis] = (std::max) (mesh.high[axis], value);
			}
		}
		for (const auto &face : part.faces) {
			if (face.corners.size() < 3 || (face.corners.size() - 2) * 3 > vertex_budget) throw std::runtime_error("OOF mesh exceeds triangle budget");
			vertex_budget -= (face.corners.size() - 2) * 3;
			OofRenderFace output;
			output.submodel = part.index;
			output.texture = face.texture;
			output.normal = face.normal;
			const auto emit = [&](const OofCorner &corner) {
				OofRenderVertex vertex;
				vertex.position = part.vertices.at(corner.vertex);
				for (unsigned axis = 0; axis < 3; ++axis) vertex.position[axis] += offset[axis];
				vertex.normal = part.normals.at(corner.vertex);
				vertex.uv = { corner.u, flip_v ? -corner.v : corner.v };
				if (face.texture < 0)
					for (unsigned channel = 0; channel < 3; ++channel) vertex.color[channel] = face.color[channel] / 255.0f;
				vertex.color[3] = part.alpha.at(corner.vertex);
				output.triangles.push_back(vertex);
			};
			for (std::size_t corner = 1; corner + 1 < face.corners.size(); ++corner) {
				emit(face.corners[0]);
				emit(face.corners[corner]);
				emit(face.corners[corner + 1]);
			}
			mesh.faces.push_back(std::move(output));
		}
	}
	if (!points || mesh.faces.empty()) throw std::runtime_error("OOF mesh has no geometry");
	return mesh;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
