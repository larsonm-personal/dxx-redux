// Compile pinned setup/selection and actual shader arithmetic with storage adapters
#include "fog.h"
#include <cmath>
#include <iostream>
#include <vector>

struct vec2 {
	float x = 0, y = 0;
};
struct vec3 {
	float r, g, b;
	vec3 operator*(const vec3 &other) const
	{
		return { r * other.r, g * other.g, b * other.b };
	}
};
struct vec4 {
	float r, g, b, a;
	vec4(float r_, float g_, float b_, float a_) : r(r_), g(g_), b(b_), a(a_) {}
	vec4(vec3 rgb_, float a_) : vec4(rgb_.r, rgb_.g, rgb_.b, a_) {}
	vec3 rgb() const
	{
		return { r, g, b };
	}
};
float min(float a, float b)
{
	return std::min(a, b);
}
namespace production
{
#include "xl_production_fog_math.h"
}

void check(bool ok, const char *message)
{
	if (!ok) throw std::runtime_error(message);
}
struct CFile {
	std::array<int, 4> bytes;
	std::size_t position = 0;
	int ReadUByte()
	{
		return bytes.at(position++);
	}
};
struct CFogData {
	struct {
		struct {
			struct {
				float r, g, b, a;
			} color;
		} v;
		void Set(float r, float g, float b, float a)
		{
			v.color = { r, g, b, a };
		}
	} m_color{};
	void Init(int32_t);
	void Read(CFile &);
};
struct CSegment {
	int props = 0;
	std::array<int, 6> children{};
	bool HasWaterProp()
	{
		return props & 1;
	}
	bool HasLavaProp()
	{
		return props & 2;
	}
	bool HasLightFogProp()
	{
		return props & 32;
	}
	bool HasDenseFogProp()
	{
		return props & 64;
	}
	int ChildId(int side)
	{
		return children.at(side);
	}
#include "xl_reference_fog_type.h"
};
struct CSegFace {
	struct {
		int nSide;
	} m_info{};
	int segment = 0;
};
struct tSegFaces {
	CSegFace *pFace;
	int nFaces = 6;
};
std::array<CSegment, 2> segments;
std::array<std::array<CSegFace, 6>, 2> faces;
std::array<tSegFaces, 2> seg_faces;
CSegment *SEGMENT(int n)
{
	return n >= 0 && n < static_cast<int>(segments.size()) ? &segments[n] : nullptr;
}
struct {
	struct {
		int bHaveFog[5]{};
	} render;
} gameStates;
enum { GL_FRONT,
	   GL_BACK };
struct {
	int cull = GL_BACK;
	void SetCullMode(int mode)
	{
		cull = mode;
	}
} ogl;
std::vector<xl_compat::FogBoundary> drawn;
void DrawFace(CSegFace *face)
{
	drawn.push_back({ 0, face->segment, face->m_info.nSide, ogl.cull == GL_FRONT });
}
#define SEGFACES seg_faces.data()
#define DBG      0
#define ENTER(a, b)
#define RETVAL(value) return value;
#include "xl_reference_fog.h"

float depth(float eye, float near = 1.f)
{
	return (1.f + (5000.f + near - 2.f * near * 5000.f / eye) / (5000.f - near)) * .5f;
}
bool close(float a, float b, float tolerance = 0.000002f)
{
	return std::isfinite(a) && std::isfinite(b) && std::abs(a - b) <= tolerance;
}
void compare(vec4 a, vec4 b)
{
	check(close(a.r, b.r) && close(a.g, b.g) && close(a.b, b.b) && close(a.a, b.a), "Fog shader arithmetic differs from extracted source");
}

int main()
{
	try {
		xl_compat::Inventory level{};
		level.level_version = 27;
		level.segments.resize(2);
		level.fog_settings = { { 51, 102, 153, 8 }, { 204, 102, 0, 3 }, { 77, 144, 211, 12 }, { 88, 177, 233, 5 } };
		std::size_t color_cases = 0, boundary_cases = 0, math_cases = 0;
		for (int version : { 23, 25, 27 }) {
			level.level_version = version;
			for (int value = 0; value < 256; ++value) {
				for (auto &row : level.fog_settings) row = { value, 255 - value, value ^ 0x55, value };
				const auto result = xl_compat::fog_colors(level);
				for (int type = 0; type < 4; ++type) {
					CFogData source;
					if (version < 27) source.Init(type);
					else {
						CFile input{ level.fog_settings[type] };
						source.Read(input);
						check(input.position == 4, "Source fog reader did not consume its record");
					}
					const auto &c = source.m_color.v.color;
					compare({ c.r, c.g, c.b, c.a }, { result[type][0], result[type][1], result[type][2], result[type][3] });
					++color_cases;
				}
			}
		}
		level.level_version = 27;
		for (int n = 0; n < 2; ++n) {
			segments[n].children.fill(-1);
			segments[n].children[n] = 1 - n;
			level.segments[n].children = segments[n].children;
			for (int side = 0; side < 6; ++side) faces[n][side] = { { side }, n };
			seg_faces[n] = { faces[n].data(), 6 };
		}
		for (int first = 0; first < 256; ++first) {
			segments[0].props = level.segments[0].properties = first;
			check(xl_compat::fog_type(first) == segments[0].FogType(), "Fog property priority differs");
			for (int second = 0; second < 256; ++second) {
				segments[1].props = level.segments[1].properties = second;
				for (int n = 0; n < 2; ++n)
					for (int mode = 0; mode < 2; ++mode) {
						drawn.clear();
						const int type = xl_compat::fog_type(level, n);
						if (type) RenderFogFaces(static_cast<int16_t>(n), type, mode);
						std::size_t index = 0;
						for (int side = 0; side < 6; ++side) {
							const auto cap = xl_compat::fog_boundary(level, n, side, mode != 0);
							if (!cap.type) continue;
							check(index < drawn.size(), "Production fog invented a boundary");
							const auto &expected = drawn[index++];
							check(cap.type == type && cap.segment == expected.segment && cap.side == expected.side && cap.reversed == expected.reversed, "Fog boundary ownership/culling differs");
						}
						check(index == drawn.size() && ogl.cull == GL_BACK, "Production fog omitted a boundary or source leaked culling");
						++boundary_cases;
					}
			}
		}
		production::scenePlanes = { 1.f, 5000.f };
		const float positions[]{ 1.f, 2.f, 4.f, 10.f, 30.f, 60.f, 160.f, 500.f, 1000.f, 2500.f, 4999.f, 5000.f };
		for (int distance : { 0, 1, 3, 5, 8, 12, 63, 127, 255 })
			for (float scene : positions)
				for (float near : positions)
					for (float far : positions) {
						const vec4 color(.2f, .4f, .6f, distance * 20.f);
						const float eye = production::EyeDepth(depth(scene));
						compare(CalcFogColor(color, eye, depth(near), depth(far)), production::FogColor(color, eye, depth(near), depth(far)));
						++math_cases;
					}
		// Empty buffers, mixed types and pair order are source behavior, including
		// multiplication by an inactive type's neutral white and summed opacity
		for (int mask = 0; mask < 16; ++mask) {
			const vec4 first(.2f, .4f, .6f, mask & 1 ? 160.f : 0.f);
			const vec4 second(.8f, .4f, 0.f, mask & 2 ? 60.f : 0.f);
			const vec4 caps(mask & 4 ? depth(10.f) : 1.f, mask & 4 ? depth(80.f) : 0.f, mask & 8 ? depth(30.f) : 1.f, mask & 8 ? depth(90.f) : 0.f);
			const float eye = production::EyeDepth(depth(50.f));
			const auto a = CalcFogColor(first, eye, caps.r, caps.g);
			const auto b = CalcFogColor(second, eye, caps.b, caps.a);
			compare(vec4(a.rgb() * b.rgb(), min(1.f, a.a + b.a)), production::FogPair(first, second, eye, caps));
			++math_cases;
		}
		std::cout << "Fog source comparison: " << color_cases << " color records, " << boundary_cases << " boundary configurations, " << math_cases << " shader cases\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
