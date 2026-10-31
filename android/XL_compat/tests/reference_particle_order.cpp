// Execute actual transparency insertion and bucket traversal with owned storage
#include "particle_order.h"
#include <cstring>
#include <deque>
#include <iostream>
namespace particle_order_reference
{
using fix = std::int32_t;
struct {
	struct {
		int nMathFormat = 2;
	} render;
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
};
struct CFloatVector {
	void Assign(const CFixVector &) {}
};
struct {
	struct {
		struct {
			struct {
				CFixVector vPos;
			} viewer;
		} mine;
	} renderData;
} gameData;
struct {
	struct {
		int nThreads = 1;
	} app;
	struct {
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
struct CTranspItem {
	CTranspItem *nextItemP = nullptr;
	int nItem = 0, bRendered = 0, bTransformed = 0, z = 0, bValid = 0;
	unsigned id = 0;
	int Type() const
	{
		return 0;
	}
	std::size_t Size() const
	{
		return sizeof(*this);
	}
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
#define RETVAL(a)           return (a);
#define RETURN              return;
#define LAZY_RESET          1
#define RENDER_TRANSPARENCY 1
#define TRANSP_DEPTH_HASH   1
#define USE_OPENMP          0
#define DBG                 0
#define I2X(a)              ((a) * 65536)
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
	int32_t Add(CTranspItem *, CFixVector, int32_t, bool, int32_t);
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
	unsigned cases = 0, items = 0;
	std::uint32_t random = 113;
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
			gameData.renderData.mine.viewer.vPos.v.coord = { viewer[0], viewer[1], viewer[2] };
			source.InitBuffer(0, maximum, 0);
			auto &buffer = source.m_data.buffers[0];
			buffer.depthBuffer.values.resize(250000, nullptr);
			for (unsigned i = 0; i < positions.size(); ++i) {
				CTranspItem item;
				item.id = i;
				const auto &p = positions[i];
				if (!source.Add(&item, { { { p[0], p[1], p[2] } } }, 10, false, 0)) throw std::runtime_error("Source render order unexpectedly exhausted storage");
			}
			for (int bucket = buffer.nMaxOffs; bucket >= buffer.nMinOffs; --bucket)
				if (buffer.depthBuffer[bucket]) {
					buffer.pBuffer = buffer.depthBuffer + bucket;
					source.RenderBuffer(buffer, true);
				}
			const auto actual = xl_compat::particle_draw_order(positions, viewer, maximum);
			if (actual != source.drawn || buffer.nItems[0]) throw std::runtime_error("Particle distance bucket order differs from source");
			items += unsigned(actual.size());
			++cases;
		}
	std::cout << "Pinned particle render order: " << cases << " viewer/depth-limit profiles, " << items << " ordered entries, distance bias, clipping and reversed equal-bucket insertion passed\n";
}
