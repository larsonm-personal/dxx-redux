#ifndef DXX_XL_ASE_MESH_H
#define DXX_XL_ASE_MESH_H
#include "ase_model.h"
#include "oof_mesh.h"
#include <cctype>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
struct AseThruster {
	int submodel = -1, flags = 0;
	OofVector center{};
};
struct AseMesh {
	OofMesh geometry;
	std::vector<std::string> textures;
	std::map<int, OofVector> gunpoints;
	std::vector<AseThruster> thrusters;
};
inline AseMesh compile_ase_mesh(const AseModel &model)
{
	AseMesh result;
	auto &mesh = result.geometry;
	mesh.low.fill((std::numeric_limits<float>::max)());
	mesh.high.fill(-(std::numeric_limits<float>::max)());
	std::vector<int> texture_ids;
	for (const auto &material : model.materials) {
		if (!material.opacity.empty()) throw std::runtime_error("ASE opacity map requires explicit material support");
		if (material.texture.empty()) texture_ids.push_back(-1);
		else {
			texture_ids.push_back(int(result.textures.size()));
			// XL strips exporter paths; ownership is checked against the mission's models directory
			const auto separator = material.texture.find_last_of("/\\");
			result.textures.push_back(material.texture.substr(separator == std::string::npos ? 0 : separator + 1));
		}
	}
	for (std::size_t index = 0; index < model.submodels.size(); ++index) {
		const auto &part = model.submodels[index];
		auto name = part.name;
		for (char &c : name) c = char(std::toupper(static_cast<unsigned char>(c)));
		const bool thruster = name.rfind("$THRUSTER", 0) == 0;
		if (thruster) {
			int flags = 1;
			if (name.rfind("$THRUSTER-", 0) == 0) {
				flags = 0;
				if (name.size() > 10) {
					if (name[10] == 'R') flags |= 1;
					else if (name[10] == 'F') flags |= 2;
				}
				if (name.size() > 11) {
					if (name[11] == 'L') flags |= 4;
					else if (name[11] == 'R') flags |= 8;
				}
				if (name.size() > 12) {
					if (name[12] == 'T') flags |= 16;
					else if (name[12] == 'B') flags |= 32;
				}
				if (!flags) flags = 1;
				flags |= flags < 3 ? 64 : 128;
			}
			OofVector low{}, high{}, center{};
			low.fill((std::numeric_limits<float>::max)());
			high.fill(-(std::numeric_limits<float>::max)());
			for (const auto &face : part.faces)
				for (int id : face.vertices)
					for (unsigned axis = 0; axis < 3; ++axis) {
						low[axis] = (std::min) (low[axis], part.vertices.at(id)[axis]);
						high[axis] = (std::max) (high[axis], part.vertices.at(id)[axis]);
					}
			for (unsigned axis = 0; axis < 3; ++axis)
				center[axis] = float((int64_t(low[axis] * 65536) + int64_t(high[axis] * 65536)) / 2) / 65536;
			result.thrusters.push_back({ int(index), flags, center });
			// G3FilterSubModel retains only rear/frontal animated thruster polygons
			if ((flags & 65) != 65) continue;
			mesh.spinning_parts.emplace(int(index), center);
		}
		if (name.rfind("$GUNPNT-", 0) == 0) {
			const int gun = ase_model_detail::integer(name.substr(8), 7);
			OofVector low{}, high{}, center{};
			low.fill((std::numeric_limits<float>::max)());
			high.fill(-(std::numeric_limits<float>::max)());
			// Source SetMinMax uses face vertices; marker triangles never render or collide
			for (const auto &face : part.faces)
				for (int id : face.vertices)
					for (unsigned axis = 0; axis < 3; ++axis) {
						low[axis] = (std::min) (low[axis], part.vertices.at(id)[axis]);
						high[axis] = (std::max) (high[axis], part.vertices.at(id)[axis]);
					}
			for (unsigned axis = 0; axis < 3; ++axis) {
				// Source converts bounds to fixed point before computing the midpoint
				const int64_t a = int32_t(low[axis] * 65536), b = int32_t(high[axis] * 65536);
				center[axis] = float((a + b) / 2) / 65536;
			}
			if (!result.gunpoints.emplace(gun, center).second) throw std::runtime_error("Duplicate ASE gunpoint marker");
			continue;
		}
		if (!thruster && (name.find('$') != std::string::npos || name.find("GLOW") != std::string::npos || name.find("FLARE") != std::string::npos))
			throw std::runtime_error("ASE conditional submodel requires explicit rendering support: " + part.name);
		const auto &material = model.materials.at(part.material);
		mesh.offsets.emplace(int(index), OofVector{});
		if (!thruster)
			for (const auto &position : part.vertices)
				for (unsigned axis = 0; axis < 3; ++axis) {
					mesh.low[axis] = (std::min) (mesh.low[axis], position[axis]);
					mesh.high[axis] = (std::max) (mesh.high[axis], position[axis]);
				}
		for (const auto &face : part.faces) {
			OofRenderFace output;
			output.submodel = int(index);
			output.collision = !thruster;
			output.texture = texture_ids.at(part.material);
			output.normal = face.normal;
			for (int corner = 0; corner < 3; ++corner) {
				OofRenderVertex vertex;
				vertex.position = part.vertices.at(face.vertices[corner]);
				vertex.normal = part.normals.at(face.vertices[corner]);
				if (!part.texcoords.empty()) vertex.uv = part.texcoords.at(face.texcoords[corner]);
				else if (output.texture >= 0) throw std::runtime_error("Textured ASE face has no UV coordinates");
				if (output.texture < 0)
					for (unsigned channel = 0; channel < 3; ++channel) vertex.color[channel] = material.diffuse[channel];
				vertex.color[3] = 1 - material.transparency;
				output.triangles.push_back(vertex);
			}
			mesh.faces.push_back(std::move(output));
		}
	}
	if (mesh.faces.empty()) throw std::runtime_error("ASE mesh has no faces");
	for (unsigned axis = 0; axis < 3; ++axis)
		if (mesh.low[axis] > mesh.high[axis]) throw std::runtime_error("ASE mesh has no collision body");
	return result;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
