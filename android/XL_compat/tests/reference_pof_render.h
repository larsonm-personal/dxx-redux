#ifndef DXX_XL_REFERENCE_POF_RENDER_H
#define DXX_XL_REFERENCE_POF_RENDER_H
namespace pof_reference
{
template <typename T>
struct CArray {
	std::vector<T> values;
	bool Create(std::size_t count, const char *)
	{
		values.resize(count);
		return true;
	}
	void Destroy()
	{
		values.clear();
	}
	void Clear()
	{
		std::fill(values.begin(), values.end(), T{});
	}
	T *Buffer()
	{
		return values.data();
	}
	std::size_t Length() const
	{
		return values.size();
	}
	operator T *()
	{
		return Buffer();
	}
	template <typename I>
	T *operator+(I i)
	{
		return Buffer() + i;
	}
	template <typename I>
	T &operator[](I i)
	{
		return values.at(i);
	}
};
struct CFloatVector3 {
	struct {
		struct {
			float x, y, z;
		} coord;
	} v;
	static CFloatVector3 Create(float x, float y, float z)
	{
		return { { { x, y, z } } };
	}
	CFloatVector3 &Assign(const CFixVector &);
	void Set(float x, float y, float z)
	{
		*this = Create(x, y, z);
	}
	const float SqrMag() const;
	const float Mag() const;
	static const float Dist(const CFloatVector3 &, const CFloatVector3 &);
	// The ASE bounds oracle does not inspect the independently computed face normal
	static CFloatVector3 Normal(const CFloatVector3 &, const CFloatVector3 &, const CFloatVector3 &)
	{
		return Create(0, 0, 0);
	}
	const CFloatVector3 &operator+=(const CFloatVector3 &);
	const CFloatVector3 operator+(const CFloatVector3 &) const;
	const CFloatVector3 operator*(float) const;
	operator ::CFloatVector() const
	{
		::CFloatVector result{};
		result.v.coord.x = v.coord.x;
		result.v.coord.y = v.coord.y;
		result.v.coord.z = v.coord.z;
		return result;
	}
	const CFloatVector3 operator-(const CFloatVector3 &) const;
};
struct CFloatVector {
	float channels[4]{};
	float &Red()
	{
		return channels[0];
	}
	float &Green()
	{
		return channels[1];
	}
	float &Blue()
	{
		return channels[2];
	}
	float &Alpha()
	{
		return channels[3];
	}
};
struct PaletteAdapter {
	void ToRgbaf(int, CFloatVector &)
	{
		throw std::runtime_error("Unexpected palette consumer");
	}
};
struct CBitmap {
	bool Flat() const
	{
		return false;
	}
	CBitmap *HasOverride()
	{
		return nullptr;
	}
	void GetAvgColor(CFloatVector *)
	{
		throw std::runtime_error("Unexpected color consumer");
	}
	void *Buffer()
	{
		return nullptr;
	}
	PaletteAdapter *Palette()
	{
		return nullptr;
	}
	int AvgColorIndex()
	{
		return 0;
	}
};
struct tModelSphere {
	int nSubModels = 0, nFaces = 0, nFaceVerts = 0;
	CFixVector vOffsets[2]{};
	fix xRads[2]{};
};
template <typename T>
struct CQuickSort {
	void SortAscending(T *data, int first, int last, int32_t (*compare)(const T *, const T *))
	{
		std::sort(data + first, data + last + 1, [&](const T &a, const T &b) { return compare(&a, &b) < 0; });
	}
};
struct {
	struct {
		struct {
			CBitmap bitmaps[1][4096];
		} tex;
	} pigData;
	struct {
		CModelHitboxList hitboxes[2];
		CFixVector offsets[2];
		tModelSphere spheres[2];
	} modelData;
} gameData;
struct {
	struct {
		float grAlpha = 1;
	} render;
} gameStates;
struct {
	PaletteAdapter *Game()
	{
		return nullptr;
	}
} paletteManager;
struct tUVL {
	int32_t u, v, l;
};
struct CAngleVector {
};
struct CVertex {
	CFloatVector3 m_vertex{}, m_normal{};
	struct {
		struct {
			float u, v;
		} v;
	} m_texCoord{};
	CFloatVector m_renderColor, m_baseColor;
	bool m_bTextured = false;
	uint16_t m_nIndex = 0;
};
struct CFace {
	int m_nBitmap = 0, m_nSubModel = 0, m_nIndex = 0, m_nVerts = 0, m_nId = 0;
	bool m_bThruster = false, m_bGlow = false;
	CFixVector m_vNormal{};
};
struct CModel;
struct CSubModel {
	CFace *m_faces = nullptr;
	int m_nFaces = 0, m_nParent = -1, m_nBomb = -1, m_nMissile = -1, m_nGun = -1, m_nGunPoint = -1, m_nVertices = 0, m_nAngles = 0;
	bool m_bBullets = false, m_bThruster = false, m_bGlow = false, m_bRender = true;
	uint16_t m_nVertexIndex[2]{};
	CFixVector m_vOffset{};
	CFloatVector3 m_vMin{}, m_vMax{};
	int m_nHitbox = 0, m_nSubModel = 0;
	bool m_bWeapon = false;
	bool m_bFlare = false, m_bBillboard = false, m_bHeadlight = false, m_bBombMount = false;
	int m_nType = 0, m_nWeaponPos = 0, m_iFrame = 0, m_tFrame = 0, m_nFrames = 0;
	CFixVector m_vCenter{};
	fix m_nRad = 0;
	void Size(CModel *, CObject *, CFixVector *);
	void InitMinMax();
	void SetMinMax(CFloatVector3 *);
};
namespace ASE
{
struct CModel;
}
struct CModel {
	void GetASEModelItems(int32_t, ASE::CModel *, float);
	int m_nModel = 0;
	int m_nType = 0;
	bool m_bBullets = false;
	CFixVector m_vBullets{};
	int32_t Shift(CObject *, int32_t, CFloatVector3 *);
	fix Size(CObject *, int32_t);
	fix Radius(CObject *);
	static int32_t CmpVerts(const CFloatVector3 *, const CFloatVector3 *);
	uint16_t FilterVertices(CArray<CFloatVector3> &, uint16_t);
	CArray<CSubModel> m_subModels;
	CArray<CFace> m_faces;
	CArray<CVertex> m_faceVerts;
	CArray<CFloatVector3> m_vertices, m_vertNorms;
	struct Owner {
		uint16_t m_nOwner, m_nVertex;
	};
	CArray<Owner> m_vertexOwner;
	int m_iFace = 0, m_iFaceVert = 0, m_iSubModel = 0, nGlow = -1;
	int m_nFaces = 0, m_nFaceVerts = 0, m_nSubModels = 0;
	CFace *AddPOFFace(CSubModel *, CFace *, CFixVector *, uint8_t *, CArray<CBitmap *> &, CFloatVector *, bool = true);
	int32_t GetPOFModelItems(void *, CAngleVector *, int32_t, int32_t, int32_t, CArray<CBitmap *> &, CFloatVector *);
};
#define Assert(condition)                                                           \
	do {                                                                            \
		if (!(condition)) throw std::runtime_error("Source POF emitter assertion"); \
	} while (0)
#define VECPTR(p)       reinterpret_cast<CFixVector *>(p)
#define WORDPTR(p)      reinterpret_cast<uint16_t *>(p)
#define PAL2RGBA(value) (value)
static constexpr bool IsMultiGame = false;
#include "xl_reference_pof_render.h"
#undef Assert
#undef VECPTR
#undef WORDPTR
#undef PAL2RGBA
inline void owned_bounds_controls()
{
	unsigned cases = 0;
	for (int count : { 1, 3, 5 })
		for (int filter : { 0, 1, 2, 3, 4 })
			for (int hierarchy : { 0, 1, 2 }) {
				CModel source;
				source.m_nSubModels = count;
				source.m_subModels.values.resize(count);
				std::vector<xl_compat::RenderModelPart> parts(count);
				for (int i = 0; i < count; ++i) {
					auto &native = parts[i];
					auto &reference = source.m_subModels[i];
					native.parent = hierarchy == 0 || i == 0 ? -1 : hierarchy == 1 ? i - 1
					                                                               : 0;
					native.offset = { i * 2345 - 351, -i * 127 + 33, i * 3457 - 29 };
					native.low = { -1.12345f - i, -2.34567f + i, -3.45678f - i };
					native.high = { 4.23456f + i, 5.34567f + i, 6.45678f + i };
					native.faces = 1;
					native.excluded_from_hitboxes = filter && (i == count - 1);
					reference.m_nSubModel = i;
					reference.m_nParent = native.parent;
					reference.m_vOffset = CFixVector::Create(native.offset[0], native.offset[1], native.offset[2]);
					reference.m_vMin = CFloatVector3::Create(native.low[0], native.low[1], native.low[2]);
					reference.m_vMax = CFloatVector3::Create(native.high[0], native.high[1], native.high[2]);
					if (native.excluded_from_hitboxes) {
						reference.m_bThruster = filter == 1;
						reference.m_bGlow = filter == 2;
						reference.m_bWeapon = filter == 3;
						reference.m_nGunPoint = filter == 4 ? 0 : -1;
					}
				}
				// Prime only the independent radius cache; Size still executes its full bounds path
				gameData.modelData.spheres[0] = {};
				gameData.modelData.spheres[0].nSubModels = count;
				source.Size(nullptr, 1);
				const auto actual = xl_compat::owned_model_hitboxes(parts);
				const auto &expected = gameData.modelData.hitboxes[0];
				if (actual.size() != std::size_t(expected.nHitboxes + 1)) throw std::runtime_error("Owned model hitbox count differs from source Size");
				const auto point = [](const CFixVector &p) { return std::array<int32_t, 3>{ p.v.coord.x, p.v.coord.y, p.v.coord.z }; };
				for (std::size_t i = 0; i < actual.size(); ++i) {
					const auto &box = expected.hitboxes[i];
					if (actual[i].low != point(box.vMin) || actual[i].high != point(box.vMax) || actual[i].offset != point(box.vOffset) || actual[i].half_size != point(box.vSize))
						throw std::runtime_error("Owned model hitbox differs from pinned high-resolution Size");
				}
				++cases;
			}
	std::cout << "Owned model hitboxes: " << cases << " pinned high-resolution sizing controls passed\n";
}
inline void compare(const std::vector<uint8_t> &bytes, const xl_compat::ModelRenderGeometry &native)
{
	CModel source;
	source.m_nSubModels = int(native.parts.size());
	source.m_nFaces = int(native.face_count);
	source.m_nFaceVerts = int(native.face_vertex_count);
	source.m_subModels.values.resize(native.parts.size());
	for (std::size_t i = 0; i < native.parts.size(); ++i) source.m_subModels[i].m_nSubModel = int(i);
	source.m_faces.values.resize(native.face_count + 1);
	source.m_faceVerts.values.resize(native.face_vertex_count + 1);
	source.m_vertNorms.values.resize(native.face_vertex_count + 1);
	source.m_vertices.values.resize(native.vertex_count());
	source.m_vertexOwner.values.resize(native.vertex_count());
	CArray<CBitmap *> bitmaps;
	for (auto &bitmap : gameData.pigData.tex.bitmaps[0]) bitmaps.values.push_back(&bitmap);
	if (!source.GetPOFModelItems(const_cast<uint8_t *>(bytes.data()), nullptr, 0, -1, 1, bitmaps, nullptr)) throw std::runtime_error("Source POF emitter failed");
	for (std::size_t part = 0; part < native.parts.size(); ++part) {
		const auto &actual = native.parts[part];
		const auto &expected = source.m_subModels[part];
		if (actual.parent != expected.m_nParent || actual.faces != unsigned(expected.m_nFaces) || actual.defined_vertices != unsigned(expected.m_nVertices) || actual.offset != std::array<int32_t, 3>{ expected.m_vOffset.v.coord.x, expected.m_vOffset.v.coord.y, expected.m_vOffset.v.coord.z }) throw std::runtime_error("Source POF part metadata differs");
		std::size_t index = 0;
		for (const auto &face : source.m_faces.values) {
			if (face.m_nSubModel != int(part)) continue;
			for (int i = 0; i < face.m_nVerts; ++i) {
				const auto &point = source.m_faceVerts[face.m_nIndex + i].m_vertex.v.coord;
				if (index >= actual.face_vertices.size() || actual.face_vertices[index++] != std::array<float, 3>{ point.x, point.y, point.z }) throw std::runtime_error("Source POF emitted vertex differs");
			}
		}
		if (index != actual.face_vertices.size()) throw std::runtime_error("Source POF emitted vertex count differs");
		if (actual.low != std::array<float, 3>{ expected.m_vMin.v.coord.x, expected.m_vMin.v.coord.y, expected.m_vMin.v.coord.z } || actual.high != std::array<float, 3>{ expected.m_vMax.v.coord.x, expected.m_vMax.v.coord.y, expected.m_vMax.v.coord.z }) throw std::runtime_error("Source POF face-used bounds differ");
	}
	if (std::all_of(native.parts.begin(), native.parts.end(), [](const auto &part) { return part.faces != 0; })) {
		// Radius sorts all positions afterward, so grouping faces by part here
		// preserves its input multiset without qualifying rendering face order
		std::stable_sort(source.m_faces.values.begin(), source.m_faces.values.begin() + source.m_nFaces, [](const auto &a, const auto &b) { return a.m_nSubModel < b.m_nSubModel; });
		for (auto &part : source.m_subModels.values) part.m_faces = nullptr;
		for (int i = 0; i < source.m_nFaces; ++i) {
			auto &part = source.m_subModels[source.m_faces[i].m_nSubModel];
			if (!part.m_faces) part.m_faces = &source.m_faces[i];
		}
		gameData.modelData.spheres[0] = {};
		const auto source_radius = source.Size(nullptr, 0);
		xl_compat::RenderModelSphereCache cache;
		const auto sphere = native.bounding_sphere(&cache, 0);
		const auto &center = gameData.modelData.offsets[0].v.coord;
		if (sphere.radius != source_radius || sphere.center != std::array<int32_t, 3>{ center.x, center.y, center.z }) throw std::runtime_error("Source POF sphere differs");
		auto changed = native;
		changed.parts[0].face_vertices[0][0] += 123.0f;
		const auto saved_vertex = source.m_faceVerts[0].m_vertex;
		source.m_faceVerts[0].m_vertex.v.coord.x += 123.0f;
		const auto cached = changed.bounding_sphere(&cache, 0);
		if (source.Radius(nullptr) != cached.radius || cached.center != sphere.center) throw std::runtime_error("Source POF sphere count-cache reuse differs");
		source.m_nType = 1;
		const auto alternate = native.bounding_sphere(&cache, 1);
		const auto alternate_radius = source.Radius(nullptr);
		if (alternate.radius != alternate_radius || alternate.center != std::array<int32_t, 3>{ center.x, center.y, center.z }) throw std::runtime_error("Source POF sphere alternate cache slot differs");
		source.m_nType = 0;
		source.m_faceVerts[0].m_vertex = saved_vertex;
		std::cout << "Source POF sphere: radius " << sphere.radius << ", center " << sphere.center[0] << ',' << sphere.center[1] << ',' << sphere.center[2] << " matched\n";
		const auto bounds = native.hitbox_bounds();
		const auto point = [](const CFixVector &p) { return std::array<int32_t, 3>{ p.v.coord.x, p.v.coord.y, p.v.coord.z }; };
		if (bounds.size() != std::size_t(gameData.modelData.hitboxes[0].nHitboxes + 1)) throw std::runtime_error("Source POF sized hitbox count differs");
		for (std::size_t i = 0; i < bounds.size(); ++i) {
			const auto &box = gameData.modelData.hitboxes[0].hitboxes[i];
			if (bounds[i].low != point(box.vMin) || bounds[i].high != point(box.vMax) || bounds[i].offset != point(box.vOffset) || bounds[i].half_size != point(box.vSize)) throw std::runtime_error("Source POF sized hitbox bounds differ");
			const auto constructed = xl_compat::model_hitbox(bounds[i]);
			for (unsigned vertex = 0; vertex < 8; ++vertex)
				if (constructed.vertices[vertex] != point(box.box.vertices[vertex])) throw std::runtime_error("Source POF sized hitbox vertex differs");
			for (unsigned face = 0; face < 6; ++face)
				if (constructed.normals[face] != point(box.box.faces[face].n[0])) throw std::runtime_error("Source POF sized hitbox normal differs");
		}
		std::cout << "Source POF sized hitboxes: " << bounds.size() << " matched\n";
	} else {
		bool rejected = false;
		try {
			native.hitbox_bounds();
		} catch (const std::exception &) {
			rejected = true;
		}
		if (!rejected) throw std::runtime_error("Face-empty POF part accepted for hitbox sizing");
	}
}
} // namespace pof_reference
#endif
