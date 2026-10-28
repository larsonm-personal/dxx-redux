#include "reference_motion.h"
#include "xl_reference_teleport.h"
#include <array>
#include <vector>
#include <algorithm>
#define ENTER(a, b)
#define RETVAL(value) return (value);
#define RETURN        return;
using CSegment = Segment;
using CWall = FluidWall;
#define SEGMENT_SIDE_COUNT   6
#define N_LOCALPLAYER        0
#define OBJ_IDX(object)      0
#define LOCALOBJECT          (&object)
#define EGI_FLAG(a, b, c, d) 1
#define SPECTATOR(object)    0
#define X2I(value)           ((value) >> 16)
#define X2F(value)           (float(value) / 65536.0f)
constexpr int OBJ_PLAYER = 4, OBJ_ROBOT = 2, OBJ_WEAPON = 5, OBJ_DEBRIS = 8, MT_PHYSICS = 1;
constexpr int WALL_ILLUSION = 3, WALL_ILLUSION_OFF = 32, TMI_WATER = 2, PLAYER_FLAGS_INVULNERABLE = 1;
constexpr int SOUND_SHIP_IN_WATER = 152, SOUND_LAVAFALL_HISS = 150, SOUND_SHIP_IN_WATERFALL = 158, SOUND_VOLATILE_WALL_HISS = 151, SOUNDCLASS_PLAYER = 0, SOUNDCLASS_GENERIC = 0, SND_ADDON_SCRAPE = 0, IsMultiGame = 0;
struct CPlayerData {
	int flags = 0, netKillsTotal = 0, nKillerObj = -1;
	void UpdateShield(fix value)
	{
		object.info.xShield += value;
	}
	fix Shield() const
	{
		return object.info.xShield;
	}
} player;
constexpr int COMPETITION = 0, IsTeamGame = 0, IsCoopGame = 0, IsEntropyGame = 0;
int GetTeam(int)
{
	throw std::runtime_error("Unexpected multiplayer oracle branch");
}
struct {
	int bInhibitSuicide = 0, bFriendlyFire = 1;
	struct {
		int bPlayerHandicap = 0;
	} entropy;
} extraGameInfo[2];
#define LOCALPLAYER   player
#define PLAYER(index) player
struct {
	int value = 0;
	int Active()
	{
		return value;
	}
} automap;
struct Texture {
	fix damage = 0;
	int flags = 0;
} textures[405];
struct {
	struct {
		CPlayerData *players = &player;
		int bWasHit[1] = {};
	} multiplayer;
	struct {
		int nObjNum = -1;
		fix xSorryTime = 0;
	} escortData;
	struct {
		fix xFrame = 0;
		std::int64_t xGame = 0;
	} timeData;
	struct {
		struct {
			Texture *tMapInfo[1] = { textures };
			Texture *pTexMapInfo = textures;
		} tex;
	} pigData;
	struct {
		CObject *pConsole = &object;
		struct {
			int bBoosted = 0;
		} speedBoost[2];
	} objData;
} gameData;
struct {
	int red = 0;
	void BumpEffect(int r, int, int)
	{
		red = std::min(30, red + r);
	}
} paletteManager;
struct Audio {
	int sound = 0, once = 0;
	int FindObjectSound(int, int value)
	{
		return sound == value && sound ? 0 : -1;
	}
	void DeleteSoundObject(int)
	{
		sound = 0;
	}
	template <class... T>
	void CreateObjectSound(int value, T...)
	{
		sound = value;
	}
	void CreateSegmentSound(int value, int, int, CFixVector)
	{
		once = value;
	}
} audio;
void MultiSendPlaySound(int, int) {}
const char *AddonSoundName(int)
{
	return "";
}
std::int64_t xLastVolatileScrapeSoundTime = 0;
std::array<int, 16> draws;
int consumed = 0;
int Rand(int maximum)
{
	if (consumed == int(draws.size())) throw std::runtime_error("Random transcript exhausted");
	return draws[consumed++] % maximum;
}
int SRandShort()
{
	return 16383 - Rand(32768);
}
void CObject::ApplyDamageToRobot(fix, int)
{
	throw std::runtime_error("Nonplayer volume damage reached");
}
#include "xl_reference_lava.h"
int main(int argc, char **argv)
{
	try {
		if (argc != 2) throw std::runtime_error("Expected lava samples");
		std::ifstream input(argv[1]);
		int count = 0, mode;
		while (input >> mode) {
			int bits, texture, solid, walltype, wallflags, thrust, invulnerable, difficulty, mask, automap_value, movement;
			fix time, mass, normal[3], lava_damage, surface_damage;
			std::int64_t elapsed;
			if (!(input >> bits >> texture >> solid >> walltype >> wallflags >> thrust >> invulnerable >> difficulty >> time >> elapsed >> mass >> automap_value >> movement >> mask >> lava_damage >> surface_damage >> normal[0] >> normal[1] >> normal[2])) throw std::runtime_error("Truncated lava inputs");
			for (auto &draw : draws)
				if (!(input >> draw)) throw std::runtime_error("Truncated lava RNG");
			std::array<std::int64_t, 10> actual;
			for (auto &value : actual)
				if (!(input >> value)) throw std::runtime_error("Truncated lava outputs");
			segment.water = bits & 1;
			segment.lava = bits & 2;
			segment.protected_segment = bits & 8;
			segment.child = solid ? -1 : 1;
			segment.mask = mask;
			CWall wall{ walltype, wallflags };
			for (auto &side : segment.m_sides) {
				side.m_nBaseTex = 0;
				side.wall = walltype < 0 ? nullptr : &wall;
				side.m_normals[0].Set(normal[0], normal[1], normal[2]);
			}
			textures[0] = { surface_damage, texture == 2 ? TMI_WATER : 0 };
			textures[404].damage = lava_damage;
			object.info.nType = OBJ_PLAYER;
			object.info.nId = 0;
			object.info.nSegment = 0;
			object.info.xShield = I2X(100);
			object.info.movementType = movement;
			object.mType.physInfo.velocity.SetZero();
			object.mType.physInfo.rotVel.SetZero();
			object.mType.physInfo.thrust.Set(thrust ? I2X(1) : 0, 0, 0);
			object.mType.physInfo.mass = mass;
			player.flags = invulnerable ? PLAYER_FLAGS_INVULNERABLE : 0;
			gameStates.app.nDifficultyLevel = difficulty;
			gameData.timeData.xFrame = time;
			gameData.timeData.xGame = I2X(10);
			xLastVolatileScrapeSoundTime = gameData.timeData.xGame - elapsed;
			paletteManager.red = 0;
			consumed = 0;
			audio = {};
			automap.value = automap_value;
			int type = 0;
			if (mode == 0) {
				object.CheckWallPhysics();
				type = 1;
			} else if (mode == 1) type = object.ApplyWallPhysics(0, 0);
			else {
				CFixVector point;
				object.ScrapeOnWall(0, 0, point);
				type = 1;
			}
			const auto &v = object.mType.physInfo.velocity.v.coord;
			const std::array<std::int64_t, 10> expected = { type, I2X(100) - object.info.xShield, paletteManager.red, object.mType.physInfo.rotVel.v.coord.x, object.mType.physInfo.rotVel.v.coord.z, v.x, v.y, v.z, gameData.timeData.xGame - xLastVolatileScrapeSoundTime, consumed };
			for (unsigned field = 0; field < expected.size(); ++field)
				if (actual[field] != expected[field]) throw std::runtime_error("Lava source differs at sample " + std::to_string(count) + " mode " + std::to_string(mode) + " field " + std::to_string(field) + " expected " + std::to_string(expected[field]) + " actual " + std::to_string(actual[field]));
			++count;
		}
		if (count < 1000) throw std::runtime_error("Insufficient lava samples");
		std::cout << count << " extracted-reference lava contact samples passed\n";
	} catch (const std::exception &error) {
		std::cerr << error.what() << '\n';
		return 1;
	}
}
