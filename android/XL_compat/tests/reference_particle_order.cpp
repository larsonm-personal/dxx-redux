// Execute actual transparency insertion and bucket traversal with owned storage
#include "transparency_order.h"
#include "transparency.h"
#include <cstring>
#include <deque>
#include <iostream>
template <class T>
void Swap(T &a, T &b)
{
	std::swap(a, b);
}
namespace energy_reference
{
#include "xl_reference_transparency_weapons.h"
}
namespace particle_order_reference
{
using fix = std::int32_t;
struct {
	struct {
		int nMathFormat = 2;
	} render;
	int SoftBlend(int) const
	{
		return 0;
	}
} options;
auto *gameOpts = &options;
struct tQuadInt {
	unsigned low;
	int high;
};
void FixMulAccum(tQuadInt *, fix, fix)
{
	throw std::runtime_error("Unexpected integer render distance profile");
}
fix QuadSqrt(unsigned, int)
{
	throw std::runtime_error("Unexpected integer render distance profile");
}
struct CFixVector {
	struct {
		struct {
			int x, y, z;
		} coord;
	} v{};
	CFixVector operator-(const CFixVector &b) const
	{
		return { { { v.coord.x - b.v.coord.x, v.coord.y - b.v.coord.y, v.coord.z - b.v.coord.z } } };
	}
	fix Mag() const;
	static const fix Dist(const CFixVector &, const CFixVector &);
	void Assign(const struct CFloatVector &);
};
struct CFloatVector {
	std::array<float, 4> values{};
	void Assign(const CFixVector &v)
	{
		values = { v.v.coord.x / 65536.f, v.v.coord.y / 65536.f, v.v.coord.z / 65536.f, 0 };
	}
	float &Alpha()
	{
		return values[3];
	}
	CFloatVector &operator+=(const CFloatVector &b)
	{
		for (unsigned i = 0; i < 4; ++i) values[i] += b.values[i];
		return *this;
	}
	CFloatVector &operator/=(int n)
	{
		for (auto &v : values) v /= float(n);
		return *this;
	}
};
void CFixVector::Assign(const CFloatVector &v)
{
	this->v.coord = { int(v.values[0] * 65536), int(v.values[1] * 65536), int(v.values[2] * 65536) };
}
struct {
	struct {
		struct {
			struct {
				CFixVector vPos;
			} viewer;
		} mine;
	} renderData;
	struct {
		CFixVector vScale;
	} modelData;
} gameData;
struct {
	struct {
		int nThreads = 1;
	} app;
	struct {
		int nShadowMap = 0;
		float grAlpha = 1;
		struct {
			bool bActive = false;
		} cameras;
	} render;
} gameStates;
struct {
	bool StereoDevice() const
	{
		return false;
	}
	int StereoSeparation() const
	{
		return 0;
	}
} ogl;
struct {
	void Transform(CFixVector &out, const CFixVector &input)
	{
		out = input;
	}
} transformation;
unsigned current_item = 0;
struct CObject {
	struct {
		int nType = 1;
		struct {
			CFixVector vPos;
		} position;
	} info;
	bool Cloaked() const
	{
		return false;
	}
};
struct CParticle {
	int m_nType = 5;
	CFixVector m_vPos;
};
struct CBitmap {
};
struct CSegFace {
	struct {
		int nSegment = 0, nSide = 0;
	} m_info;
};
struct tFaceTriangle {
};
struct tTexCoord2f {
	float u, v;
};
using CFaceColor = CFloatVector;
struct Segment {
	CFixVector center;
	Segment *Side(int)
	{
		return this;
	}
	CFixVector Center() const
	{
		return center;
	}
} segment;
tTexCoord2f tcDefault[4]{};
struct CTranspItem {
	CTranspItem *nextItemP = nullptr;
	int nItem = 0, bRendered = 0, bTransformed = 0, z = 0, bValid = 0;
	unsigned id = current_item;
	int Type() const
	{
		return 0;
	}
	std::size_t Size() const
	{
		return sizeof(*this);
	}
};
struct CTranspObject : CTranspItem {
	CObject *pObj = nullptr;
	CFixVector vScale;
};
struct CTranspParticle : CTranspItem {
	CParticle *particle = nullptr;
	float fBrightness = 0;
};
struct CLightning {
	CFixVector m_vPos, m_vEnd;
};
struct CTranspLightning : CTranspItem {
	CLightning *lightning = nullptr;
	int16_t nDepth = 0;
};
struct CTranspPoly : CTranspItem {
	CSegFace *pFace = nullptr;
	tFaceTriangle *pTriangle = nullptr;
	CBitmap *pBm = nullptr;
	int nVertices = 0, nPrimitive = 0, nWrap = 0, bDepthMask = 0, bAdditive = 0, nSegment = 0, nColors = 0;
	tTexCoord2f texCoord[4];
	CFloatVector color[4], vertices[4];
};
struct CTranspSprite : CTranspItem {
	CBitmap *pBm = nullptr;
	int bColor = 0, nWidth = 0, nHeight = 0, nFrame = 0, bAdditive = 0;
	float fSoftRad = 0;
	CFloatVector color, position;
};
struct DepthBuffer {
	std::vector<CTranspItem *> values;
	std::size_t Length() const
	{
		return values.size();
	}
	CTranspItem **operator+(int i)
	{
		return values.data() + i;
	}
	CTranspItem *&operator[](int i)
	{
		return values[std::size_t(i)];
	}
};
struct CTranspItemBuffers {
	DepthBuffer depthBuffer;
	std::deque<CTranspItem> storage;
	int nItems[2]{}, nMinOffs = 250000, nMaxOffs = 0;
	CTranspItem **pBuffer = nullptr, *freeList[1]{};
	CTranspItem *AllocItem(int, std::size_t)
	{
		storage.emplace_back();
		return &storage.back();
	}
};
#define ENTER(a, b)
#define RETVAL(a)            return (a);
#define RETURN               return;
#define LAZY_RESET           1
#define RENDER_TRANSPARENCY  1
#define TRANSP_DEPTH_HASH    1
#define USE_OPENMP           0
#define DBG                  0
#define I2X(a)               ((a) * 65536)
#define OBJPOS(p)            (&(p)->info.position)
#define SOFT_BLEND_SPRITES   1
#define SOFT_BLEND_PARTICLES 2
#define TI_SPLIT_POLYS       0
#define SEGMENT(i)           (&segment)
struct CTransparencyRenderer {
	struct {
		bool bReady = true;
		int bAllowAdd = 1, zMin = 0, zMax = 0, bRenderGlow = 0, bSoftBlend = 0;
		double zScale = 1;
		CFixVector vViewer[2];
		CFloatVector vViewerf[2];
		CTranspItemBuffers buffers[1];
	} m_data;
	std::vector<std::size_t> drawn;
	bool Ready() const
	{
		return m_data.bReady;
	}
	void InitBuffer(int32_t, int32_t, int32_t);
	int32_t Depth(CFixVector, int32_t);
	int32_t Add(CTranspItem *, CFixVector, int32_t = 0, bool = false, int32_t = -1);
	int32_t AddObject(CObject *);
	int32_t AddPoly(CSegFace *, tFaceTriangle *, CBitmap *, CFloatVector *, char, tTexCoord2f *, CFloatVector *, CFaceColor *, char, char, int32_t, int32_t, int32_t, int16_t);
	int32_t AddSprite(CBitmap *, const CFixVector &, CFloatVector *, int32_t, int32_t, char, char, float);
	int32_t AddParticle(CParticle *, float, int32_t);
	int32_t AddLightning(CLightning *, int16_t);
	void RenderBuffer(CTranspItemBuffers &, bool);
	void RenderItem(CTranspItem *item)
	{
		drawn.push_back(item->id);
	}
};
#include "xl_reference_particle_order.h"
} // namespace particle_order_reference

void particle_order_source_controls()
{
	using namespace particle_order_reference;
	energy_reference::WeaponTable categories;
	for (int id = 0; id < 256; ++id)
		if (categories.IsEnergyProjectile(int16_t(id)) != xl_compat::transparency_energy_projectile(unsigned(id)))
			throw std::runtime_error("Source energy projectile queue classification differs at " + std::to_string(id));
	std::cout << "Pinned transparency weapons: 256 actual category table/predicate comparisons passed\n";
	unsigned cases = 0, items = 0;
	std::uint32_t random = 113;
	for (int mixed : { 0, 1 })
		for (int maximum : { 0, 1000, 655360, 400 * 65536 })
			for (const xl_compat::ParticlePoint viewer : { xl_compat::ParticlePoint{}, xl_compat::ParticlePoint{ -113777, 991001, -37555 } }) {
				std::vector<xl_compat::ParticlePoint> positions;
				for (int offset : { 0, 1, 1000, 655359, 655360, 655361, 1310720, maximum, maximum + 655360, maximum + 655361 }) {
					positions.push_back({ viewer[0] + offset, viewer[1], viewer[2] });
					positions.push_back(positions.back());
				}
				for (unsigned i = 0; i < 1000; ++i) {
					xl_compat::ParticlePoint at;
					for (unsigned axis = 0; axis < 3; ++axis) {
						random = random * 1664525u + 1013904223u;
						at[axis] = viewer[axis] + int(random % (400 * 65536u)) - 200 * 65536;
					}
					positions.push_back(at);
				}
				CTransparencyRenderer source;
				std::vector<xl_compat::TransparencyPosition> entries;
				gameData.renderData.mine.viewer.vPos.v.coord = { viewer[0], viewer[1], viewer[2] };
				source.InitBuffer(0, maximum, 0);
				auto &buffer = source.m_data.buffers[0];
				buffer.depthBuffer.values.resize(250000, nullptr);
				for (unsigned i = 0; i < positions.size(); ++i) {
					current_item = i;
					const auto &p = positions[i];
					const int bias = mixed && i % 4 ? 0 : 10;
					entries.push_back({ p, bias * 65536, mixed && i % 4 == 3 });
					const CFixVector position{ { { p[0], p[1], p[2] } } };
					int accepted;
					if (!mixed || i % 4 == 0) {
						CParticle particle{ 5, position };
						accepted = source.AddParticle(&particle, 1, 0);
					} else if (i % 4 == 1) {
						CObject object;
						object.info.position.vPos = position;
						accepted = source.AddObject(&object);
					} else if (i % 4 == 2) accepted = source.AddSprite(nullptr, position, nullptr, 1, 1, 0, 0, 0);
					else {
						CSegFace face;
						CFloatVector vertices[4];
						segment.center = position;
						accepted = source.AddPoly(&face, nullptr, nullptr, vertices, (i & 4) ? 3 : 4, nullptr, nullptr, nullptr, 0, 0, 0, 0, 0, 0);
					}
					if (!accepted) throw std::runtime_error("Source render order unexpectedly exhausted storage");
				}
				for (int bucket = buffer.nMaxOffs; bucket >= buffer.nMinOffs; --bucket)
					if (buffer.depthBuffer[bucket]) {
						buffer.pBuffer = buffer.depthBuffer + bucket;
						source.RenderBuffer(buffer, true);
					}
				const auto actual = xl_compat::transparency_draw_order(entries, viewer, maximum);
				if (actual != source.drawn || buffer.nItems[0]) throw std::runtime_error("Particle distance bucket order differs from source");
				items += unsigned(actual.size());
				++cases;
			}
	std::cout << "Pinned transparency order: " << cases << " particle/mixed viewer/depth-limit profiles, " << items << " ordered entries, per-kind distance bias, clipping and reversed equal-bucket insertion passed\n";
	unsigned bolts = 0, ordered = 0;
	for (int maximum : { 0, 1000, 655360, 400 * 65536 })
		for (const xl_compat::ParticlePoint viewer : { xl_compat::ParticlePoint{}, xl_compat::ParticlePoint{ -113777, 991001, -37555 } }) {
			CTransparencyRenderer source;
			gameData.renderData.mine.viewer.vPos.v.coord = { viewer[0], viewer[1], viewer[2] };
			source.InitBuffer(0, maximum, 0);
			auto &buffer = source.m_data.buffers[0];
			buffer.depthBuffer.values.resize(250000, nullptr);
			std::vector<xl_compat::TransparencyPosition> entries;
			const std::array<int, 8> offsets{ 0, 1, 999, 1000, 655360, maximum, maximum + 1, maximum + 655360 };
			for (unsigned i = 0; i < 576; ++i) {
				xl_compat::ParticlePoint start = viewer, end = viewer;
				if (i < 64) {
					start[0] += offsets[i / 8];
					end[1] -= offsets[i % 8];
				} else {
					for (unsigned axis = 0; axis < 3; ++axis) {
						random = random * 1664525u + 1013904223u;
						start[axis] += int(random % (400 * 65536u)) - 200 * 65536;
						random = random * 1664525u + 1013904223u;
						end[axis] += int(random % (400 * 65536u)) - 200 * 65536;
					}
					if (i % 4 == 0) start = viewer;
					if (i % 4 == 1) end = start;
				}
				CLightning lightning{ { { { start[0], start[1], start[2] } } }, { { { end[0], end[1], end[2] } } } };
				current_item = unsigned(entries.size());
				const bool accepted = source.AddLightning(&lightning, 0) != 0;
				const auto actual = xl_compat::lightning_transparency_position(start, end, viewer, maximum);
				if (accepted != actual.has_value()) throw std::runtime_error("Lightning endpoint depth admission differs from source");
				if (actual) entries.push_back(*actual);
				if (i % 3 == 0) {
					current_item = unsigned(entries.size());
					CParticle particle{ 5, lightning.m_vPos };
					if (!source.AddParticle(&particle, 1, 0)) throw std::runtime_error("Mixed lightning particle storage failed");
					entries.push_back({ start, 10 * 65536, false });
				}
				++bolts;
			}
			for (int bucket = buffer.nMaxOffs; bucket >= buffer.nMinOffs; --bucket)
				if (buffer.depthBuffer[bucket]) {
					buffer.pBuffer = buffer.depthBuffer + bucket;
					source.RenderBuffer(buffer, true);
				}
			const auto actual = xl_compat::transparency_draw_order(entries, viewer, maximum);
			if (actual != source.drawn || buffer.nItems[0]) throw std::runtime_error("Lightning/particle bucket order differs from source");
			ordered += unsigned(actual.size());
		}
	std::cout << "Pinned lightning order: " << bolts << " endpoint/depth admissions and " << ordered << " mixed lightning/particle entries passed\n";
}
