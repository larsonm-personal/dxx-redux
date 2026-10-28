#include <cstdint>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
using fix = std::int32_t;
constexpr fix I2X(int x)
{
	return x * 65536;
}
fix FixMul(fix a, fix b)
{
	return fix(std::int64_t(a) * b >> 16);
}
#define ENTER(a, b)
#define RETVAL(x) return (x);
#define RETURN    return;
#include "xl_reference_repair_types.h"
constexpr int SOUND_REFUEL_STATION_GIVING_FUEL = 62, SOUNDCLASS_GENERIC = 0;
constexpr fix PRODUCER_SOUND_DELAY = 65536 / 4;
constexpr bool IsEntropyGame = false, IsMultiGame = false;
constexpr int N_LOCALPLAYER = 0;
constexpr int OBJ_PLAYER = 4, GM_CAPTURE = 1;
constexpr bool IsHoardGame = false;
struct {
	int forwardThrustTime = 0, verticalThrustTime = 0, sidewaysThrustTime = 0;
} controls[1];
struct {
	struct {
		int nTimeLastMoved = 0;
	} entropy;
} gameStates;
void StopConquerWarning() {}
struct CPlayerData {
	fix shield = 0, energy = 0;
	fix Shield() const
	{
		return shield;
	}
	fix Energy() const
	{
		return energy;
	}
	fix InitialShield() const
	{
		return I2X(100);
	}
	fix InitialEnergy() const
	{
		return I2X(100);
	}
	void UpdateShield(fix amount)
	{
		shield += amount;
	}
	void UpdateEnergy(fix amount)
	{
		energy += amount;
	}
};
struct CObject {
	struct {
		int nType = OBJ_PLAYER, nId = 0, nSegment = 0;
	} info;
	void HandleSegmentFunction();
} player;
void StartPlayerDeathSequence(CObject *) {}
int GetTeam(int)
{
	return 0;
}
void MultiSendPlaySound(int, fix) {}
fix repair_clock = 0;
unsigned cues = 0;
struct CSegment {
	int m_function = 0, m_owner = -1, m_props = 0, m_xDamage[2]{};
	void Upgrade();
	fix Repair(fix);
	fix ShieldDamage(fix)
	{
		return 0;
	}
	fix EnergyDamage(fix)
	{
		return 0;
	}
	fix Refuel(fix)
	{
		return 0;
	}
	void CheckForGoal() {}
	void CheckForHoardGoal() {}
	void ConquerCheck() {}
} segment;
CSegment *SEGMENT(int)
{
	return &segment;
}
struct {
	struct {
		int bProducers = 0;
	} legacy;
} options;
auto *gameOpts = &options;
struct {
	struct {
		CSegment *playerSegP = nullptr;
	} producerData;
	struct {
		fix xFrame = 0, xGame = 0;
	} timeData;
	struct {
		bool GameMode(int) const
		{
			return false;
		}
	} appData;
	struct {
		CPlayerData players[1];
	} multiplayer;
} gameData;
struct {
	struct {
		int nShieldFillRate = 11;
	} entropy;
} extraGameInfo[2];
struct {
	void PlaySound(int sound, int, fix volume)
	{
		if (sound != 62 || volume != 32768) throw std::runtime_error("Incorrect repair cue");
		++cues;
	}
} audio;
#if defined(__GNUC__)
// Preserve upstream indentation inside the hash-pinned extracted caller
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmisleading-indentation"
#endif
#include "xl_reference_repair.h"
#include "xl_reference_repair_upgrade.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
int main(int argc, char **argv)
{
	try {
		if (argc != 3 && argc != 4) throw std::runtime_error("Expected repair and actual-frame samples");
		const bool legacy = argc == 4 && std::string(argv[3]) == "legacy";
		if (argc == 4 && !legacy) throw std::runtime_error("Unknown repair profile");
		auto setup = [&](int function) {
			segment.m_function = function;
			if (legacy) {
				segment.m_props = 255;
				segment.m_xDamage[0] = segment.m_xDamage[1] = 123;
				segment.Upgrade();
				if (segment.m_function != function || segment.m_props || segment.m_xDamage[0] || segment.m_xDamage[1]) throw std::runtime_error("Legacy repair upgrade differs");
			}
		};
		std::ifstream in(argv[1]);
		int function, frame, maximum, now, last, expected_amount, expected_clock, expected_cues, count = 0;
		while (in >> function >> frame >> maximum >> now >> last >> expected_amount >> expected_clock >> expected_cues) {
			setup(function);
			gameData.timeData = { frame, now };
			repair_clock = last;
			cues = 0;
			const auto amount = segment.Repair(maximum);
			if (amount != expected_amount || repair_clock != expected_clock || cues != unsigned(expected_cues))
				throw std::runtime_error("Repair differs at sample " + std::to_string(count));
			++count;
		}
		if (count != (legacy ? 1440 : 5760)) throw std::runtime_error("Incomplete repair source matrix");
		std::cout << count << " extracted-reference repair samples passed\n";
		std::ifstream frames(argv[2]);
		int health, before_energy, after_health, after_energy;
		count = 0;
		while (frames >> function >> frame >> health >> before_energy >> after_health >> after_energy) {
			setup(function);
			gameData.timeData = { frame, 0 };
			repair_clock = 0;
			gameData.multiplayer.players[0] = { health, before_energy };
			player.HandleSegmentFunction();
			const auto &result = gameData.multiplayer.players[0];
			if (result.shield != after_health || result.energy != after_energy)
				throw std::runtime_error("Actual repair frame differs from source caller at " + std::to_string(count));
			++count;
		}
		if (count != (legacy ? 12 : 42)) throw std::runtime_error("Incomplete actual-frame repair matrix");
		std::cout << count << " actual player frames matched extracted source caller\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
