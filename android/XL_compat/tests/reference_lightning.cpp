// Geometry Setup is an explicit boundary; these controls compare its scheduling
#include "lightning_effect.h"
#include <cmath>
#include <cstring>
#include <iostream>

namespace reference
{
struct CFixVector {
	std::array<int, 3> values;
	void SetZero()
	{
		values = {};
	}
	bool IsZero() const
	{
		return values == std::array<int, 3>{};
	}
	static int Dist(const CFixVector &a, const CFixVector &b)
	{
		return std::abs(a.values[0] - b.values[0]);
	}
};
struct CFloatVector {
	std::array<float, 4> values;
	float &Red()
	{
		return values[0];
	}
	float &Green()
	{
		return values[1];
	}
	float &Blue()
	{
		return values[2];
	}
	float &Alpha()
	{
		return values[3];
	}
};
struct tLightningInfo {
	int nLife, nDelay, nLength, nAmplitude, nOffset, nWayPoint;
	int16_t nBolts, nId, nTarget, nNodes, nChildren, nFrames;
	char nWidth, nAngle, nStyle, nSmoothe, bClamp, bGlow, bBlur, bSound;
	CFloatVector color;
};
struct {
	struct {
		struct {
			int nStyle;
		} lightning;
	} render;
} options;
auto *gameOpts = &options;
struct {
	int bUseLightning;
} extraGameInfo[1];
double random_value;
unsigned random_calls;
double RandDouble()
{
	++random_calls;
	return random_value;
}
#define ENTER(...)
#define RETURN        return;
#define RETVAL(value) return (value);
#define I2X(value)    ((value) * 65536)
#define OBJECT(value) ((value) >= 0)
struct CLightning {
	int16_t m_nObject, m_nSegment, m_nNode, m_nNodes, m_nChildren, m_nFrames, m_nSmoothe, m_iStep;
	int m_nLife, m_nTTL, m_nLength, m_nDelay, m_nAmplitude, m_nOffset;
	char m_bRandom, m_bInPlane, m_nAngle, m_bClamp, m_bGlow, m_bBlur, m_bLight, m_nStyle;
	float m_width;
	CFixVector m_vRefEnd, m_vEnd, m_vDelta, m_vBase;
	CFloatVector m_color;
	CLightning *m_parent;
	unsigned setups, destroys;
	void Init(CFixVector *, CFixVector *, CFixVector *, int16_t, int32_t, int32_t, int32_t, int32_t,
	          char, int32_t, int16_t, int16_t, int16_t, int16_t, char, char, char, char, char, float, CFloatVector *, CLightning *, int16_t);
	int32_t SetLife();
	void Setup(bool)
	{
		++setups;
	}
	void DestroyNodes()
	{
		++destroys;
	}
};
struct CLightningManager {
	xl_compat::LightningDefinition captured{};
	int32_t Create(tLightningInfo &, CFixVector *, CFixVector *, CFixVector *, int16_t);
	int Create(int bolts, CFixVector *, CFixVector *, CFixVector *, int16_t, int life, int delay,
	           int length, int amplitude, char angle, int offset, int nodes, int children, bool,
	           int frames, int smooth, char clamp, char glow, char blur, char sound, char light,
	           char style, float width, CFloatVector *color)
	{
		captured = { life, delay, length, amplitude, offset, bolts, nodes, children, frames,
			         angle, smooth, style, clamp, glow, blur, sound, width, color->values, false, false, false, bool(light) };
		return 1;
	}
};
#include "xl_reference_lightning.h"
#undef ENTER
#undef RETURN
#undef RETVAL
#undef I2X
#undef OBJECT
} // namespace reference

void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}

void compare(const xl_compat::Object &object, int style, bool endpoint, bool children)
{
	using namespace reference;
	const auto &p = object.render_data;
	options.render.lightning.nStyle = style;
	extraGameInfo[0].bUseLightning = children ? 2 : 1;
	tLightningInfo info{ p[0], p[1], p[2], p[3], p[4], p[5], int16_t(p[6]), int16_t(p[7]), int16_t(p[8]), int16_t(p[9]), int16_t(p[10]), int16_t(p[11]), char(p[12]), char(p[13]), char(p[14]), char(p[15]), char(p[16]), char(p[17]), char(p[17]), char(p[18]), { { float(p[21]), float(p[22]), float(p[23]), float(p[24]) } } };
	CFixVector pos{ { 0, 0, 0 } }, end{ { 13 * 65536, 0, 0 } }, plane{ { 65536, 0, 0 } };
	CLightningManager manager;
	manager.Create(info, &pos, &end, &plane, 307);
	const auto d = xl_compat::lightning_definition(object, style, 13 * 65536);
	const auto &r = manager.captured;
	require(d.life == r.life && d.delay == r.delay && d.length == r.length && d.amplitude == r.amplitude && d.offset == r.offset &&
	            d.bolts == r.bolts && d.nodes == r.nodes && d.children == r.children && d.frames == r.frames && d.angle == r.angle &&
	            d.smooth == r.smooth && d.style == r.style && d.clamp == r.clamp && d.glow == r.glow && d.blur == r.blur &&
	            d.sound == r.sound && d.width == r.width && d.color == r.color && d.light == r.light,
	        "Authored lightning conversion differs from pinned manager");
	if (d.nodes <= 4) return;
	CLightning source{};
	source.Init(&pos, endpoint ? &end : nullptr, &plane, 307, r.life, r.delay, r.length, r.amplitude, char(r.angle), r.offset,
	            int16_t(r.nodes), int16_t(r.children), int16_t(r.frames), int16_t(r.smooth), char(r.clamp), char(r.glow), char(r.blur), char(r.light),
	            char(r.style), r.width, &info.color, nullptr, -1);
	const auto initial = xl_compat::lightning_lifetime(d, endpoint, children);
	require(initial.life == source.m_nLife && initial.ttl == source.m_nTTL && initial.delay == source.m_nDelay &&
	            initial.nodes == source.m_nNodes && initial.random_nodes == source.m_bRandom && initial.length == source.m_nLength &&
	            initial.children == source.m_nChildren && initial.frames == source.m_nFrames && initial.step == source.m_iStep,
	        "Lightning initialization differs from pinned bolt");
	for (double random : { 0.0, 0.125, 0.5, 0.999 }) {
		for (int ttl : { 30, 0, -17 }) {
			for (int sign : { -1, 1 }) {
				auto native = initial;
				auto oracle = source;
				native.ttl = oracle.m_nTTL = ttl;
				native.nodes *= sign;
				oracle.m_nNodes *= sign;
				random_value = random;
				random_calls = 0;
				const auto keep = oracle.SetLife();
				unsigned calls = 0;
				const auto action = xl_compat::lightning_set_life(native, [&] { ++calls; return random; });
				require(keep && action != xl_compat::LightningLifeAction::expire && native.ttl == oracle.m_nTTL && native.nodes == oracle.m_nNodes &&
				            calls == random_calls && oracle.setups == unsigned(action == xl_compat::LightningLifeAction::restart),
				        "Lightning active/delay transition or random visits differ from pinned bolt");
			}
		}
	}
}

void lightning_path_controls();
void lightning_bolt_controls();
void lightning_emitter_controls();

int main()
{
	try {
		lightning_path_controls();
		lightning_bolt_controls();
		lightning_emitter_controls();
		xl_compat::Object original{};
		original.render = 12;
		original.render_data = { 300, 50, 18, 5, 1, -1, 3, 0, 0, 8, 0, 2, 20, 0, 0, 0, 0, 1, 1, 1, 0, 0, 52, 149, 128, 1 };
		unsigned count = 0;
		for (int style : { 0, 1, 2 })
			for (int nodes : { -8, 8, 20 })
				for (int amplitude : { -5, 5 })
					for (int delay : { -50, 0, 50 })
						for (int angle : { 0, 30 })
							for (int children : { -1, 0, 30 })
								for (bool endpoint : { false, true })
									for (bool enabled : { false, true }) {
										auto object = original;
										auto &p = object.render_data;
										p[1] = delay;
										p[3] = amplitude;
										p[9] = nodes;
										p[10] = children;
										p[13] = angle;
										compare(object, style, endpoint, enabled);
										++count;
									}
		for (unsigned field : { 0u, 1u, 2u, 3u, 4u }) {
			auto object = original;
			object.render_data[field] = std::numeric_limits<int>::min();
			bool rejected = false;
			try {
				const auto d = xl_compat::lightning_definition(object, 2);
				(void) xl_compat::lightning_lifetime(d, false, true);
			} catch (const std::exception &) {
				rejected = true;
			}
			require(rejected, "Unsafe lightning arithmetic was accepted");
		}
		for (const auto field : { std::array<int, 2>{ 9, 640 }, { 9, -32768 }, { 11, -32768 } }) {
			auto object = original;
			object.render_data[field[0]] = field[1];
			bool rejected = false;
			try {
				(void) xl_compat::lightning_lifetime(xl_compat::lightning_definition(object, 2), false, true);
			} catch (const std::exception &) {
				rejected = true;
			}
			require(rejected, "Lightning source field capacity overflow was accepted");
		}
		std::cout << "Pinned lightning: " << count << " authored setup profiles and lifetime/random scheduling controls passed\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
