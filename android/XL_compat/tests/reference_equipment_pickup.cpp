#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>

namespace
{
constexpr int MAX_POWERUP_TYPES = 50, N_LOCALPLAYER = 0, SUSP_POWERUPS = 4;
constexpr int OBJ_GHOST = 12, PF_SPAT_BY_PLAYER = 1, GM_CAPTURE = 1;
constexpr int PROXMINE_INDEX = 2, SMARTMINE_INDEX = 7, SEGMENT_FUNC_ROBOTMAKER = 4;
constexpr int PLAYER_FLAGS_FLAG = 1, PLAYER_FLAGS_BLUE_KEY = 2;
constexpr int POW_VULCAN_AMMO = 22, POW_KEY_BLUE = 4, POW_BLUEFLAG = 46;
constexpr int POW_TURBO = 24, POW_MEGAWOW = 27, POW_MONSTERBALL = 8, POW_HOARD_ORB = 7, POW_REDFLAG = 47;
constexpr int POWERUP_IS_GUN = 1, POWERUP_IS_MISSILE = 2, POWERUP_IS_KEY = 3, POWERUP_IS_EQUIPMENT = 4, POWERUP_IS_FLAG = 5;
constexpr bool OBSERVING = false, IsHoardGame = false, IsEntropyGame = false;
#define I2X(x)           ((x) * 65536)
#define sizeofa(x)       (sizeof(x) / sizeof((x)[0]))
#define ISLOCALPLAYER(x) ((x) == N_LOCALPLAYER)
#define SPECTATOR(x)     false
#define GAMETEXT(x)      "test"
#define GT(x)            "test"
struct CObject {
	struct {
		int nType = 0, nId = 0, nCreator = 0, nSegment = 0;
	} info;
	struct {
		struct {
			int xCreationTime = 0, nFlags = 0;
		} powerupInfo;
	} cType;
	bool Ignored(int, int)
	{
		return false;
	}
	void Die()
	{
		throw std::runtime_error("Single-player inert pickup died");
	}
} console;
struct CPlayerData {
	int nObject = 0, secondaryAmmo[10]{}, flags = 0;
	int Shield()
	{
		return I2X(100);
	}
};
struct {
	struct {
		CPlayerData players[1];
	} multiplayer;
	struct {
		CObject *pConsole = &console;
	} objData;
	struct {
		int xGame = I2X(10);
	} timeData;
	struct {
		int bPlayerMessage = 1;
	} hudData;
	struct {
		bool GameMode(int)
		{
			return false;
		}
	} appData;
	struct {
		struct {
			struct {
				void Reset(int, int)
				{
					throw std::runtime_error("Unexpected flag reset");
				}
			} path;
		} flags[2];
	} pigData;
} gameData;
struct {
	struct {
		int bGameSuspended = 0;
		bool bPlayerIsDead = false;
	} app;
} gameStates;
struct {
	struct {
		int nVirusStability = 0, nMaxVirusCapacity = 0;
	} entropy;
} extraGameInfo[2];
struct Segment {
	int m_owner = 0, m_function = 0;
} segment;
Segment *SEGMENT(int)
{
	return &segment;
}
int GetTeam(int)
{
	return 0;
}
void MultiSendGotOrb(int)
{
	throw std::runtime_error("Unexpected orb message");
}
void MultiSendGotFlag(int)
{
	throw std::runtime_error("Unexpected flag message");
}
void PickupEffect(int, int, int, int, const char *, int)
{
	throw std::runtime_error("Unexpected pickup reward");
}
void ReturnFlagHome(CObject *)
{
	throw std::runtime_error("Unexpected flag return");
}
void UsePowerup(int)
{
	throw std::runtime_error("Unexpected consumed pickup");
}
int PowerupCount(int)
{
	return 1;
}
using pPickupGun = int32_t (*)(CObject *, int32_t, int32_t);
using pPickupMissile = int32_t (*)(CObject *, int32_t, int32_t, int32_t);
using pPickupEquipment = int32_t (*)(CObject *, int32_t);
using pPickupKey = int32_t (*)(CObject *, int32_t, const char *, int32_t);
using pPickupFlag = int32_t (*)(CObject *, int32_t, int32_t, const char *, int32_t);
void *pickupHandler[MAX_POWERUP_TYPES]{};
int powerupType[MAX_POWERUP_TYPES]{}, powerupToDevice[MAX_POWERUP_TYPES]{};
#include "xl_reference_equipment_pickup.h"
} // namespace

void equipment_pickup_reference()
{
	// Assignments are extracted from the pinned initialization, not inferred from Redux
	initialize_pickup_handlers();
	powerupType[POW_HOARD_ORB] = POWERUP_IS_EQUIPMENT;
	powerupType[POW_BLUEFLAG] = powerupType[POW_REDFLAG] = POWERUP_IS_FLAG;
	unsigned cases = 0;
	for (int id : { 7, 8, 9, 24, 26, 27, 46, 47 }) {
		CObject item;
		item.info.nId = id;
		const auto before = gameData.multiplayer.players[0];
		if (DoPowerup(&item, 0) != 0) throw std::runtime_error("Source consumed inert equipment item");
		const auto &after = gameData.multiplayer.players[0];
		if (after.flags != before.flags) throw std::runtime_error("Source inert pickup changed player flags");
		for (unsigned i = 0; i < 10; ++i)
			if (after.secondaryAmmo[i] != before.secondaryAmmo[i]) throw std::runtime_error("Source inert pickup changed ammunition");
		++cases;
	}
	std::cout << cases << " pinned-source single-player equipment pickup dispatch cases passed\n";
}
