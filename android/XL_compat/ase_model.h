#ifndef DXX_XL_ASE_MODEL_H
#define DXX_XL_ASE_MODEL_H
#include "ase.h"
#include <algorithm>
#include <array>
#include <cmath>
#include <set>

#ifdef _MSC_VER
#pragma pack(push, 8)
#endif
namespace xl_compat
{
using AseVector = std::array<float, 3>;
struct AseMaterial {
	std::string name, texture, opacity;
	AseVector diffuse{ 1, 1, 1 };
	float transparency = 0;
};
struct AseFace {
	std::array<int, 3> vertices{}, texcoords{};
	AseVector normal{};
};
struct AseSubmodel {
	std::string name, parent;
	int material = 0;
	std::vector<AseVector> vertices, normals;
	std::vector<std::array<float, 2>> texcoords;
	std::vector<AseFace> faces;
};
struct AseModel {
	std::vector<AseMaterial> materials;
	std::vector<AseSubmodel> submodels;
};

namespace ase_model_detail
{
inline void require(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(std::string("ASE: ") + message);
}
inline const AseRecord *find(const std::vector<AseRecord> &records, const char *name)
{
	const AseRecord *result = nullptr;
	for (const auto &record : records)
		if (record.name == name) {
			require(!result, "duplicate singleton record");
			result = &record;
		}
	return result;
}
inline const AseRecord &get(const std::vector<AseRecord> &records, const char *name)
{
	const auto *result = find(records, name);
	if (!result) throw std::runtime_error(std::string("ASE: missing ") + name);
	return *result;
}
inline const std::string &value(const AseRecord &record, std::size_t index = 0)
{
	require(index < record.values.size(), "missing record value");
	return record.values[index];
}
inline int integer(std::string text, int limit)
{
	if (!text.empty() && text.back() == ':') text.pop_back();
	std::size_t end = 0;
	const auto number = std::stoll(text, &end);
	require(end == text.size() && number >= 0 && number <= limit, "integer outside bounds");
	return int(number);
}
inline float real(const std::string &text)
{
	std::size_t end = 0;
	const auto number = std::stof(text, &end);
	require(end == text.size() && std::isfinite(number), "invalid real number");
	return number;
}
inline AseVector vector(const AseRecord &record, std::size_t first, bool rotate)
{
	AseVector result{ real(value(record, first)), real(value(record, first + 1)), real(value(record, first + 2)) };
	// XL ASE_ROTATE_MODEL maps exported Z-up coordinates to Descent
	return rotate ? AseVector{ result[0], result[2], -result[1] } : result;
}
template <typename F>
inline void indexed(const AseRecord &block, const char *name, int count, F apply)
{
	require(block.block, "expected indexed block");
	std::vector<bool> seen(count);
	for (const auto &record : block.children)
		if (record.name == name) {
			const int index = integer(value(record), count - 1);
			require(!seen[index], "duplicate indexed record");
			seen[index] = true;
			apply(index, record);
		}
	require(std::all_of(seen.begin(), seen.end(), [](bool v) { return v; }), "incomplete indexed block");
}
} // namespace ase_model_detail

inline AseModel decode_ase_model(const std::vector<AseRecord> &records)
{
	using namespace ase_model_detail;
	AseModel result;
	const auto &materials = get(records, "*MATERIAL_LIST");
	const int material_count = integer(value(get(materials.children, "*MATERIAL_COUNT")), 256);
	require(material_count > 0, "no materials");
	result.materials.resize(material_count);
	indexed(materials, "*MATERIAL", material_count, [&](int index, const AseRecord &record) {
		auto &material = result.materials[index];
		material.name = value(get(record.children, "*MATERIAL_NAME"));
		if (const auto *diffuse = find(record.children, "*MATERIAL_DIFFUSE")) material.diffuse = vector(*diffuse, 0, false);
		if (const auto *alpha = find(record.children, "*MATERIAL_TRANSPARENCY")) material.transparency = real(value(*alpha));
		require(material.transparency >= 0 && material.transparency <= 1, "invalid material transparency");
		const auto *map = find(record.children, "*MAP_DIFFUSE");
		if (!map) map = find(record.children, "*MAP_GENERIC");
		if (map) material.texture = value(get(map->children, "*BITMAP"));
		if (const auto *opacity = find(record.children, "*MAP_OPACITY")) material.opacity = value(get(opacity->children, "*BITMAP"));
		require(!find(record.children, "*SUBMATERIAL"), "nested materials require explicit support");
	});
	std::size_t vertex_budget = 1000000, face_budget = 1000000;
	std::set<std::string> names;
	for (const auto &record : records)
		if (record.name == "*GEOMOBJECT") {
			require(!find(record.children, "*TM_ANIMATION"), "animated transform requires explicit support");
			require(result.submodels.size() < 256, "submodel limit exceeded");
			AseSubmodel part;
			part.name = value(get(record.children, "*NODE_NAME"));
			require(names.insert(part.name).second, "duplicate submodel name");
			if (const auto *parent = find(record.children, "*NODE_PARENT")) part.parent = value(*parent);
			part.material = integer(value(get(record.children, "*MATERIAL_REF")), material_count - 1);
			const auto &mesh = get(record.children, "*MESH");
			const int vertices = integer(value(get(mesh.children, "*MESH_NUMVERTEX")), 65535);
			const int faces = integer(value(get(mesh.children, "*MESH_NUMFACES")), 65535);
			const int texcoords = integer(value(get(mesh.children, "*MESH_NUMTVERTEX")), 65535);
			require(vertices > 0 && faces > 0 && std::size_t(vertices) <= vertex_budget && std::size_t(faces) <= face_budget,
			        "mesh count exceeds bounds");
			vertex_budget -= vertices;
			face_budget -= faces;
			part.vertices.resize(vertices);
			part.normals.resize(vertices);
			part.faces.resize(faces);
			part.texcoords.resize(texcoords);
			indexed(get(mesh.children, "*MESH_VERTEX_LIST"), "*MESH_VERTEX", vertices, [&](int i, const AseRecord &row) {
				part.vertices[i] = vector(row, 1, true);
				for (float v : part.vertices[i]) require(v >= -32767 && v <= 32767, "vertex exceeds fixed-point bounds");
			});
			indexed(get(mesh.children, "*MESH_FACE_LIST"), "*MESH_FACE", faces, [&](int i, const AseRecord &row) {
				for (int corner = 0; corner < 3; ++corner) {
					const std::string label = std::string(1, char('A' + corner)) + ':';
					auto it = std::find(row.values.begin(), row.values.end(), label);
					require(it != row.values.end() && ++it != row.values.end(), "missing face vertex");
					part.faces[i].vertices[corner] = integer(*it, vertices - 1);
				}
			});
			if (texcoords) {
				indexed(get(mesh.children, "*MESH_TVERTLIST"), "*MESH_TVERT", texcoords, [&](int i, const AseRecord &row) {
					part.texcoords[i] = { real(value(row, 1)), -real(value(row, 2)) };
				});
				require(integer(value(get(mesh.children, "*MESH_NUMTVFACES")), 65535) == faces, "texture face count differs");
				indexed(get(mesh.children, "*MESH_TFACELIST"), "*MESH_TFACE", faces, [&](int i, const AseRecord &row) {
					for (int corner = 0; corner < 3; ++corner) part.faces[i].texcoords[corner] = integer(value(row, corner + 1), texcoords - 1);
				});
			}
			const auto &normals = get(mesh.children, "*MESH_NORMALS");
			indexed(normals, "*MESH_FACENORMAL", faces, [&](int i, const AseRecord &row) { part.faces[i].normal = vector(row, 1, true); });
			std::vector<bool> normal_seen(vertices);
			for (const auto &row : normals.children)
				if (row.name == "*MESH_VERTEXNORMAL") {
					const int i = integer(value(row), vertices - 1);
					part.normals[i] = vector(row, 1, true);
					normal_seen[i] = true;
				}
			for (const auto &face : part.faces)
				for (int vertex : face.vertices) require(normal_seen[vertex], "missing referenced vertex normal");
			result.submodels.push_back(std::move(part));
		}
	require(!result.submodels.empty(), "no geometry");
	for (const auto &part : result.submodels) {
		std::set<std::string> visited{ part.name };
		std::string parent = part.parent;
		while (!parent.empty()) {
			require(visited.insert(parent).second, "cyclic parent hierarchy");
			const auto found = std::find_if(result.submodels.begin(), result.submodels.end(), [&](const AseSubmodel &p) { return p.name == parent; });
			require(found != result.submodels.end(), "missing parent submodel");
			parent = found->parent;
		}
	}
	return result;
}
} // namespace xl_compat
#ifdef _MSC_VER
#pragma pack(pop)
#endif
#endif
