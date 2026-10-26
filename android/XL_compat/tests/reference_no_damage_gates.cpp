// State adapter only: the two gate implementations are extracted unchanged
#include <cstdint>
#include <iostream>
#include <cstring>
#include <cmath>
#include "inventory.h"
using fix = int32_t;
constexpr fix I2X(int value)
{
	return value * 65536;
}
constexpr bool OBSERVING = false, IsMultiGame = false;
struct Segment {
	bool protected_region = false;
	bool HasNoDamageProp() const
	{
		return protected_region;
	}
} segment;
Segment *SEGMENT(int)
{
	return &segment;
}
struct Object {
	struct {
		int nSegment = 0;
	} info;
} console;
struct {
	struct {
		bool bPlayerIsDead = false, bD2XLevel = true;
	} app;
	struct {
		struct {
			float fSpeed = 1;
		} slowmo[1];
	} gameplay;
	struct {
		bool bColored = false;
	} render;
} gameStates;
struct {
	struct {
		Object *pConsole = &console;
	} objData;
	struct {
		fix xGame = I2X(100);
	} timeData;
	struct {
		fix xLastFiredTime = 0, xNextFireTime = 0;
	} laserData;
	struct {
		int nGlobalFiringCount = 0;
		fix xLastFiredTime = 0, xNextFireTime = 0;
	} missileData;
	struct {
		struct {
			fix xMslFireTime = 0;
		} weaponStates[1];
	} multiplayer;
} gameData;
// Bump/audio sinks observe gate dispatch, not the impulse or mixer implementations
struct CFixVector {
};
struct CObject {
	struct {
		int nSegment = 0, nFlags = 0;
	} info;
	int32_t CollidePlayerAndPlayer(CObject *, CFixVector &, CFixVector *);
	int32_t CollideActorAndClutter(CObject *, CFixVector &, CFixVector *);
};
int bumps = 0, sounds = 0;
bool BumpTwoObjects(CObject *, CObject *, int, CFixVector &)
{
	++bumps;
	return true;
}
struct {
	void CreateSegmentSound(int, int, int, CFixVector &)
	{
		++sounds;
	}
} audio;
constexpr int OF_EXPLODING = 1, SOUND_ROBOT_HIT_PLAYER = 0;
constexpr int OBJ_PLAYER = 0, OBJ_ROBOT = 1, OBJ_DEBRIS = 2, RESULT_CHECK = 1, RESULT_NOTHING = 0;
int collisions[3][3] = {};
struct Options {
	struct {
		int nDebrisLife = 0;
	} render;
} options;
Options *gameOpts = &options;
#define ENTER(a, b)
#define RETVAL(value)              return value;
#define SET_COLLISION(a, b, value) collisions[a][b] = collisions[b][a] = value
struct tRgbColord {
	double red, green, blue;
};
struct CFaceColor {
	float channels[4]{};
	int index = 0;
	float &Red()
	{
		return channels[0];
	}
	float &Green()
	{
		return channels[1];
	}
	float &Blue()
	{
		return channels[2];
	}
	float &Alpha()
	{
		return channels[3];
	}
};
struct CFile {
	double value;
	int ReadByte()
	{
		return 0;
	}
	int32_t ReadInt()
	{
		return static_cast<int32_t>(std::llround(value * 2147483647.0));
	}
	void Read(void *out, std::size_t, int)
	{
		const tRgbColord rgb{ value, value, value };
		std::memcpy(out, &rgb, sizeof(rgb));
	}
};
#include "xl_reference_no_damage_gates.h"

int main()
{
	for (int floating : { 0, 1 })
		for (int registered : { 0, 1 })
			for (double channel : { 0.0, 0.5, 1.0 - 1e-10, 1.0 }) {
				gameStates.render.bColored = false;
				CFile file{ channel };
				CFaceColor color;
				ReadColor(file, &color, floating, registered);
				xl_compat::Inventory level;
				const double decoded = floating ? channel : file.ReadInt() / 2147483647.0;
				auto &table = registered ? level.vertex_colors : level.texture_colors;
				table.push_back({ 0, { decoded, decoded, decoded } });
				if (xl_compat::has_registered_colors(level) != gameStates.render.bColored) return 2;
			}
	std::cout << "Pinned ReadColor registration matches 16 float/integer, registered/texture and boundary controls\n";

	for (int protected_region : { 0, 1 })
		for (int dead : { 0, 1 })
			for (int offset : { -1, 0, 1 }) {
				segment.protected_region = protected_region != 0;
				gameStates.app.bPlayerIsDead = dead != 0;
				gameData.laserData.xNextFireTime = gameData.missileData.xNextFireTime = gameData.timeData.xGame + I2X(offset);
				gameData.missileData.nGlobalFiringCount = 3;
				const int gun = AllowedToFireGun();
				const int missile = AllowedToFireMissile(-1, 1);
				std::cout << "ND_GATE " << protected_region << ' ' << dead << ' ' << offset << ' '
				          << gun << ' ' << missile << ' ' << gameData.missileData.nGlobalFiringCount << '\n';
			}
	for (int type : { 0, 1 })
		for (int protected_region : { 0, 1 }) {
			segment.protected_region = protected_region != 0;
			CObject receiver, other;
			CFixVector hit;
			bumps = sounds = 0;
			if (type == 0) receiver.CollideActorAndClutter(&other, hit, nullptr);
			else receiver.CollidePlayerAndPlayer(&other, hit, nullptr);
			std::cout << "ND_CONTACT " << type << ' ' << protected_region << ' ' << bumps << ' ' << sounds << '\n';
		}
	for (int lifetime : { 0, 1 }) {
		options.render.nDebrisLife = lifetime;
		SetDebrisCollisions();
		if (collisions[OBJ_PLAYER][OBJ_DEBRIS] != lifetime || collisions[OBJ_ROBOT][OBJ_DEBRIS] != lifetime) return 1;
	}
	std::cout << "Reference debris contacts depend on optional persistent debris; default lifetime zero disables both actor pairs\n";
}
