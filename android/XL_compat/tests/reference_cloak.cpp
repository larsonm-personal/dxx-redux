#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using fix = int32_t;
#include "xl_reference_cloak_constants.h"
constexpr int ND_STATE_RECORDING = 1, ND_STATE_PLAYBACK = 2;
constexpr int CLOAKING_WALL_TIME = 65536;
constexpr int SOUND_WALL_CLOAK_ON = 0, SOUND_WALL_CLOAK_OFF = 1;
#define SHOW_DYN_LIGHT 0
#define IsMultiGame    0
#define COMPETITION    0
#define ENTER(a, b)
#define RETURN     return;
#define Assert     assert
#define IS_WALL(i) ((i) >= 0 && (i) < 2)
#define I2X(i)     ((i) * 65536)
#define X2F(i)     (float(i) / 65536.0f)
fix FixMul(fix a, fix b)
{
	return fix((int64_t(a) * b) >> 16);
}
fix FixDiv(fix a, fix b)
{
	return fix(int64_t(a) * 65536 / b);
}
struct CCloakingWall {
	int16_t nFrontWall = -1, nBackWall = -1;
	fix front_ls[4]{}, back_ls[4]{}, time = 0;
};
struct CloakArray {
	CCloakingWall data[MAX_CLOAKING_WALLS]{};
	unsigned count = 0;
	bool Grow()
	{
		if (count == MAX_CLOAKING_WALLS) return false;
		++count;
		return true;
	}
	CCloakingWall *Top()
	{
		return data + count - 1;
	}
	CCloakingWall *Buffer()
	{
		return data;
	}
	unsigned ToS() const
	{
		return count;
	}
	CCloakingWall *operator+(int i)
	{
		return data + i;
	}
	void Delete(unsigned i)
	{
		assert(i < count);
		if (i < --count) std::memmove(data + i, data + i + 1, (count - i) * sizeof(CCloakingWall));
	}
};
struct {
	struct {
		CloakArray cloaking;
	} wallData;
	struct {
		int nState = 0;
	} demoData;
	struct {
		fix xFrame = 0;
	} timeData;
} gameData;
struct {
	struct {
		bool bD2XLevel = true, bHaveExtraGameInfo[2]{};
	} app;
	struct {
		struct {
			float fSpeed = 1;
		} slowmo[1];
	} gameplay;
} gameStates;
struct Options {
	struct {
		struct {
			bool bWalls = true;
		} color;
		struct {
			bool bAutoTransparency = false;
		} effects;
	} render;
} options, *gameOpts = &options;
struct {
	int grWallTransparency = 0;
} extraGameInfo[2];
struct {
	void Update() {}
} audio;
struct {
	void Toggle(int, int, int, int) {}
} lightManager;
struct CWall {
	int nSegment = 0, nSide = 0, nType = 0, state = 0;
	int8_t cloakValue = 0;
	fix hps = 0;
	CCloakingWall *StartCloak();
	CCloakingWall *StartDecloak();
} walls[2];
#define WALLS walls
CWall *WALL(int i)
{
	return IS_WALL(i) ? walls + i : nullptr;
}
struct tUVL {
	fix l = 0;
};
struct CSegment {
	int index = 0, wall = 0, m_children[6]{};
	struct {
		tUVL m_uvls[4];
		int m_nBaseTex = 0;
	} m_sides[6];
	CWall *Wall(int)
	{
		return WALL(wall);
	}
	int WallNum(int)
	{
		return wall;
	}
	int ConnectedSide(CSegment *)
	{
		return 0;
	}
	void CreateSound(int, int) {}
	void StartCloak(int32_t);
	void StartDecloak(int32_t);
} segments[2];
CSegment *SEGMENT(int i)
{
	return i >= 0 && i < 2 ? segments + i : nullptr;
}
int SEG_IDX(CSegment *s)
{
	return s->index;
}
void NDRecordCloakingWall(int, int, int, int, int, fix, fix, fix, fix) {}
struct CFloatVector {
	struct {
		float values[4];
	} v;
	float &Red()
	{
		return v.values[0];
	}
	float &Green()
	{
		return v.values[1];
	}
	float &Blue()
	{
		return v.values[2];
	}
	float &Alpha()
	{
		return v.values[3];
	}
	float Max() const
	{
		return std::max({ v.values[0], v.values[1], v.values[2] });
	}
};
struct Palette {
	void ToRgbaf(uint8_t, CFloatVector &) {}
} palette;
struct {
	Palette *Game()
	{
		return &palette;
	}
} paletteManager;
int IsTransparentTexture(int)
{
	return 0;
}
#if defined(__GNUC__)
#pragma GCC diagnostic push
// Keep the extracted source's original indentation
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#include "xl_reference_cloak.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif

int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected native cloak samples");
		std::ifstream input(argv[1]);
		int operation, argument, back, initial, samples = 0;
		while (input >> operation >> argument >> back >> initial) {
			if (operation == 0) {
				gameData.wallData.cloaking = {};
				for (int i = 0; i < 2; ++i) {
					walls[i] = {};
					walls[i].nSegment = i;
					walls[i].nType = argument ? WALL_CLOSED : WALL_OPEN;
					walls[i].cloakValue = int8_t(initial);
					segments[i] = {};
					segments[i].index = i;
					segments[i].wall = i && !back ? -1 : i;
					segments[i].m_children[0] = 1 - i;
					for (int c = 0; c < 4; ++c) segments[i].m_sides[0].m_uvls[c].l = (i * 4 + c + 1) * 8192;
				}
				if (argument) segments[0].StartCloak(0);
				else segments[0].StartDecloak(0);
			} else if (operation == 1) {
				gameData.timeData.xFrame = argument;
				if (gameData.wallData.cloaking.count) {
					if (walls[0].state == WALL_DOOR_CLOAKING) DoCloakingWallFrame(0);
					else DoDecloakingWallFrame(0);
				}
			} else if (operation == 2) {
				if (argument) segments[0].StartCloak(0);
				else segments[0].StartDecloak(0);
			} else throw std::runtime_error("Unknown sample operation");
			int expected[16]{};
			for (int &value : expected)
				if (!(input >> value)) throw std::runtime_error("Truncated cloak sample");
			int actual[16] = { walls[0].nType, walls[0].state, walls[0].cloakValue, walls[1].nType, walls[1].state, walls[1].cloakValue,
				               int(gameData.wallData.cloaking.count), gameData.wallData.cloaking.count ? gameData.wallData.cloaking.data[0].time : -1 };
			for (int i = 0; i < 2; ++i)
				for (int c = 0; c < 4; ++c) actual[8 + i * 4 + c] = segments[i].m_sides[0].m_uvls[c].l;
			for (int field = 0; field < 16; ++field)
				if (actual[field] != expected[field]) throw std::runtime_error("Cloak mismatch sample " + std::to_string(samples) + " field " + std::to_string(field) + " expected " + std::to_string(actual[field]) + " actual " + std::to_string(expected[field]));
			int expected_flat;
			float expected_alpha;
			if (!(input >> expected_flat >> expected_alpha)) throw std::runtime_error("Missing native cloak alpha");
			CFloatVector color{};
			int32_t color_id = 0;
			uint8_t textured = 1, cloaked = 0, transparent = 0;
			const float alpha = WallAlpha(0, 0, 0, walls[0].nType == WALL_CLOAKED ? WID_CLOAKED_FLAG : 0, 0, 0, &color, color_id, textured, cloaked, transparent);
			if (expected_flat != int(cloaked) || (cloaked && std::fabs(expected_alpha - alpha) > 0.000001f)) throw std::runtime_error("Cloak draw mismatch at " + std::to_string(samples));
			++samples;
		}
		if (samples < 1000) throw std::runtime_error("Incomplete cloak sample stream");
		std::cout << samples << " extracted cloak caller/frame/alpha comparisons passed\n";
		return 0;
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
