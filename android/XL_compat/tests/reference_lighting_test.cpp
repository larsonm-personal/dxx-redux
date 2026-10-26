// Minimal data surface for compiling the pinned, unchanged XL lighting functions
// This is a calculation oracle, not a substitute for reference gameplay/rendering
#include "lighting.h"
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>

namespace reference
{
using fix = std::int32_t;
struct CFloatVector {
	struct {
		struct {
			float r = 0, g = 0, b = 0, a = 1;
		} color;
	} v;
	float &Red()
	{
		return v.color.r;
	}
	float &Green()
	{
		return v.color.g;
	}
	float &Blue()
	{
		return v.color.b;
	}
	float &Alpha()
	{
		return v.color.a;
	}
	void Set(float r, float g, float b)
	{
		v.color.r = r;
		v.color.g = g;
		v.color.b = b;
	}
	void Assign(const CFloatVector &other)
	{
		v = other.v;
	}
	CFloatVector &operator*=(float factor)
	{
		v.color.r *= factor;
		v.color.g *= factor;
		v.color.b *= factor;
		return *this;
	}
	CFloatVector &operator*=(const CFloatVector &other)
	{
		v.color.r *= other.v.color.r;
		v.color.g *= other.v.color.g;
		v.color.b *= other.v.color.b;
		return *this;
	}
	CFloatVector &operator+=(const CFloatVector &other)
	{
		v.color.r += other.v.color.r;
		v.color.g += other.v.color.g;
		v.color.b += other.v.color.b;
		return *this;
	}
	CFloatVector operator*(float factor) const
	{
		auto copy = *this;
		copy *= factor;
		return copy;
	}
};
struct CFaceColor : CFloatVector {
	int index = 1;
};
struct CBitmap {
	int flags = 0;
	int Flags() const
	{
		return flags;
	}
};
struct {
	struct {
		struct {
			CFaceColor ambient[1];
			float vertBright[1];
		} color;
		struct {
			fix dynamicLight[1];
			bool bGotDynColor[1];
			CFloatVector dynamicColor[1];
		} lights;
		float fBrightness = 1;
	} renderData;
	struct {
		bool bDestroyed = false;
	} reactorData;
} gameData;
struct {
	struct {
		bool bHaveExtraGameInfo[1];
		int bEndLevelSequence = 0;
	} app;
	struct {
		bool bAmbientColor = true, bHeadlightOn = false;
		fix nFlashScale = 65536;
	} render;
	struct {
		struct {
			int nMagnitude = 0;
		} seismic;
	} gameplay;
} gameStates;
struct Options {
	struct {
		struct {
			bool bMix = true, bCap = false;
			int nLevel = 1;
		} color;
	} render;
} options;
Options *gameOpts = &options;
struct {
	struct {
		bool bScaleLight = false;
	} m_states;
} ogl;
bool darkness = false;
constexpr int IsMultiGame = 0, MAX_LIGHT = 65536, EL_OUTSIDE = 1, BM_FLAG_NO_LIGHTING = 1;
float X2F(fix value)
{
	return float(value) / 65536;
}
fix FixMul(fix a, fix b)
{
	const auto value = std::int64_t(a) * b;
	return fix(value >= 0 ? value / 65536 : -((-value + 65535) / 65536));
}
#define EGI_FLAG(a, b, c, d) darkness
#include "xl_reference_lighting.h"
#undef EGI_FLAG

xl_compat::ShadedVertex shade(const xl_compat::VertexLighting &in)
{
	CFaceColor face;
	face.Set(in.face[0], in.face[1], in.face[2]);
	face.Alpha() = in.face_alpha;
	face.index = in.face_index;
	gameData.renderData.color.ambient[0].Set(in.ambient[0], in.ambient[1], in.ambient[2]);
	gameData.renderData.color.vertBright[0] = in.vertex_brightness;
	gameData.renderData.lights.dynamicLight[0] = in.dynamic_light;
	gameData.renderData.lights.bGotDynColor[0] = in.has_dynamic_color;
	gameData.renderData.lights.dynamicColor[0].Set(in.dynamic_color[0], in.dynamic_color[1], in.dynamic_color[2]);
	gameData.renderData.fBrightness = in.brightness;
	gameData.reactorData.bDestroyed = in.flashing;
	gameStates.render.nFlashScale = in.flash_scale;
	gameStates.render.bAmbientColor = in.ambient_enabled;
	gameStates.render.bHeadlightOn = in.headlight;
	gameStates.app.bHaveExtraGameInfo[0] = in.extra_game_info;
	gameStates.app.bEndLevelSequence = in.outside_endlevel ? EL_OUTSIDE : 0;
	options.render.color.bMix = in.mix_colors;
	options.render.color.bCap = in.cap_colors;
	options.render.color.nLevel = in.colored ? 1 : 0;
	ogl.m_states.bScaleLight = in.scale_light;
	darkness = in.darkness;
	SetVertexColor(0, &face, in.blend);
	const auto light = SetVertexLight(0, 0, 0, &face, in.static_light);
	CBitmap bitmap;
	bitmap.flags = in.no_bitmap_lighting ? BM_FLAG_NO_LIGHTING : 0;
	AdjustVertexColor(&bitmap, &face, light);
	return { { face.Red(), face.Green(), face.Blue() }, light };
}
} // namespace reference

int main()
{
	std::uint32_t state = 0x584c;
	const auto random = [&]() { state = state * 1664525u + 1013904223u; return state; };
	try {
		for (unsigned sample = 0; sample < 8192; ++sample) {
			xl_compat::VertexLighting input;
			for (unsigned i = 0; i < 3; ++i) {
				input.face[i] = float(random() % 1000) / 500;
				input.ambient[i] = float(random() % 1000) / 500;
				input.dynamic_color[i] = float(random() % 1000) / 500;
			}
			input.face_alpha = float(random() % 101) / 100;
			input.vertex_brightness = float(random() % 20) / 10;
			input.brightness = float(random() % 20) / 10;
			input.static_light = random() % 131073;
			input.dynamic_light = random() % 65537;
			input.flash_scale = random() % 131073;
			input.blend = sample % 3;
			input.face_index = sample & 1;
			input.ambient_enabled = sample & 2;
			input.darkness = sample & 4;
			input.flashing = sample & 8;
			input.extra_game_info = sample & 16;
			input.has_dynamic_color = sample & 32;
			input.mix_colors = sample & 64;
			input.colored = sample & 128;
			input.cap_colors = sample & 256;
			input.scale_light = sample & 512;
			input.headlight = sample & 1024;
			input.outside_endlevel = sample & 2048;
			input.no_bitmap_lighting = sample & 4096;
			const auto actual = xl_compat::shade_vertex(input), expected = reference::shade(input);
			bool equal = actual.scalar_light == expected.scalar_light;
			for (unsigned i = 0; i < 3; ++i) equal = equal && std::abs(actual.rgb[i] - expected.rgb[i]) <= 0.000001f;
			if (!equal) throw std::runtime_error("reference lighting mismatch at sample " + std::to_string(sample));
		}
		std::cout << "8192 pinned XL lighting calculations match; runtime/rendering parity remains unverified\n";
		return 0;
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
