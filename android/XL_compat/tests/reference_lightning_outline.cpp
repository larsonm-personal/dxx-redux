#include "lightning_outline.h"
#include <iostream>

namespace lightning_outline_reference
{
using GLfloat = float;
template <class T>
T Min(T a, T b)
{
	return std::min(a, b);
}
template <class T>
T Max(T a, T b)
{
	return std::max(a, b);
}
template <class T>
T Clamp(T value, T low, T high)
{
	return std::clamp(value, low, high);
}
struct CMeshEdge {
#include "xl_reference_lightning_distance.h"
};
struct CCanvas {
	int width = 640;
	int Width() const
	{
		return width;
	}
	static CCanvas *Current();
} canvas;
CCanvas *CCanvas::Current()
{
	return &canvas;
}
struct {
	int Active() const
	{
		return 0;
	}
} automap;
struct CRenderStates {
	float outlineWidth[2][2]{ { 3, 2 }, { 2, 1 } };
	float OutlineWidth(int32_t, float, int32_t);
};
struct {
	CRenderStates render;
} gameStates;
struct {
	struct {
		float lineWidthRange[2]{ 1, 255 };
	} m_data;
	void SetBlendMode(int) {}
	void SetLineSmooth(bool) {}
	bool EnableClientStates(int, int, int, int)
	{
		return true;
	}
	void DisableClientStates(int, int, int, int) {}
	void SetTexturing(bool) {}
	void SelectTMU(int, bool = false) {}
	void ClearError(int) {}
} ogl;
using CFixVector = std::array<int, 3>;
struct CFloatVector3 {
	std::array<float, 3> point{};
	void Assign(const CFixVector &fixed)
	{
		for (unsigned i = 0; i < 3; ++i) point[i] = float(fixed[i]) / 65536.f;
	}
	static float Dist(const CFloatVector3 &a, const CFloatVector3 &b)
	{
		const float x = a.point[0] - b.point[0], y = a.point[1] - b.point[1], z = a.point[2] - b.point[2];
		return std::sqrt(x * x + y * y + z * z);
	}
};
struct CFloatVector {
	float values[4]{};
};
struct Viewer {
	CFixVector position{};
	int size = 0;
	const CFixVector &Position() const
	{
		return position;
	}
	int Size() const
	{
		return size;
	}
} viewer;
struct {
	struct {
		Viewer *pViewer = &viewer;
	} objData;
} gameData;
struct {
	unsigned ends = 0, begins = 0;
	void Begin(int, int, bool, float)
	{
		++begins;
	}
	void End()
	{
		++ends;
	}
	bool Available(int) const
	{
		return true;
	}
} glowRenderer;
struct {
	struct {
		struct {
			bool bGlow = true;
		} lightning;
	} render;
} options;
auto *gameOpts = &options;
struct Corona {
	bool Load() const
	{
		return true;
	}
	Corona *Bitmap()
	{
		return this;
	}
	Corona *Texture()
	{
		return this;
	}
	int Bind(int) const
	{
		return 0;
	}
	void Wrap(int) {}
} corona;
struct Storage {
	std::vector<CFloatVector3> values;
	CFloatVector3 *Buffer()
	{
		return values.data();
	}
	CFloatVector3 &operator[](int i)
	{
		return values.at(i);
	}
};
struct CLightning {
	Storage m_coreVerts;
	int m_nNodes = 0, m_bRandom = 0, m_bGlow = 0;
	float m_width = 20, m_fDistScale = 1;
	int m_bBlur = 0;
	void RenderCore(CFloatVector *, int32_t, int32_t);
	int32_t SetupGlow();
};
constexpr int OGL_BLEND_ADD = 1, GL_TEXTURE0 = 0, GLOW_LIGHTNING = 1, GL_FLOAT = 1, GL_LINE_STRIP = 1, GL_CLAMP = 1;
std::vector<float> widths;
int vertices = 0;
void glColor4fv(const float *) {}
void glLineWidth(float width)
{
	widths.push_back(width);
}
void OglVertexPointer(int, int, int, const void *) {}
void OglDrawArrays(int, int, int count)
{
	vertices = count;
}
#define ENTER(a, b)
#define RETURN        return;
#define RETVAL(value) return (value);
#define GL_FALLBACK   0
#define X2F(value)    (float(value) / 65536.f)
#include "xl_reference_lightning_outline.h"
#undef X2F
#undef GL_FALLBACK
#undef RETURN
#undef RETVAL
#undef ENTER
} // namespace lightning_outline_reference

void lightning_outline_controls()
{
	using namespace lightning_outline_reference;
	unsigned cases = 0;
	CLightning nonglow;
	if (nonglow.SetupGlow() != 0 || glowRenderer.ends != 1 || glowRenderer.begins != 0)
		throw std::runtime_error("Pinned non-glowing SetupGlow did not end the previous glow batch");
	for (int canvas_width : { 320, 640, 1280, 4096 })
		for (int radius : { 0, 32768, 4 * 65536, 32 * 65536 })
			for (float distance : { 0.f, .00001f, .001f, .01f, .1f, .25f, .5f, 1.f, 2.f, 3.f, 4.f, 5.f, 8.f, 10.f, 20.f, 30.f, 50.f, 100.f, 200.f, 500.f, 1000.f, 5000.f, 15000.f, 30000.f })
				for (int random_nodes : { 0, 1, 2 })
					for (const CFixVector &position : { CFixVector{ 0, 0, 0 }, CFixVector{ 2 * 65536, -17 * 65536, 40 * 65536 } }) {
						canvas.width = canvas_width;
						viewer = { position, radius };
						CFloatVector3 eye;
						eye.Assign(position);
						xl_compat::LightningCore core{};
						core.vertices = { { eye.point[0] + distance * 2, eye.point[1], eye.point[2] }, eye.point, { eye.point[0], eye.point[1] + distance, eye.point[2] } };
						for (int i = 0; i < random_nodes; ++i) core.vertices.push_back(eye.point);
						core.vertices.resize(core.vertices.size() + 4, eye.point);
						CLightning source;
						source.m_nNodes = int(core.vertices.size()) - 4;
						source.m_bRandom = random_nodes;
						for (const auto &point : core.vertices) source.m_coreVerts.values.push_back({ point });
						widths.clear();
						CFloatVector color;
						source.RenderCore(&color, 0, 0);
						const auto actual = xl_compat::lightning_outline_width(core, random_nodes, eye.point, float(radius) / 65536.f, canvas_width);
						if (widths.size() != 2 || widths.back() != 1 || widths.front() != actual || vertices != 3)
							throw std::runtime_error("Non-glowing lightning width or endpoint range differs from pinned RenderCore");
						++cases;
					}
	std::cout << "Pinned non-glowing lightning: " << cases << " endpoint, viewer-radius, canvas and outline-width controls passed\n";
}
