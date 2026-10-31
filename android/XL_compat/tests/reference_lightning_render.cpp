#include "lightning_render.h"
#include <cstring>
#include <iostream>
namespace lightning_render_reference
{
struct CFixVector {
	struct {
		struct {
			int x, y, z;
		} coord;
	} v;
};
struct CFloatVector {
	union {
		struct {
			float x, y, z, w;
		} coord;
		float values[4];
	} v{};
	static const CFloatVector ZERO;
	void Set(float x, float y, float z, float w = 1)
	{
		v.coord = { x, y, z, w };
	}
	void SetZero()
	{
		v = {};
	}
	CFloatVector &Assign(const CFixVector &);
	float &Red()
	{
		return v.coord.x;
	}
	float &Green()
	{
		return v.coord.y;
	}
	float &Blue()
	{
		return v.coord.z;
	}
	float &Alpha()
	{
		return v.coord.w;
	}
	const float SqrMag() const;
	const float Mag() const;
	static const float Normalize(CFloatVector &);
	static const CFloatVector Avg(const CFloatVector &, const CFloatVector &);
	static const CFloatVector Cross(const CFloatVector &, const CFloatVector &);
	static const CFloatVector Perp(const CFloatVector &, const CFloatVector &, const CFloatVector &);
	static const CFloatVector Normal(const CFloatVector &, const CFloatVector &, const CFloatVector &);
	const CFloatVector &operator+=(const CFloatVector &);
	const CFloatVector &operator*=(float);
	const CFloatVector &operator/=(float);
	const CFloatVector operator+(const CFloatVector &) const;
	const CFloatVector operator-(const CFloatVector &) const;
	const CFloatVector operator*(float) const;
	const CFloatVector operator/(float) const;
};
const CFloatVector CFloatVector::ZERO{};
struct CFloatVector3 {
	union {
		struct {
			float x, y, z;
		} coord;
		float values[3];
	} v{};
	void Set(float x, float y, float z)
	{
		v.coord = { x, y, z };
	}
	CFloatVector3 &Assign(const CFixVector &);
	const float SqrMag() const;
	const float Mag() const;
	const CFloatVector3 &operator+=(const CFloatVector3 &);
	const CFloatVector3 &operator/=(float);
	const CFloatVector3 operator-(const CFloatVector3 &) const;
};
struct {
	unsigned view = 0;
	void Transform(CFloatVector3 &out, const CFloatVector3 &in) const
	{
		out = in;
		if (view == 1) out.v.coord.z = in.v.coord.x - 30.0f;
		if (view == 2) out.v.coord.z = in.v.coord.x * 0.6f + in.v.coord.z * 0.8f + 200.0f;
	}
} transformation;
struct CLightning;
struct CLightningNode {
	CFixVector m_vPos;
	CLightning *GetChild()
	{
		return nullptr;
	}
};
struct tTexCoord2f {
	struct {
		float u, v;
	} v;
};
template <class T>
struct Storage {
	std::vector<T> values;
	T *Buffer()
	{
		return values.data();
	}
	T &operator[](int i)
	{
		return values.at(i);
	}
};
struct CLightning {
	Storage<CLightningNode> m_nodes;
	Storage<CFloatVector> m_plasmaVerts;
	Storage<CFloatVector3> m_coreVerts;
	Storage<tTexCoord2f> m_plasmaTexCoord;
	int m_nNodes = 0, m_bRandom = 1, m_bGlow = 1;
	int m_nFrames = 2, m_nLife = -300, m_nTTL = 200, m_bBlur = 1;
	CFloatVector m_color, observed;
	unsigned core_calls = 0, glow_calls = 0, setup_calls = 0;
	int SetupGlow()
	{
		++setup_calls;
		return m_bGlow;
	}
	void RenderCore(CFloatVector *color, int, int)
	{
		observed = *color;
		++core_calls;
	}
	void RenderGlow(CFloatVector *color, int, int)
	{
		observed = *color;
		++glow_calls;
	}
	void Draw(int32_t, int32_t);
	float m_width = 20, m_fAvgDist = 0, m_fDistScale = 0;
	float ComputeDistScale(float);
	void ComputeGlow(int32_t, int32_t);
	void ComputeCore();
	float ComputeAvgDist(CFloatVector3 *, int32_t);
};
struct {
	struct {
		struct {
			bool bGlow = true;
		} lightning;
	} render;
} options;
auto *gameOpts = &options;
struct {
	struct {
		struct {
			struct {
				CFixVector vPos;
			} viewer;
		} mine;
	} renderData;
} gameData;
float depth_range = 9999;
struct {
	int bUseLightning = 1;
} extraGameInfo[1];
struct {
	int viewport = 1;
	unsigned done = 0;
	template <class T>
	int SetViewport(int, T *, int)
	{
		return viewport;
	}
	void Done(int)
	{
		++done;
	}
} glowRenderer;
struct {
	void ClearError(int) {}
} ogl;
constexpr int GLOW_LIGHTNING = 1, GL_MAX = 2, GL_FUNC_ADD = 3;
std::vector<int> blend_calls;
void glBlendEquation(int mode)
{
	blend_calls.push_back(mode);
}
unsigned random_index = 0;
double RandDouble()
{
	return double((random_index++ * 7919u + 12345u) % 32768u) / 32767.0;
}
#define ZRANGE     depth_range
#define X2F(value) (float(value) / 65536.0f)
#define ENTER(...)
#define RETURN return;
#include "xl_reference_lightning_glow.h"
#undef RETURN
#undef ENTER
#undef X2F
#undef ZRANGE
} // namespace lightning_render_reference

void lightning_render_controls()
{
	using namespace lightning_render_reference;
	unsigned cases = 0;
	for (int count : { 3, 9, 17 })
		for (int random_nodes : { 0, 1, 2 })
			for (float width : { 1.0f, 5.0f, 20.0f })
				for (float depth : { -25.0f, 0.0f, 100.0f, 999.0f })
					for (int mode = 0; mode < 3; ++mode) {
						if (count - random_nodes < 2) continue;
						CLightning source;
						source.m_nNodes = count;
						source.m_bRandom = random_nodes;
						source.m_width = width;
						source.m_fAvgDist = depth;
						const bool outer = mode == 0;
						options.render.lightning.bGlow = mode != 1;
						gameData.renderData.mine.viewer.vPos.v.coord = { 300000, -500000, 1900000 };
						source.m_nodes.values.resize(count);
						const int vertices = 4 * (count - 1 - random_nodes) * (outer ? 2 : 1);
						source.m_plasmaVerts.values.resize(vertices);
						source.m_plasmaTexCoord.values.resize(vertices);
						std::vector<xl_compat::LightningPoint> nodes;
						for (int i = 0; i < count; ++i) {
							xl_compat::LightningPoint point{ 100000 + i * 75431, (i % 3 - 1) * 17513, 300000 + i * 29137 };
							nodes.push_back(point);
							source.m_nodes[i].m_vPos.v.coord = { point[0], point[1], point[2] };
						}
						source.ComputeGlow(mode == 2 ? 1 : 0, 0);
						const auto actual = xl_compat::lightning_glow_geometry(nodes, random_nodes, width, outer, { 300000 / 65536.0f, -500000 / 65536.0f, 1900000 / 65536.0f }, depth, depth_range);
						if (actual.vertices.size() != std::size_t(vertices) || actual.uv.size() != std::size_t(vertices) || actual.distance_scale != source.m_fDistScale)
							throw std::runtime_error("Lightning glow counts or distance scaling differ from source");
						for (int i = 0; i < vertices; ++i) {
							for (unsigned axis = 0; axis < 3; ++axis)
								if (actual.vertices[i][axis] != source.m_plasmaVerts[i].v.values[axis]) throw std::runtime_error("Lightning glow vertex differs from source");
							if (actual.uv[i] != std::array<float, 2>{ source.m_plasmaTexCoord[i].v.u, source.m_plasmaTexCoord[i].v.v })
								throw std::runtime_error("Lightning glow UV differs from source");
						}
						transformation.view = mode;
						source.m_coreVerts.values.resize(count + 4);
						source.ComputeCore();
						const auto core = xl_compat::lightning_core_geometry(nodes, [](const xl_compat::LightningVertex &vertex) {
							CFloatVector3 input, output;
							input.Set(vertex[0], vertex[1], vertex[2]);
							transformation.Transform(output, input);
							return output.v.coord.z;
						});
						if (core.vertices.size() != source.m_coreVerts.values.size() || core.average_depth != source.m_fAvgDist)
							throw std::runtime_error("Lightning core count or camera depth differs from source");
						for (unsigned i = 0; i < core.vertices.size(); ++i)
							for (unsigned axis = 0; axis < 3; ++axis)
								if (core.vertices[i][axis] != source.m_coreVerts[i].v.values[axis]) throw std::runtime_error("Lightning core endpoint differs from source");
						++cases;
					}
	std::cout << "Pinned lightning geometry: " << cases << " core/depth/billboard/layer/endpoint/UV cases passed\n";
	unsigned draws = 0;
	for (bool blur : { false, true })
		for (int visible : { -1, 0, 1 })
			for (unsigned seed : { 0u, 17u, 991u }) {
				CLightning source;
				source.m_nNodes = 9;
				source.m_nodes.values.resize(9);
				source.m_plasmaVerts.values.resize(64);
				source.m_bBlur = blur;
				source.m_color.Set(.12f, .58f, .83f, .37f);
				glowRenderer.viewport = visible;
				glowRenderer.done = 0;
				blend_calls.clear();
				random_index = seed;
				source.Draw(0, 0);
				const unsigned source_end = random_index;
				struct Random {
					double unit()
					{
						return RandDouble();
					}
				} random;
				random_index = seed;
				const bool shown = !blur || visible >= 0;
				const auto color = xl_compat::lightning_draw_color({ .12f, .58f, .83f, .37f }, blur && shown, random);
				if (source_end != seed + 3 || random_index != source_end || source.core_calls != unsigned(shown) || source.glow_calls != unsigned(shown) || source.setup_calls != 1 || glowRenderer.done != 1)
					throw std::runtime_error("Lightning draw visibility or RNG order differs from source");
				if (shown)
					for (unsigned channel = 0; channel < 4; ++channel)
						if (color[channel] != source.observed.v.values[channel]) throw std::runtime_error("Lightning draw color differs from source");
				if (blend_calls != (blur && shown ? std::vector<int>{ GL_MAX, GL_FUNC_ADD } : std::vector<int>{}))
					throw std::runtime_error("Lightning blur blend equation sequence changed");
				++draws;
			}
	std::cout << "Pinned lightning draw: " << draws << " visibility/color/RNG/blend cases passed\n";
}
