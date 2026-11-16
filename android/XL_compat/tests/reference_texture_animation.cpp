#include <algorithm>
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include "../xl_texture_frame.h"
#include "../xl_vclip_frame.h"
#include "../fireball_material.h"

namespace texture_animation_reference
{
using fix = int32_t;
constexpr int BM_TYPE_STD = 0, BM_TYPE_ALT = 1, BM_TYPE_FRAME = 2;
template <typename T>
T Min(T a, T b)
{
	return std::min(a, b);
}

// Storage and GL adapters only; selection and frame-count logic is extracted
struct CBitmap {
	int type = BM_TYPE_STD, count = 1, key = 0;
	bool wall = false;
	CBitmap *replacement = nullptr, *frames = nullptr, *current = nullptr;
	CBitmap *Parent() const
	{
		return nullptr;
	}
	int Type() const
	{
		return type;
	}
	int FrameCount() const
	{
		return count;
	}
	CBitmap *Frames() const
	{
		return frames;
	}
	CBitmap *Override() const
	{
		return replacement;
	}
	bool WallAnim() const
	{
		return wall;
	}
	void SetOverride(CBitmap *value)
	{
		replacement = value;
	}
	CBitmap *SetCurFrame(CBitmap *value)
	{
		return current = value;
	}
	void SetKey(int value)
	{
		key = value;
	}
	void FreeHiresAnimation(int)
	{
		replacement = nullptr;
	}
	void Unload(int, int)
	{
		replacement = nullptr;
	}
	void SetTranspType(int) {}
	void SetStatic(int) {}
	void SetupTexture(int, int) {}
};
template <typename T>
struct Bank : std::array<T, 64> {
	T &operator[](int index)
	{
		return this->at(std::size_t(index));
	}
	T *Buffer()
	{
		return this->data();
	}
	operator T *()
	{
		return this->data();
	}
};
struct BitmapIndex {
	int16_t index = 0;
};
struct tAnimationInfo {
	int nFrameCount = 0, flags = 0;
	fix xTotalTime = 0, xFrameTime = 0;
	BitmapIndex frames[64];
};
struct tAnimationState {
	int nCurFrame = 0;
	fix xTotalTime = 0, xFrameTime = 0;
};
struct CObject {
	struct {
		int renderType = 0, nType = 0;
	} info;
	struct {
		struct {
			int flags = 0;
		} physInfo;
	} mType;
	bool mine = false;
	bool IsMine() const
	{
		return mine;
	}
	struct {
		tAnimationState animationInfo;
	} rType;
};
constexpr int WCF_ALTFMT = 1, WCF_INITIALIZED = 2, ANIM_AFTERBURNER_BLOB = 5;
int random_calls = 0;
int Rand(int frames)
{
	++random_calls;
	return frames - 1;
}
struct {
	struct {
		Bank<tAnimationInfo> animations[2];
	} effectData;
	struct {
		struct {
			Bank<CBitmap> bitmaps[2];
			Bank<CBitmap> &pBitmap = bitmaps[0];
			Bank<BitmapIndex> pBmIndex, objBmIndex;
		} tex;
	} pigData;
} gameData;
struct {
	struct {
		int bD1Data = 0, bD1Mission = 0;
	} app;
	struct {
		struct {
			double fSpeed = 1;
		} slowmo[1];
	} gameplay;
} gameStates;
struct Options {
	struct {
		bool bGlTexMerge = true;
	} ogl;
} options, *gameOpts = &options;
struct tEffectInfo {
	struct {
		fix xFrameTime = 6144;
		int nFrameCount = 8;
		BitmapIndex frames[64];
	} animationInfo;
	struct {
		int nWallTexture = 0, nObjectTexture = -1;
	} changing;
};
void LoadTexture(int, int, int) {}
#define ENTER(...)
#define RETVAL(x)                        return (x);
#define BM_INDEX(p, i, indirect, object) ((indirect) ? ((object) ? gameData.pigData.tex.objBmIndex[(p)[i]].index : gameData.pigData.tex.pBmIndex[(p)[i]].index) : (p)[i])
#define DBG                              0
namespace release
{
#include "xl_reference_texture_time.h"
}
#include "xl_reference_texture_animation.h"
#include "xl_reference_vclip_animation.h"
constexpr int RT_THRUSTER = 8, PF_WIGGLE = 32, OBJ_FIREBALL = 1, OBJ_EXPLOSION = 2, OBJ_WEAPON = 3, VF_ROD = 4;
struct CFloatVector {
};
struct DrawObservation {
	int frame = -1, bitmap = -1, rod = 0;
	float alpha = -1;
} drawn;
int32_t SetupHiresVClip(tAnimationInfo *clip, tAnimationState *state)
{
	return SetupHiresVClip(clip, state, nullptr);
}
void DrawObjectRodTexPoly(CObject *, BitmapIndex bitmap, int, int frame)
{
	drawn = { frame, bitmap.index, 1, -1 };
}
void DrawObjectBitmap(CObject *, int, int bitmap, int frame, CFloatVector *, float alpha)
{
	drawn = { frame, bitmap, 0, alpha };
}
struct {
	void SetDepthWrite(bool) {}
} ogl;
#define RETURN return;
#include "xl_reference_vclip_draw.h"
#undef RETURN
#undef DBG
#define DBG 1
namespace debug
{
#include "xl_reference_texture_time.h"
}
#undef DBG
#undef BM_INDEX
#undef RETVAL
#undef ENTER

void check(bool condition, const char *message)
{
	if (!condition) throw std::runtime_error(message);
}
} // namespace texture_animation_reference

void texture_animation_source_controls()
{
	using namespace texture_animation_reference;
	unsigned flag_cases = 0;
	for (unsigned frame = 0; frame < 128; ++frame)
		for (unsigned flags = 0; flags < 4; ++flags) {
			struct {
				unsigned transparentFrames[4]{}, supertranspFrames[4]{};
			} m_info;
			if (flags & 1) m_info.transparentFrames[frame / 32] = 1u << (frame % 32);
			if (flags & 2) m_info.supertranspFrames[frame / 32] = 1u << (frame % 32);
			const unsigned i = frame;
			unsigned nFlags;
			constexpr unsigned BM_FLAG_TGA = 128, BM_FLAG_TRANSPARENT = 1, BM_FLAG_SUPER_TRANSPARENT = 2;
#include "xl_reference_texture_frame_flags.h"
			check(dxx_xl_texture_frame_flags(m_info.transparentFrames, m_info.supertranspFrames, frame) == (nFlags & ~BM_FLAG_TGA),
			      "Native child frame flags differ from pinned CreateFrames");
			++flag_cases;
		}
	unsigned cases = 0;
	unsigned vclip_cases = 0;
	unsigned afterburner_cases = 0;
	unsigned draw_cases = 0;
	auto &draw_clip = gameData.effectData.animations[0][4];
	draw_clip.xTotalTime = 65536;
	draw_clip.xFrameTime = 1024;
	for (int i = 0; i < 64; ++i) draw_clip.frames[i].index = int16_t(i + 100);
	for (int frames : { 5, 17, 30, 64 })
		for (int frame = -1; frame <= frames; ++frame)
			for (int type : { OBJ_FIREBALL, OBJ_EXPLOSION, OBJ_WEAPON })
				for (bool rod : { false, true })
					for (int render : { 2, RT_THRUSTER }) {
						draw_clip.nFrameCount = frames;
						draw_clip.flags = rod ? VF_ROD : 0;
						CObject object;
						object.info.nType = type;
						object.info.renderType = render;
						// Negative CurFrame requires an explicit object clock; ordinary clocks wrap
						const int remaining = frame < 0 ? draw_clip.xTotalTime : draw_clip.xTotalTime - frame * draw_clip.xFrameTime;
						const int selected = CurFrame(&object, 4, remaining, frames);
						drawn = {};
						DrawVClipObject(&object, remaining, 0, 4, nullptr);
						if (type == OBJ_FIREBALL && !rod) {
							const auto material = xl_compat::fireball_material(selected, frames);
							check(material.visible == (drawn.frame >= 0) && (!material.visible || material.color[3] == drawn.alpha),
							      "Native fireball material differs from pinned draw policy");
						}
						if (selected >= 30) check(drawn.frame == -1, "Pinned vclip draw did not apply its 30-frame visibility bound");
						else {
							check(drawn.frame == selected && drawn.bitmap == selected + 100 && drawn.rod == int(rod), "Pinned vclip draw selection differs");
							if (!rod) {
								const float alpha = type == OBJ_WEAPON ? .7f : float(.8 - double(selected) / frames * .8);
								check(drawn.alpha == alpha, "Pinned vclip material alpha differs");
							}
						}
						++draw_cases;
					}
	std::cout << "Pinned vclip draw frame/alpha policy: " << draw_cases << " controls\n";
	auto &afterburner = gameData.effectData.animations[0][ANIM_AFTERBURNER_BLOB];
	afterburner.xTotalTime = 65537;
	afterburner.xFrameTime = 4097;
	afterburner.nFrameCount = 16;
	for (int lifetime : { 0, 1, 16385, 32769, 65537, 98307, 131075 }) {
		CObject object;
		object.rType.animationInfo.xTotalTime = lifetime;
		object.rType.animationInfo.xFrameTime = int(int64_t(afterburner.xFrameTime) * lifetime / afterburner.xTotalTime);
		for (int frames : { 2, 16, 30, 128 })
			for (int remaining = -1; remaining <= 2 * afterburner.xTotalTime; remaining += 257) {
				check(dxx_xl_vclip_object_frame(afterburner.xTotalTime, afterburner.xFrameTime, frames, remaining,
				                                object.rType.animationInfo.xTotalTime, object.rType.animationInfo.xFrameTime) ==
				          CurFrame(&object, ANIM_AFTERBURNER_BLOB, remaining, frames),
				      "Afterburner object lifetime frame differs from pinned source");
				++afterburner_cases;
			}
	}
	std::cout << "Pinned afterburner object lifetime frames: " << afterburner_cases << " controls\n";
	// Cache-time setup initializes shared strips without consuming object RNG
	for (int native_frames : { 4, 17, 30 }) {
		for (int replacement_frames : { 2, 17, 30 }) {
			gameData.pigData.tex.bitmaps[0] = {};
			gameOpts->ogl.bGlTexMerge = true;
			CBitmap strip;
			strip.type = BM_TYPE_ALT;
			strip.count = replacement_frames;
			auto &clip = gameData.effectData.animations[0][0];
			clip = {};
			clip.flags = WCF_ALTFMT;
			clip.nFrameCount = native_frames;
			clip.xFrameTime = 4096;
			clip.xTotalTime = native_frames * clip.xFrameTime;
			for (int i = 0; i < native_frames; ++i) clip.frames[i].index = int16_t(i + 1);
			gameData.pigData.tex.bitmaps[0][1].replacement = &strip;
			random_calls = 0;
			check(SetupHiresVClip(&clip, nullptr, &strip) == replacement_frames, "Cached vclip count differs");
			check(random_calls == 0 && (clip.flags & WCF_INITIALIZED), "Cached vclip consumes RNG or remains uninitialized");
			CObject object;
			object.rType.animationInfo.nCurFrame = 3;
			check(SetupHiresVClip(&clip, &object.rType.animationInfo, nullptr) == replacement_frames &&
			          random_calls == 0 && object.rType.animationInfo.nCurFrame == 3,
			      "Cached vclip resets object phase or RNG");
			for (int i = 0; i < native_frames; ++i)
				check(gameData.pigData.tex.bitmaps[0][i + 1].Override() == &strip, "Vclip aliases do not share selected strip");
			for (int time = 0; time <= clip.xTotalTime * 2; time += 1024) {
				const int wrapped = time > clip.xTotalTime ? time % clip.xTotalTime : time;
				const int expected = std::min((clip.xTotalTime - wrapped) / clip.xFrameTime, replacement_frames - 1);
				check(CurFrame(&object, 0, time, replacement_frames) == expected, "Vclip lifetime frame differs");
				check(dxx_xl_vclip_lifetime_frame(clip.xTotalTime, clip.xFrameTime, replacement_frames, time) ==
				          CurFrame(&object, 0, time, replacement_frames),
				      "Native vclip lifetime selection differs from source");
				++vclip_cases;
			}
			// Without cache preparation the first object setup consumes exactly one draw
			clip.flags = WCF_ALTFMT;
			check(SetupHiresVClip(&clip, &object.rType.animationInfo, &strip) == replacement_frames && random_calls == 1,
			      "Uncached vclip initialization RNG control differs");
		}
	}
	for (int stock_count : { 4, 8, 10 }) {
		for (int replacement_count : { 2, 4, 6, 12 }) {
			for (bool merge : { false, true }) {
				gameData.pigData.tex.bitmaps[0] = {};
				gameOpts->ogl.bGlTexMerge = merge;
				std::array<CBitmap, 12> children;
				CBitmap replacement;
				replacement.type = BM_TYPE_ALT;
				replacement.count = replacement_count;
				replacement.frames = children.data();
				int16_t ids[10];
				for (int i = 0; i < stock_count; ++i) ids[i] = int16_t(i + 1);
				auto *base = gameData.pigData.tex.pBitmap.Buffer();
				base[1].replacement = &replacement;
				int effective_count = stock_count;
				check(SetupHiresAnim(ids, stock_count, 0, 0, 0, &effective_count, nullptr) == &replacement,
				      "Source did not find the authored strip override");
				check(effective_count == (merge ? replacement_count : stock_count), "Source effective animation count differs");
				for (int i = 0; i < stock_count; ++i) {
					CBitmap *expected = merge ? &replacement : i < std::min(stock_count, replacement_count) ? &children[i * std::max(1, replacement_count / stock_count)]
					                                                                                        : nullptr;
					check(base[i + 1].Override() == expected, "Source replacement frame binding differs");
				}
				tEffectInfo effect;
				effect.animationInfo.frames[0].index = 1;
				effect.animationInfo.nFrameCount = stock_count;
				for (double speed : { 1.0, 2.0 }) {
					gameStates.gameplay.slowmo[0].fSpeed = speed;
					check(release::EffectFrameTime(&effect) == fix(6144 / speed), "STD base with ALT override changed source frame period");
					check(debug::EffectFrameTime(&effect) == 6144, "Debug source frame period differs");
					base[1].type = BM_TYPE_ALT;
					base[1].frames = children.data();
					base[1].count = replacement_count;
					check(release::EffectFrameTime(&effect) == fix((6144 * stock_count / replacement_count) / speed), "ALT base ratio control differs");
					base[1].type = BM_TYPE_STD;
				}
				++cases;
			}
		}
	}
	// Physics returns a whole unclocked strip, even after effect setup
	std::array<CBitmap, 4> children;
	CBitmap strip;
	strip.type = BM_TYPE_ALT;
	strip.count = 4;
	strip.frames = children.data();
	gameData.pigData.tex.pBmIndex[0].index = 1;
	gameData.pigData.tex.pBitmap[1].replacement = &strip;
	for (int frame : { -6, -4, -2, -1, 0, 1, 4 }) {
		strip.wall = false;
		check(LoadFaceBitmap(0, int16_t(frame), 0) == &strip, "Unclocked strip did not retain whole-image physics view");
		strip.wall = true;
		const int selected = frame < 0 && frame >= -4 ? -frame - 1 : 0;
		check(LoadFaceBitmap(0, int16_t(frame), 0) == &children[selected], "Wall strip frame selection differs");
		++cases;
	}
	std::cout << "Pinned texture animation setup/timing/physics: " << cases << " profiles passed\n";
	std::cout << "Pinned child-frame transparency flags: " << flag_cases << " controls passed\n";
	std::cout << "Pinned vclip cache/RNG/lifetime selection: " << vclip_cases << " phases passed\n";
}
