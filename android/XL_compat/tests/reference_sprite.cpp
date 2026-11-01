#include "sprite_render.h"
#include <array>
#include <cstdint>
#include <iostream>
namespace sprite_source
{
constexpr int GLOW_SPRITES = 4, SOFT_BLEND_SPRITES = 1, GL_TEXTURE0 = 0, GL_CLAMP = 0;
struct Vector {
	std::array<float, 4> values{};
	Vector *XYZ()
	{
		return this;
	}
};
struct Bitmap {
	void SetColor() {}
} bitmap;
struct Trace {
	int begin = 0, end = 0, done = 0, strength = 0, blend = -1, draw = 0, shader = 0, deploy = 0;
	float brightness = 0;
} trace;
struct Glow {
	bool available = false, active = false;
	int viewport = 1;
	bool Available(int)
	{
		return available;
	}
	void Begin(int, int strength, bool, float brightness)
	{
		++trace.begin;
		trace.strength = strength;
		trace.brightness = brightness;
		active = true;
	}
	bool End()
	{
		++trace.end;
		const bool old = active;
		active = false;
		return old;
	}
	void Done(int)
	{
		++trace.done;
	}
	int SetViewport(int, Vector, float, float, bool)
	{
		return viewport;
	}
} glowRenderer;
struct Transparency {
	struct DataState {
		Bitmap *pBm[3]{};
		int bUseLightmaps = 0;
	} data;
	bool soft = false, texture = true;
	bool SoftBlend(int)
	{
		return soft;
	}
	DataState &Data()
	{
		return data;
	}
	void ResetBitmaps() {}
	bool LoadTexture(void *, Bitmap *, int, int, int, int, int)
	{
		return texture;
	}
} transparencyRenderer;
struct Ogl {
	void ResetClientStates(int) {}
	void SelectTMU(int, bool) {}
	void SetTexturing(bool) {}
	void SetBlendMode(int blend)
	{
		trace.blend = blend;
	}
	void RenderQuad(Bitmap *, Vector, float, float, int)
	{
		++trace.draw;
	}
} ogl;
struct Glare {
	bool success = false;
	bool LoadShader(float, int)
	{
		++trace.shader;
		return success;
	}
} glareRenderer;
struct Shader {
	void Deploy(int, bool)
	{
		++trace.deploy;
	}
} shaderManager;
struct Transformation {
	void Transform(Vector &to, const Vector &from, int)
	{
		to = from;
	}
} transformation;
void glColor4fv(const float *) {}
void glColor3f(float, float, float) {}
float X2F(int value)
{
	return float(value) / 65536.f;
}
struct CTranspSprite {
	float fSoftRad = 0;
	int bAdditive = 0, bColor = 1, nWidth = 65536, nHeight = 65536;
	Bitmap *pBm = &bitmap;
	Vector color, position;
	void Render();
};
using CFloatVector = Vector;
using GLfloat = float;
#define ENTER(...)
#define RETURN         return;
#define TRANSP_SPRITES 1
#define DBG            0
#include "xl_reference_sprite.h"
#undef DBG
#undef TRANSP_SPRITES
#undef RETURN
#undef ENTER
} // namespace sprite_source
namespace corona_source
{
constexpr int POW_EXTRA_LIFE = 0, POW_ENERGY = 1, POW_SHIELD_BOOST = 2, POW_KEY_BLUE = 4, POW_KEY_GOLD = 6, POW_HOARD_ORB = 7, POW_CLOAK = 23, POW_INVUL = 25;
using fix = int32_t;
struct CFloatVector {
	struct {
		float values[4];
	} v;
	void Set(float r, float g, float b)
	{
		v = { r, g, b, 1 };
	}
	CFloatVector &operator/=(float scale)
	{
		for (int i = 0; i < 3; ++i) v.values[i] /= scale;
		return *this;
	}
	CFloatVector &operator*=(float scale)
	{
		for (int i = 0; i < 3; ++i) v.values[i] *= scale;
		return *this;
	}
	float &Alpha()
	{
		return v.values[3];
	}
};
struct CObject {
	struct {
		int nId = 0;
		fix xSize = 0;
		struct {
			int vPos = 0;
		} position;
	} info;
};
struct Options {
	struct {
		struct {
			bool bPowerups = true, bWeapons = false;
			int nObjIntensity = 1;
		} coronas;
	} render;
} options;
auto *gameOpts = &options;
constexpr float coronaIntensities[] = { .35f, .5f, .75f, 1.f };
struct Bitmap {
	CFloatVector color{};
	void SetColor(const CFloatVector *value)
	{
		if (value) color = *value;
	}
} bitmap;
struct Glare {
	bool loaded = true;
	bool Load()
	{
		return loaded;
	}
	auto Bitmap() -> corona_source::Bitmap *
	{
		return &bitmap;
	}
} glare;
struct Renderer {
	int draws = 0, width = 0, height = 0, blend = 0, radius = 0;
	float alpha = 0;
	void RenderSprite(Bitmap *, int, int w, int h, float a, int b, int r)
	{
		++draws;
		width = w;
		height = h;
		alpha = a;
		blend = b;
		radius = r;
	}
} ogl;
template <class T>
T Max(T a, T b)
{
	return std::max(a, b);
}
#define ENTER(...)
#define RETURN               return;
#define I2X(value)           ((value) * 65536)
#define LIGHTTRAIL_BLENDMODE 1
#include "xl_reference_powerup_corona.h"
#undef LIGHTTRAIL_BLENDMODE
#undef I2X
#undef RETURN
#undef ENTER
void controls()
{
	unsigned cases = 0;
	for (int id = 0; id < 50; ++id) {
		if (bool(IsEnergyPowerup(id)) != xl_compat::energy_powerup(id)) throw std::runtime_error("Powerup energy classification differs from source");
		for (bool energy : { false, true })
			for (bool weapons : { false, true })
				for (bool loaded : { false, true })
					for (int intensity = 0; intensity < 4; ++intensity)
						for (const auto rgb : { std::array<float, 3>{ 1, 1, 1 }, { .1f, .6f, .9f }, { 0.f, 1.f, 0.f } }) {
							options.render.coronas = { energy, weapons, intensity };
							glare.loaded = loaded;
							ogl = {};
							CObject object;
							object.info.nId = id;
							object.info.xSize = 123456;
							RenderPowerupCorona(&object, rgb[0], rgb[1], rgb[2], coronaIntensities[intensity]);
							const bool draw = loaded && (xl_compat::energy_powerup(id) ? energy : weapons);
							if (ogl.draws != int(draw)) throw std::runtime_error("Powerup corona eligibility differs from source");
							if (draw) {
								const auto result = xl_compat::powerup_corona(id, object.info.xSize, rgb, coronaIntensities[intensity]);
								if (result.size != ogl.width || result.size != ogl.height || ogl.blend != -1 || ogl.radius != 5 || ogl.alpha != result.color[3]) throw std::runtime_error("Powerup corona geometry differs from source");
								for (unsigned axis = 0; axis < 4; ++axis)
									if (result.color[axis] != bitmap.color.v.values[axis]) throw std::runtime_error("Powerup corona color differs from source");
							}
							++cases;
						}
	}
	std::cout << "Source powerup corona: " << cases << " eligibility/color/size controls passed\n";
}
} // namespace corona_source
void sprite_bitmap_controls();
void sprite_render_controls()
{
	sprite_bitmap_controls();
	corona_source::controls();
	using namespace sprite_source;
	unsigned cases = 0;
	for (int blend : { -2, -1, 0, 1, 2 })
		for (bool available : { false, true })
			for (bool active : { false, true })
				for (bool texture : { false, true })
					for (bool soft : { false, true })
						for (bool shader : { false, true })
							for (float radius : { 0.f, 5.f })
								for (int viewport : { -1, 1 }) {
									trace = {};
									glowRenderer = { available, active, viewport };
									transparencyRenderer.soft = soft;
									transparencyRenderer.texture = texture;
									glareRenderer.success = shader;
									CTranspSprite item;
									item.bAdditive = blend;
									item.fSoftRad = radius;
									item.Render();
									const auto plan = xl_compat::sprite_batch_plan(blend, available);
									const bool begin = plan.action == xl_compat::SpriteBatchAction::begin;
									const bool end = plan.action == xl_compat::SpriteBatchAction::end;
									const bool expected_active = begin || (!end && active);
									if (trace.begin != int(begin) || trace.end != int(end) || trace.done != int(begin) || glowRenderer.active != expected_active ||
									    (begin && (trace.strength != plan.strength || trace.brightness != plan.brightness)) ||
									    (texture && trace.blend != plan.blend) || trace.draw != int(texture && (!begin || viewport >= 0)) ||
									    trace.shader != int(texture && soft && radius > 0) || trace.deploy != int(texture && !(soft && radius > 0 && shader)))
										throw std::runtime_error("Pinned sprite dispatch differs from native batch plan");
									++cases;
								}
	std::cout << "Source sprite batch dispatch: " << cases << " blend/glow/soft-depth/texture controls passed\n";
}
