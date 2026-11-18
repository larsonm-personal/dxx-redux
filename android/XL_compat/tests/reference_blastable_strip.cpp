#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
#include "../door_strip_phase.h"

namespace
{
// Geometry, GPU and multiplayer are adapters; damage and destruction are extracted source
#define ENTER(a, b)
#define RETURN      return;
#define IS_WALL(n)  ((n) >= 0 && (n) < 2)
#define IsMultiGame false
using fix = int32_t;
constexpr int WALL_BLASTABLE = 1, WALL_BLASTED = 1, WALL_HPS = 100 * 65536;
constexpr int WCF_EXPLODES = 1, BM_TYPE_ALT = 1, NO_WALL = -1;
constexpr int TXT_BLAST_SINGLE = 0, TXT_DMG_SINGLE = 0;
constexpr int EXPL_WALL_TIME = 65536, EXPL_WALL_TOTAL_FIREBALLS = 32, EXPL_WALL_FIREBALL_SIZE = 0x48000 * 6 / 10;
constexpr int ANIM_SMALL_EXPLOSION = 0;
int FixDiv(int a, int b)
{
	return int((int64_t(a) * 65536) / b);
}
int FixMul(int a, int b)
{
	return int((int64_t(a) * b) / 65536);
}
int X2I(int a)
{
	return a / 65536;
}
int I2X(int a)
{
	return a * 65536;
}
struct CFixVector {
	CFixVector operator-(const CFixVector &) const
	{
		return {};
	}
	CFixVector operator+(const CFixVector &) const
	{
		return {};
	}
	CFixVector operator*(int) const
	{
		return {};
	}
	CFixVector &operator+=(const CFixVector &)
	{
		return *this;
	}
} reference_vertices[4];
struct Explosion {
	int nSegment = 0, nSide = 0, time = 0;
};
struct Explosions {
	Explosion entry;
	unsigned count = 0;
	unsigned ToS()
	{
		return count;
	}
	Explosion &operator[](unsigned)
	{
		return entry;
	}
};
struct CBitmap {
	int frames = 16;
	CBitmap *Override()
	{
		return nullptr;
	}
	int Type()
	{
		return BM_TYPE_ALT;
	}
	int FrameCount()
	{
		return frames;
	}
} bitmap;
struct tWallEffect {
	int frames[64]{}, nFrameCount = 8, flags = 0;
} animation;
struct {
	struct {
		struct {
			CBitmap *pBitmap = &bitmap;
			struct {
				int index = 0;
			} pBmIndex[64];
		} tex;
	} pigData;
	struct {
		tWallEffect *pAnim = &animation;
		Explosions exploding;
	} wallData;
	struct {
		int xFrame = 0;
	} timeData;
	struct {
		CFixVector *vertices = reference_vertices;
	} segData;
} gameData;
struct Options {
	struct {
		bool bWalls = false;
	} legacy;
} options;
Options *gameOpts = &options;
struct CWall {
	int hps = WALL_HPS, nType = WALL_BLASTABLE, nClip = 0, flags = 0;
} walls[2];
CWall *WALLS = walls;
CWall *WALL(int n)
{
	return &walls[n];
}
struct CSegment {
	int index = 0, m_children[6]{}, phase = 0;
	struct {
		CFixVector m_normals[2];
	} m_sides[6];
	uint16_t *Corners(int)
	{
		static uint16_t corners[4] = { 0, 1, 2, 3 };
		return corners;
	}
	CWall *Wall(int)
	{
		return &walls[index];
	}
	int WallNum(int)
	{
		return index;
	}
	int ConnectedSide(CSegment *)
	{
		return 0;
	}
	int Index()
	{
		return index;
	}
	void SetTexture(int, CSegment *other, int, int, int frame)
	{
		phase = xl_compat::door_texture_phase(frame, bitmap.frames);
		if (other) other->phase = phase;
	}
	void BlastWall(int32_t);
	void DamageWall(int32_t, fix);
} segments[2];
CSegment *SEGMENTS = segments;
CSegment *SEGMENT(int n)
{
	return &segments[n];
}
int SEG_IDX(CSegment *p)
{
	return p->index;
}
void KillStuckObjects(int) {}
void Warning(int, ...) {}
void Assert(bool value)
{
	if (!value) throw std::runtime_error("Source connection invalid");
}
void ExplodeWall(int, int)
{
	gameData.wallData.exploding = {};
	gameData.wallData.exploding.count = 1;
}
void DeleteExplodingWall(unsigned)
{
	gameData.wallData.exploding.count = 0;
}
int random_calls = 0, normal_fireballs = 0, damaging_fireballs = 0;
int RandShort()
{
	++random_calls;
	return 0;
}
void CreateExplosion(int16_t, CFixVector, int, uint8_t)
{
	++normal_fireballs;
}
void CreateSplashDamageExplosion(void *, int16_t, CFixVector, CFixVector, int, uint8_t, int, int, int, int)
{
	++damaging_fireballs;
}
void MultiSendDoorOpen(int, int, int) {}
#include "xl_reference_blastable_strip.h"
} // namespace

void blastable_strip_reference(const char *path)
{
	std::ifstream input(path);
	int damage, before, before_back, phase, phase_back, flags, flags_back;
	int expected_hp, expected_back_hp, expected_phase, expected_back_phase, expected_flags, expected_back_flags;
	int samples = 0;
	segments[1].index = 1;
	segments[0].m_children[0] = 1;
	while (input >> damage >> before >> before_back >> phase >> phase_back >> flags >> flags_back >>
	       expected_hp >> expected_back_hp >> expected_phase >> expected_back_phase >> expected_flags >> expected_back_flags) {
		walls[0].hps = before;
		walls[1].hps = before_back;
		walls[0].flags = flags;
		walls[1].flags = flags_back;
		segments[0].phase = phase;
		segments[1].phase = phase_back;
		segments[0].DamageWall(0, damage);
		if (walls[0].hps != expected_hp || walls[1].hps != expected_back_hp || segments[0].phase != expected_phase ||
		    segments[1].phase != expected_back_phase || walls[0].flags != expected_flags || walls[1].flags != expected_back_flags)
			throw std::runtime_error("Blastable strip source mismatch at " + std::to_string(samples));
		++samples;
	}
	if (!input.eof() || samples != 32) throw std::runtime_error("Incomplete blastable strip samples");
	std::cout << samples << " extracted-reference blastable strip damage, phase and collision-flag comparisons passed\n";
}

void exploding_strip_reference(const char *path)
{
	std::ifstream input(path);
	segments[1].index = 1;
	segments[0].m_children[0] = 1;
	int elapsed, before, phase, flags, expected_time, active, expected_phase, expected_flags, expected_objects;
	int samples = 0;
	animation.flags = WCF_EXPLODES;
	walls[0] = walls[1] = {};
	segments[0].phase = segments[1].phase = 0;
	segments[0].DamageWall(0, WALL_HPS);
	if (gameData.wallData.exploding.count != 1 || walls[0].hps != -1 || walls[0].flags || segments[0].phase)
		throw std::runtime_error("Extracted source did not register delayed strip explosion");
	while (input >> elapsed >> before >> phase >> flags >> expected_time >> active >> expected_phase >> expected_flags >> expected_objects) {
		gameData.wallData.exploding.entry = { 0, 0, before };
		gameData.wallData.exploding.count = 1;
		gameData.timeData.xFrame = elapsed;
		segments[0].phase = segments[1].phase = phase;
		walls[0].flags = walls[1].flags = flags;
		random_calls = normal_fireballs = damaging_fireballs = 0;
		DoExplodingWallFrame();
		if (gameData.wallData.exploding.entry.time != expected_time || int(gameData.wallData.exploding.count) != active ||
		    segments[0].phase != expected_phase || segments[1].phase != expected_phase || walls[0].flags != expected_flags ||
		    walls[1].flags != expected_flags || normal_fireballs + damaging_fireballs != expected_objects || random_calls != expected_objects * 2)
			throw std::runtime_error("Exploding strip source clock/frame/fireball-count mismatch at " + std::to_string(samples));
		++samples;
	}
	if (!input.eof() || samples != 4) throw std::runtime_error("Incomplete exploding strip samples");
	std::cout << samples << " extracted-reference exploding strip timer, frame, flag and fireball-count comparisons passed\n";
}
