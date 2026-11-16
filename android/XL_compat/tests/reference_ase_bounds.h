#ifndef DXX_XL_REFERENCE_ASE_BOUNDS_H
#define DXX_XL_REFERENCE_ASE_BOUNDS_H
namespace pof_reference
{
namespace ASE
{
struct CFace {
	int m_nVerts[3]{}, m_nTexCoord[3]{};
	CFloatVector3 m_vNormal{};
};
struct CSubModel : pof_reference::CSubModel {
	CSubModel *m_next = nullptr;
	int m_nVerts = 0, m_nBitmap = 0, m_nBullets = 0;
	bool m_bBarrel = false;
	CArray<CFace> m_faces;
	CArray<CVertex> m_vertices;
	CArray<decltype(CVertex{}.m_texCoord)> m_texCoord;
	CFloatVector3 m_vOffset{};
};
struct CModel {
	CSubModel *m_subModels = nullptr;
	struct {
		CArray<CBitmap> m_bitmaps;
	} m_textures;
};
} // namespace ASE
#include "xl_reference_ase_conversion.h"

inline void ase_bounds(const xl_compat::AseModel &decoded)
{
	const auto actual = xl_compat::compile_ase_mesh(decoded).geometry.hitboxes;
	ASE::CModel input;
	input.m_textures.m_bitmaps.values.resize(decoded.materials.size());
	std::vector<ASE::CSubModel> parts(decoded.submodels.size());
	input.m_subModels = parts.data();
	unsigned vertices = 0, faces = 0;
	for (std::size_t i = 0; i < parts.size(); ++i) {
		const auto &original = decoded.submodels[i];
		auto &part = parts[i];
		part.m_nSubModel = int(i);
		part.m_nParent = -1;
		if (!original.parent.empty()) {
			const auto parent = std::find_if(decoded.submodels.begin(), decoded.submodels.end(), [&](const xl_compat::AseSubmodel &p) { return p.name == original.parent; });
			if (parent == decoded.submodels.end()) throw std::runtime_error("ASE reference parent is missing");
			part.m_nParent = int(parent - decoded.submodels.begin());
		}
		part.m_next = i + 1 == parts.size() ? nullptr : &parts[i + 1];
		auto name = original.name;
		for (char &c : name) c = char(std::toupper(static_cast<unsigned char>(c)));
		part.m_bThruster = name.rfind("$THRUSTER", 0) == 0;
		part.m_nGunPoint = name.rfind("$GUNPNT-", 0) == 0 ? 0 : -1;
		part.m_nBitmap = original.material;
		part.m_nVerts = int(original.vertices.size());
		part.m_nFaces = int(original.faces.size());
		vertices += unsigned(original.vertices.size());
		faces += unsigned(original.faces.size());
		part.m_vertices.values.resize(original.vertices.size());
		for (std::size_t v = 0; v < original.vertices.size(); ++v) {
			const auto &position = original.vertices[v], &normal = original.normals.at(v);
			part.m_vertices[v].m_vertex = CFloatVector3::Create(position[0], position[1], position[2]);
			part.m_vertices[v].m_normal = CFloatVector3::Create(normal[0], normal[1], normal[2]);
		}
		for (const auto &uv : original.texcoords) part.m_texCoord.values.push_back({ { uv[0], uv[1] } });
		for (const auto &face : original.faces) {
			ASE::CFace target;
			for (unsigned c = 0; c < 3; ++c) {
				target.m_nVerts[c] = face.vertices[c];
				target.m_nTexCoord[c] = face.texcoords[c];
			}
			target.m_vNormal = CFloatVector3::Create(face.normal[0], face.normal[1], face.normal[2]);
			part.m_faces.values.push_back(target);
		}
	}
	CModel source;
	source.m_nSubModels = int(parts.size());
	source.m_nFaces = int(faces);
	source.m_nFaceVerts = int(faces * 3);
	source.m_subModels.values.resize(parts.size());
	source.m_faces.values.resize(faces);
	source.m_faceVerts.values.resize(faces * 3);
	source.m_vertices.values.resize(vertices);
	source.m_vertexOwner.values.resize(vertices);
	source.m_vertNorms.values.resize((std::max) (vertices, faces * 3));
	source.GetASEModelItems(0, &input, 1.0f);
	// Geometry sizing executes in full; radius and face-normal fidelity are separate owners
	gameData.modelData.spheres[0] = {};
	gameData.modelData.spheres[0].nSubModels = source.m_nSubModels;
	gameData.modelData.spheres[0].nFaces = source.m_nFaces;
	gameData.modelData.spheres[0].nFaceVerts = source.m_nFaceVerts;
	source.Size(nullptr, 1);
	const auto &expected = gameData.modelData.hitboxes[0];
	if (actual.size() != std::size_t(expected.nHitboxes + 1)) throw std::runtime_error("ASE hitbox count differs from source conversion and sizing");
	const auto point = [](const CFixVector &p) { return std::array<int32_t, 3>{ p.v.coord.x, p.v.coord.y, p.v.coord.z }; };
	for (std::size_t i = 0; i < actual.size(); ++i) {
		const auto &box = expected.hitboxes[i];
		if (actual[i].low != point(box.vMin) || actual[i].high != point(box.vMax) || actual[i].offset != point(box.vOffset) || actual[i].half_size != point(box.vSize))
			throw std::runtime_error("ASE retained hitbox differs from source conversion and sizing");
	}
	std::cout << "ASE source conversion/sizing: " << parts.size() << " parts, " << actual.size() - 1 << " body hitboxes passed\n";
}
inline void ase_bounds_controls()
{
	for (int count : { 1, 2, 4 })
		for (bool unused : { false, true }) {
			xl_compat::AseModel model;
			model.materials.push_back({ "body", "owned.tga", "", { 1, 1, 1 }, 0 });
			for (int i = 0; i < count; ++i) {
				xl_compat::AseSubmodel part;
				part.name = i == 1 ? "$thruster-RXX-00" : i == 2 ? "$gunpnt-0"
				                                                 : "body" + std::to_string(i);
				if (i) part.parent = "body0";
				part.vertices = { { -1.125f - i, 0, 0 }, { 1.875f, 0, 0 }, { 0, 2.3456f, 3.6789f + i } };
				part.normals.resize(3, { 0, 1, 0 });
				part.texcoords = { { 0, 0 }, { 1, 0 }, { 0, 1 } };
				part.faces.push_back({ { 0, 1, 2 }, { 0, 1, 2 }, { 0, 1, 0 } });
				if (unused) {
					part.vertices.push_back({ 500, -500, 700 });
					part.normals.push_back({ 0, 1, 0 });
				}
				model.submodels.push_back(part);
			}
			ase_bounds(model);
		}
}
} // namespace pof_reference
#endif
