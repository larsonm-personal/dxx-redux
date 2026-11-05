// Diagnostic oracle only: execute the pinned optimized OOF conversion before
// admission
#include "oof_fixture.h"
#include "../oof_mesh.h"
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>

template <class T>
struct Array : std::vector<T> {
	using std::vector<T>::vector;
	T *Buffer()
	{
		return this->data();
	}
	operator T *()
	{
		return this->data();
	}
	void Clear()
	{
		std::fill(this->begin(), this->end(), T{});
	}
};
struct V3 {
	struct {
		struct {
			float x = 0, y = 0, z = 0;
		} coord;
	} v;
	V3() = default;
	void SetZero()
	{
		v.coord = {};
	}
	explicit V3(const xl_compat::OofVector &a)
	{
		v.coord = { a[0], a[1], a[2] };
	}
	static void Normalize(V3 &v)
	{
		const float m = std::sqrt(v.v.coord.x * v.v.coord.x + v.v.coord.y * v.v.coord.y + v.v.coord.z * v.v.coord.z);
		if (m > 0) {
			v.v.coord.x /= m;
			v.v.coord.y /= m;
			v.v.coord.z /= m;
		}
	}
	V3 *XYZ()
	{
		return this;
	}
	V3 operator+(const V3 &b) const
	{
		return V3({ v.coord.x + b.v.coord.x, v.coord.y + b.v.coord.y,
		            v.coord.z + b.v.coord.z });
	}
	V3 operator-(const V3 &b) const
	{
		return V3({ v.coord.x - b.v.coord.x, v.coord.y - b.v.coord.y,
		            v.coord.z - b.v.coord.z });
	}
	V3 &operator+=(const V3 &b)
	{
		*this = *this + b;
		return *this;
	}
	void Assign(const V3 &a)
	{
		*this = a;
	}
	V3 &operator*=(float f)
	{
		v.coord.x *= f;
		v.coord.y *= f;
		v.coord.z *= f;
		return *this;
	}
	static V3 Normal(const V3 &, const V3 &, const V3 &)
	{
		throw std::runtime_error("Outside model-132 profile");
	}
};
using CFloatVector3 = V3;
using CFloatVector = V3;
struct CObject {
	struct {
		struct {
			V3 vPos;
			int mOrient = 0;
		} position;
	} info;
};
struct {
	struct {
		int nShadowPass = 0;
		bool bShadowMap = false;
		int nShadowMap = 0;
	} render;
	struct {
		int nSDLTicks[1]{};
	} app;
} gameStates;
constexpr bool IsMultiGame = false;
struct {
	struct {
		bool BrightPlayers = false;
	} m_info;
} netGameInfo;
constexpr int GL_TEXTURE0 = 0;
#define FAST_SHADOWS  0
#define OOF_TEST_CUBE 0
#define Assert(x)                                                  \
	do {                                                           \
		if (!(x)) throw std::runtime_error("Reference assertion"); \
	} while (false)

struct {
	struct {
		bool bUseTransform = true;
	} m_states;
	void SetFaceCulling(bool) {}
	void SelectTMU(int) {}
	void SetTexturing(bool) {}
} ogl;
V3 vPos;
struct IdentityView {
	void Assign(const IdentityView &) {}
	V3 operator*(const V3 &v) const
	{
		return v;
	}
} mView;
struct {
	struct {
		IdentityView view[1];
		V3 pos;
	} m_info;
	void Begin(const V3 &, int, const char *, int) {}
	void End(const char *, int) {}
} transformation;
struct Color {
	float c[4]{};
	float &Red()
	{
		return c[0];
	}
	float &Green()
	{
		return c[1];
	}
	float &Blue()
	{
		return c[2];
	}
	float &Alpha()
	{
		return c[3];
	}
};
namespace OOF
{
struct CModel;
struct CFaceVert {
	int m_nIndex = 0;
	float m_fu = 0, m_fv = 0;
};
struct CFace {
	int m_nVerts = 0, m_bTextured = 0;
	struct {
		int nTexId = 0;
		Color color;
	} m_texProps;
	V3 m_vNormal, m_vRotNormal, m_vCenter, m_vRotCenter;
	int m_bReverse = 0;
	CFaceVert *m_vertices = nullptr;
};
struct CSubModel {
	int m_nParent = -1, m_nVerts = 0, m_nIndex = 0;
	void TransformVertex(V3 *, V3 *, V3 *);
	void Transform(V3);
	int32_t Render(CObject *, CModel *, V3, int32_t, float *);
	int Draw(CObject *, CModel *, float *)
	{
		return 1;
	}
	V3 m_vOffset, m_vMin, m_vMax;
	int m_nChildren = 0;
	Array<int> m_children;
	Array<V3> m_vertices, m_rotVerts;
	struct {
		int m_nFaces = 0;
		Array<CFace> m_list;
	} m_faces;
};
struct CModel {
	int m_nSubModels = 0;
	Array<CSubModel> m_subModels;
	V3 m_vMin, m_vMax;
	void GetSubModelBounds(CSubModel *, V3);
	int m_bCloaked = 0, m_nCloakPulse = 0, m_nCloakChangedTime = 0;
	float m_fAlpha = 1;
	int32_t Render(CObject *, float *, int32_t);
	int32_t Draw(CObject *, float *);
	int32_t RenderShadow(CObject *, float *)
	{
		throw std::runtime_error("Shadow profile not qualified");
	}
};
} // namespace OOF
namespace RenderModel
{
struct CVertex {
	int m_nIndex = 0, m_bTextured = 0;
	struct {
		struct {
			float u = 0, v = 0;
		} v;
	} m_texCoord;
	V3 m_normal, m_vertex;
	Color m_baseColor;
};
struct CFace {
	int m_nIndex = 0, m_bThruster = 0, m_bGlow = 0, m_nVerts = 0, m_nBitmap = 0;
};
struct CSubModel {
	int m_nParent = -1, m_nAngles = 0, m_nBomb = 0, m_nMissile = 0, m_nGun = 0,
	    m_nGunPoint = 0;
	int m_bBullets = 0, m_bThruster = 0, m_bGlow = 0, m_bRender = 0,
	    m_nVertexIndex[1]{}, m_nVertices = 0, m_nFaces = 0;
	V3 m_vOffset, low, high;
	CFace *m_faces = nullptr;
	void InitMinMax()
	{
		low.v.coord = { 1e30f, 1e30f, 1e30f };
		high.v.coord = { -1e30f, -1e30f, -1e30f };
	}
	void SetMinMax(V3 *p)
	{
		low.v.coord.x = std::min(low.v.coord.x, p->v.coord.x);
		low.v.coord.y = std::min(low.v.coord.y, p->v.coord.y);
		low.v.coord.z = std::min(low.v.coord.z, p->v.coord.z);
		high.v.coord.x = std::max(high.v.coord.x, p->v.coord.x);
		high.v.coord.y = std::max(high.v.coord.y, p->v.coord.y);
		high.v.coord.z = std::max(high.v.coord.z, p->v.coord.z);
	}
};
struct CModel {
	int m_iSubModel = 0;
	Array<CSubModel> m_subModels;
	Array<V3> m_vertNorms, m_vertices;
	Array<CVertex> m_faceVerts;
	Array<CFace> m_faces;
	struct Owner {
		uint16_t m_nOwner = 0, m_nVertex = 0;
	};
	Array<Owner> m_vertexOwner;
	void GetOOFModelItems(int32_t, OOF::CModel *, float);
};
} // namespace RenderModel
#define DBG    0
#define F2X(x) (int32_t((x) * 65536.0f))
void OOF_DrawShadow(OOF::CModel *, OOF::CSubModel *)
{
	throw std::runtime_error("Shadow profile not qualified");
}
using namespace RenderModel;
#include "oof_conversion_source.h"
namespace OOF
{
#include "oof_direct_source.h"
}

static bool same(const V3 &a, const xl_compat::OofVector &b)
{
	return a.v.coord.x == b[0] && a.v.coord.y == b[1] && a.v.coord.z == b[2];
}
struct Snapshot {
	std::vector<float> values;
	std::size_t normal_differences = 0;
};
Snapshot compare(const xl_compat::OofModel &decoded, int tick = 0)
{
	Snapshot snapshot;
	OOF::CModel source;
	source.m_nSubModels = decoded.submodels.size();
	source.m_subModels.resize(source.m_nSubModels);
	std::vector<std::vector<std::vector<OOF::CFaceVert>>> corners(
	    source.m_nSubModels);
	std::size_t vertices = 0, faces = 0, faceverts = 0;
	for (const auto &entry : decoded.submodels) {
		const auto &p = entry.second;
		auto &s = source.m_subModels.at(entry.first);
		if (!p.properties.empty() || !p.facing_lists.empty())
			throw std::runtime_error("Outside static profile");
		s.m_nParent = p.parent;
		s.m_nIndex = entry.first;
		s.m_vOffset = V3(p.offset);
		s.m_vMin.v.coord = { 1e30f, 1e30f, 1e30f };
		s.m_vMax.v.coord = { -1e30f, -1e30f, -1e30f };
		for (const auto &v : p.vertices) {
			s.m_vMin.v.coord.x = std::min(s.m_vMin.v.coord.x, v[0]);
			s.m_vMin.v.coord.y = std::min(s.m_vMin.v.coord.y, v[1]);
			s.m_vMin.v.coord.z = std::min(s.m_vMin.v.coord.z, v[2]);
			s.m_vMax.v.coord.x = std::max(s.m_vMax.v.coord.x, v[0]);
			s.m_vMax.v.coord.y = std::max(s.m_vMax.v.coord.y, v[1]);
			s.m_vMax.v.coord.z = std::max(s.m_vMax.v.coord.z, v[2]);
		}
		for (const auto &child : decoded.submodels)
			if (child.second.parent == entry.first && child.first != entry.first) {
				s.m_children.push_back(child.first);
				++s.m_nChildren;
			}
		s.m_nVerts = p.vertices.size();
		s.m_rotVerts.resize(p.vertices.size());
		vertices += p.vertices.size();
		for (auto v : p.vertices)
			s.m_vertices.push_back(V3(v));
		s.m_faces.m_nFaces = p.faces.size();
		s.m_faces.m_list.resize(p.faces.size());
		corners[entry.first].resize(p.faces.size());
		faces += p.faces.size();
		for (std::size_t i = 0; i < p.faces.size(); ++i) {
			const auto &f = p.faces[i];
			auto &d = s.m_faces.m_list[i];
			d.m_nVerts = f.corners.size();
			d.m_bTextured = f.texture >= 0;
			d.m_texProps.nTexId = f.texture;
			d.m_vNormal = V3(f.normal);
			for (int c = 0; c < 3; ++c)
				d.m_texProps.color.c[c] = f.color[c];
			for (auto c : f.corners)
				corners[entry.first][i].push_back({ c.vertex, c.u, c.v });
			d.m_vertices = corners[entry.first][i].data();
			faceverts += f.corners.size();
		}
	}
	gameStates.app.nSDLTicks[0] = tick;
	CObject object;
	float light = 1;
	for (bool transform : { false, true }) {
		ogl.m_states.bUseTransform = transform;
		if (!source.Render(&object, &light, 0)) throw std::runtime_error("Pinned direct render failed");
		snapshot.values.push_back(source.m_fAlpha);
		for (const auto &part : source.m_subModels) {
			for (const auto &v : part.m_rotVerts)
				for (float x : { v.v.coord.x, v.v.coord.y, v.v.coord.z }) snapshot.values.push_back(x);
			for (const auto &f : part.m_faces.m_list)
				for (float x : { f.m_vRotNormal.v.coord.x, f.m_vRotNormal.v.coord.y, f.m_vRotNormal.v.coord.z }) snapshot.values.push_back(x);
		}
	}
	RenderModel::CModel converted;
	converted.m_subModels.resize(source.m_nSubModels);
	converted.m_vertices.resize(vertices);
	converted.m_vertexOwner.resize(vertices);
	converted.m_vertNorms.resize(faceverts);
	converted.m_faceVerts.resize(faceverts);
	converted.m_faces.resize(faces);
	converted.GetOOFModelItems(132, &source, 1.0f);
	float max_normal_component_difference = 0;
	std::size_t significant_normal_differences = 0;
	std::size_t uv_bad = 0;
	std::size_t at = 0, position_bad = 0, normal_bad = 0, alpha_bad = 0,
	            direct_bad = 0;
	for (const auto &entry : decoded.submodels) {
		auto &part = source.m_subModels.at(entry.first);
		V3 offset;
		int parent = entry.first;
		for (std::size_t depth = 0; parent != -1; ++depth) {
			if (depth >= decoded.submodels.size())
				throw std::runtime_error("Bad hierarchy");
			const auto &p = decoded.submodels.at(parent);
			offset += V3(p.offset);
			parent = p.parent == parent ? -1 : p.parent;
		}
		for (bool transformed : { false, true })
			for (auto point : part.m_vertices) {
				ogl.m_states.bUseTransform = transformed;
				V3 actual;
				part.TransformVertex(&actual, &point, &offset);
				const auto expected = point + offset;
				direct_bad += actual.v.coord.x != expected.v.coord.x ||
				              actual.v.coord.y != expected.v.coord.y ||
				              actual.v.coord.z != expected.v.coord.z;
			}
	}
	for (const auto &entry : decoded.submodels)
		for (const auto &f : entry.second.faces)
			for (const auto &c : f.corners) {
				const auto &v = converted.m_faceVerts.at(at++);
				position_bad += !same(v.m_vertex, entry.second.vertices.at(c.vertex));
				uv_bad += v.m_texCoord.v.u != c.u || v.m_texCoord.v.v != c.v;
				for (float value : { v.m_vertex.v.coord.x, v.m_vertex.v.coord.y, v.m_vertex.v.coord.z, v.m_normal.v.coord.x, v.m_normal.v.coord.y, v.m_normal.v.coord.z, v.m_texCoord.v.u, v.m_texCoord.v.v, v.m_baseColor.c[3] }) snapshot.values.push_back(value);
				normal_bad += !same(v.m_normal, entry.second.normals.at(c.vertex));
				const auto &normal = entry.second.normals.at(c.vertex);
				const float distance =
				    std::max({ std::fabs(v.m_normal.v.coord.x - normal[0]),
				               std::fabs(v.m_normal.v.coord.y - normal[1]),
				               std::fabs(v.m_normal.v.coord.z - normal[2]) });
				max_normal_component_difference =
				    std::max(max_normal_component_difference, distance);
				significant_normal_differences += distance > 0.00001f;
				alpha_bad += v.m_baseColor.c[3] != entry.second.alpha.at(c.vertex);
			}
	std::cout << "corners=" << at << " positions_differ=" << position_bad
	          << " normals_differ=" << normal_bad
	          << " normals_differ_over_1e-5=" << significant_normal_differences
	          << " max_component_difference=" << max_normal_component_difference
	          << " alpha_differ=" << alpha_bad
	          << " direct_positions_differ=" << direct_bad << "\n";

	if (position_bad || alpha_bad || direct_bad || uv_bad) throw std::runtime_error("OOF geometry or UV reference differs");
	source.m_vMin.v.coord = { 1e30f, 1e30f, 1e30f };
	source.m_vMax.v.coord = { -1e30f, -1e30f, -1e30f };
	for (auto &part : source.m_subModels)
		if (part.m_nParent == -1) source.GetSubModelBounds(&part, V3{});
	for (float value : { source.m_vMin.v.coord.x, source.m_vMin.v.coord.y, source.m_vMin.v.coord.z, source.m_vMax.v.coord.x, source.m_vMax.v.coord.y, source.m_vMax.v.coord.z }) snapshot.values.push_back(value);
	for (const auto &entry : decoded.submodels) {
		snapshot.values.push_back(float(entry.second.parent));
		for (auto v : entry.second.offset) snapshot.values.push_back(v);
	}
	const auto mesh = xl_compat::compile_oof_mesh(decoded, false);
	if (!same(source.m_vMin, mesh.low) || !same(source.m_vMax, mesh.high)) throw std::runtime_error("Redux bounds differ from pinned hierarchy bounds");
	std::size_t face_index = 0;
	for (const auto &entry : decoded.submodels) {
		const auto &part = entry.second;
		if (converted.m_subModels.at(entry.first).m_nParent != part.parent) throw std::runtime_error("Converted hierarchy differs");
		for (const auto &face : part.faces) {
			const auto &output = mesh.faces.at(face_index++);
			std::size_t triangle = 0;
			for (std::size_t corner = 1; corner + 1 < face.corners.size(); ++corner)
				for (std::size_t index : { std::size_t(0), corner, corner + 1 }) {
					const auto &authored = face.corners[index];
					const auto &v = output.triangles.at(triangle++);
					auto expected = part.vertices.at(authored.vertex);
					for (int axis = 0; axis < 3; ++axis) expected[axis] += mesh.offsets.at(entry.first)[axis];
					if (v.position != expected || v.uv != std::array<float, 2>{ authored.u, authored.v } || v.normal != part.normals.at(authored.vertex) || v.color[3] != part.alpha.at(authored.vertex)) throw std::runtime_error("Metadata changed existing Redux geometry, UV, normal or alpha output");
				}
		}
	}
	snapshot.normal_differences = normal_bad;
	return snapshot;
}
int main(int argc, char **argv)
{
	try {
		std::vector<std::vector<uint8_t>> inputs;
		const bool require_normal_parity = argc > 1 && std::string(argv[1]) == "--require-normal-parity";
		const int first_input = require_normal_parity ? 2 : 1;
		if (argc == first_input) inputs.push_back(xl_oof_test::metadata_fixture());
		for (int i = first_input; i < argc; ++i) {
			std::ifstream f(argv[i], std::ios::binary);
			if (!f) throw std::runtime_error("Missing model");
			inputs.emplace_back(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
		}
		for (const auto &bytes : inputs) {
			const auto decoded = xl_compat::read_oof(bytes);
			const auto baseline = compare(decoded);
			if (require_normal_parity && baseline.normal_differences) throw std::runtime_error("Existing reference/Redux normal mismatch remains unaddressed");
			for (int variant : { 0, 1 })
				for (int tick : { 0, 1, 17, 1000 }) {
					auto changed = bytes;
					std::size_t at = 8;
					while (at < changed.size()) {
						std::uint32_t size = 0;
						for (int j = 0; j < 4; ++j) size |= std::uint32_t(changed.at(at + 4 + j)) << (8 * j);
						if (std::string(reinterpret_cast<const char *>(changed.data() + at), 4) == "RANI") {
							const auto payload = xl_oof_test::rotation_tracks(decoded.submodels.size(), variant);
							std::vector<uint8_t> replacement;
							xl_oof_test::chunk(replacement, "RANI", payload);
							changed.erase(changed.begin() + at, changed.begin() + at + 8 + size);
							changed.insert(changed.begin() + at, replacement.begin(), replacement.end());
							break;
						}
						at += 8 + size;
					}
					const auto actual = compare(xl_compat::read_oof(changed), tick);
					if (actual.values != baseline.values || actual.normal_differences != baseline.normal_differences) throw std::runtime_error("Authored RANI changed pinned static output or normal limitation");
				}
		}
		std::cout << "Pinned static geometry/UV/bounds/hierarchy invariance passed; normal parity is not claimed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
