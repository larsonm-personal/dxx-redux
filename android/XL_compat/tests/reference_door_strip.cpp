#include <cstdint>
#include <iostream>
#include <stdexcept>
#include "../door_strip_phase.h"
#include "xl_reference_door_strip_flags.h"

namespace
{
// GPU uploads and resource lookup are adapters; the generated header contains
// the actual source selection, SetTexture, initialization and authored mutation
#define ENTER(a, b)
#define RETURN    return;
#define RETVAL(x) return (x);
constexpr int BM_TYPE_ALT = 1;
constexpr int ND_STATE_RECORDING = 1, WALL_DOOR = 2;
struct CBitmap {
	CBitmap *replacement = nullptr, *frames = nullptr, *current = nullptr;
	int count = 0, type = 0, wall = 0;
	CBitmap *Override()
	{
		return replacement;
	}
	int Type()
	{
		return type;
	}
	int FrameCount()
	{
		return count;
	}
	CBitmap *Frames()
	{
		return frames;
	}
	CBitmap *CurFrame()
	{
		return current;
	}
	void SetCurFrame(CBitmap *value)
	{
		current = value;
	}
	void SetWallAnim(int value)
	{
		wall = value;
	}
	int WallAnim()
	{
		return wall;
	}
	void SetTranspType(int) {}
	void SetupTexture(int, int) {}
	void SetStatic(int) {}
} bitmaps[64], strip, frame_views[128];
struct tWallEffect {
	int16_t frames[64]{};
	int nFrameCount = 0, flags = 0;
};
struct {
	struct {
		struct {
			CBitmap *pBitmap = bitmaps;
			struct {
				int index = 0;
			} pBmIndex[64];
		} tex;
	} pigData;
	struct {
		tWallEffect pAnim[2];
		int nWalls = 0;
	} wallData;
	struct {
		int nState = 0;
	} demoData;
} gameData;
struct {
	struct {
		bool bD1Mission = false;
	} app;
} gameStates;
struct Options {
	struct {
		bool bGlTexMerge = true;
	} ogl;
} options;
Options *gameOpts = &options;
struct CSide {
	int m_nBaseTex = 40, m_nOvlTex = 41, m_nFrame = 0;
	void SetTextures(int32_t, int32_t);
};
struct CSegment {
	CSide m_sides[6];
	void SetTexture(int32_t, CSegment *, int16_t, int32_t, int32_t);
} segments[2];
struct CWall {
	int nType = WALL_DOOR, nClip = 0, nSegment = 0, nSide = 0;
} walls[2];
struct WallArray {
	CWall *Buffer()
	{
		return walls;
	}
} WALLS;
CSegment *SEGMENT(int i)
{
	return &segments[i];
}
int SEG_IDX(CSegment *s)
{
	return static_cast<int>(s - segments);
}
void LoadTexture(int, int, bool) {}
void NDRecordWallSetTMapNum1(int, uint8_t, int16_t, uint8_t, int16_t, int, int) {}
void NDRecordWallSetTMapNum2(int, uint8_t, int16_t, uint8_t, int16_t, int, int) {}
CBitmap *SetupHiresAnim(int16_t *, int, int, int, int, int *count)
{
	*count = strip.count;
	return &strip;
}
#include "xl_reference_door_strip.h"
void require(bool value, const char *message)
{
	if (!value) throw std::runtime_error(message);
}
} // namespace

void door_strip_reference()
{
	int samples = 0;
	for (int i = 0; i < 64; ++i) gameData.pigData.tex.pBmIndex[i].index = i;
	auto &clip = gameData.wallData.pAnim[0];
	for (int i = 0; i < 64; ++i) clip.frames[i] = static_cast<int16_t>(i);
	strip.type = BM_TYPE_ALT;
	strip.frames = frame_views;
	bitmaps[0].replacement = &strip;
	for (int native_count : { 1, 2, 8, 50 })
		for (int count : { 0, 1, 2, 3, 16, 64, 128 }) {
			clip.nFrameCount = native_count;
			strip.count = count;
			require(WallEffectFrameCount(&clip) == xl_compat::door_effect_frame_count(native_count, count), "Door strip source frame count differs");
			++samples;
		}
	bitmaps[0].replacement = nullptr;
	require(WallEffectFrameCount(&clip) == clip.nFrameCount, "Missing override changed source frame count");
	require(LoadFaceBitmap(0, -3, 1) == bitmaps, "Missing override changed source face selection");
	bitmaps[0].replacement = &strip;
	strip.type = 0;
	require(WallEffectFrameCount(&clip) == clip.nFrameCount, "Ordinary override changed source frame count");
	strip.type = BM_TYPE_ALT;
	strip.wall = 0;
	require(LoadFaceBitmap(0, -3, 1) == &strip, "Nonwall strip selected a door frame");
	strip.wall = 1;
	strip.count = 1;
	require(LoadFaceBitmap(0, -1, 1) == &strip, "One-frame strip selected a door frame");
	strip.count = 2;
	strip.frames = nullptr;
	require(LoadFaceBitmap(0, -2, 1) == &strip, "Missing frame storage did not preserve source parent");
	strip.frames = frame_views;
	samples += 6;
	clip.nFrameCount = 8;
	for (int count : { 2, 3, 16, 64, 128 }) {
		strip.count = count;
		strip.wall = 1;
		for (int phase = -130; phase <= 2; ++phase) {
			require(LoadFaceBitmap(0, static_cast<int16_t>(phase), 1) == frame_views + xl_compat::door_strip_frame(phase, count), "Door strip source face selection differs");
			++samples;
		}
		for (bool base : { false, true }) {
			clip.flags = WCF_ALTFMT | (base ? WCF_TMAP1 : 0);
			gameData.wallData.nWalls = 2;
			walls[1].nSegment = 1;
			segments[0] = {};
			segments[1] = {};
			InitDoorAnims();
			require(segments[0].m_sides[0].m_nFrame == xl_compat::door_strip_initial_phase(base), "Door strip source initial phase differs");
			for (int frame = 0; frame < count; ++frame) {
				const int other_phase = segments[1].m_sides[0].m_nFrame;
				segments[0].SetTexture(0, nullptr, -1, 0, frame);
				auto &side = segments[0].m_sides[0];
				require(side.m_nFrame == xl_compat::door_texture_phase(frame, count), "Door strip source SetTexture phase differs");
				require(side.m_nBaseTex == 40 && side.m_nOvlTex == 41, "Alternate door source unexpectedly changes texture mapping");
				require(segments[1].m_sides[0].m_nFrame == other_phase, "Independent source door phase changed");
				// A later draw of another side mutates the source bitmap's current
				// pointer, but the first side must still select its own frame
				LoadFaceBitmap(0, static_cast<int16_t>(other_phase), 1);
				require(LoadFaceBitmap(0, static_cast<int16_t>(side.m_nFrame), 1) == frame_views + frame, "Shared image lost independent side phase");
				side.SetTextures(42, 43);
				require(side.m_nFrame == xl_compat::door_texture_phase(frame, count), "Authored source texture change reset phase");
				side.SetTextures(40, 41);
				segments[0].SetTexture(0, &segments[1], 0, 0, frame);
				require(segments[1].m_sides[0].m_nFrame == side.m_nFrame, "Connected source side phase differs");
				++samples;
			}
		}
	}
	clip.flags = 0;
	for (int frame = 0; frame < clip.nFrameCount; ++frame) {
		segments[0].SetTexture(0, nullptr, -1, 0, frame);
		require(segments[0].m_sides[0].m_nFrame == xl_compat::door_texture_phase(frame, 0), "Ordinary source texture phase differs");
		++samples;
	}
	std::cout << samples << " extracted-reference door strip selection and phase comparisons passed\n";
}
