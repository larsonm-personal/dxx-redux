#include "../segment_damage.h"
#include <algorithm>
#include <vector>
#include <cmath>
#include <climits>
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
constexpr int SOUND_REFUEL_STATION_GIVING_FUEL = 62, SOUND_PLAYER_GOT_HIT = 7, SOUNDCLASS_GENERIC = 0;
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
std::vector<int> events;
int deaths = 0;
fix fuel_max = 0, repair_max = 0;
bool centers = false;
int center_mode = 3;
template <class T>
T Clamp(T v, T lo, T hi)
{
	return std::clamp(v, lo, hi);
}
int FRound(float v)
{
	return int(std::round(v));
}
struct CShipEnergy {
	fix *m_current = nullptr;
	struct {
		void Start() {}
		void Setup(int) {}
	} m_toRecharge;
	fix Get() const
	{
		return *m_current;
	}
	fix Max() const
	{
		return I2X(200);
	}
	float Scale() const
	{
		return 1;
	}
	void SetRechargeDelay(int) {}
	int RechargeDelay(int)
	{
		return 0;
	}
	bool Set(fix, bool = false);
#include "xl_reference_resource_update.h"
};
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
	void UpdateShield(fix delta)
	{
		events.push_back(1);
		CShipEnergy v;
		v.m_current = &shield;
		v.Update(delta);
	}
	void UpdateEnergy(fix delta)
	{
		events.push_back(2);
		CShipEnergy v;
		v.m_current = &energy;
		v.Update(delta);
	}
};
struct CObject {
	struct {
		int nType = OBJ_PLAYER, nId = 0, nSegment = 0;
	} info;
	void HandleSegmentFunction();
} player;
void StartPlayerDeathSequence(CObject *)
{
	events.push_back(3);
	++deaths;
}
int GetTeam(int)
{
	return 0;
}
void MultiSendPlaySound(int, fix) {}
fix repair_clock = 0;
unsigned cues = 0;
fix game_frame_fuel = 0, game_frame_repair = 0;
struct CSegment {
	int m_function = 0, m_owner = -1, m_props = 0, m_xDamage[2]{};
	void Upgrade();
	fix ShieldDamage(fix);
	fix EnergyDamage(fix);
	fix Refuel(fix maximum)
	{
		events.push_back(4);
		fuel_max = maximum;
		return centers && (center_mode & 1) ? std::max(0, std::min(maximum, game_frame_fuel)) : 0;
	}
	fix Repair(fix maximum)
	{
		events.push_back(5);
		repair_max = maximum;
		return centers && (center_mode & 2) ? std::max(0, std::min(maximum, game_frame_repair)) : 0;
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
		int nShieldFillRate = 11, nShieldDamageRate = 0;
		int nRechargeDelay = 0;
	} entropy;
	int nRechargeDelay = 0;
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
#include "xl_reference_segment_damage.h"
#if defined(__GNUC__)
#pragma GCC diagnostic pop
#endif
static void require(bool valid, const char *message)
{
	if (!valid) throw std::runtime_error(message);
}
int main(int argc, char **argv)
{
	try {
		unsigned cases = 0;
		for (int sr : { -32768, -5, -1, 0, 1, 3, 32767 })
			for (int er : { -32768, -5, -1, 0, 1, 5, 32767 })
				for (fix frame : { 0, 1, 65536 / 60, 65536 / 30, 65536 / 4, 65536 / 5 })
					for (fix health : { -1, 0, 1, 32769, 6553600, 13107200, 32768000 })
						for (fix power : { -1, 0, 1, 32769, 6553600, 13107200, 32768000 })
							for (int kind : { 0, 1, 2, 3 })
								for (bool center : { false, true }) {
									segment.m_xDamage[0] = I2X(sr);
									segment.m_xDamage[1] = I2X(er);
									gameData.timeData.xFrame = frame;
									player.info.nType = kind < 2 ? OBJ_PLAYER : kind == 2 ? 2
									                                                      : 6;
									player.info.nId = kind == 1 ? 1 : 0;
									auto &p = gameData.multiplayer.players[0];
									p = { health, power };
									events.clear();
									deaths = 0;
									centers = center;
									game_frame_fuel = frame * 25;
									game_frame_repair = frame * 11;
									fuel_max = repair_max = INT32_MIN;
									player.HandleSegmentFunction();
									fix h = health, e = power;
									std::vector<int> order;
									if (!kind) {
										h = xl_compat::segment_resource_after(h, xl_compat::segment_damage_amount(frame, sr, h, true));
										e = xl_compat::segment_resource_after(e, xl_compat::segment_damage_amount(frame, er, e, false));
										order = { 1, 2 };
										if (h < 0) {
											order.push_back(3);
											require(deaths == 1, "Missing death boundary");
										} else {
											require(fuel_max == I2X(100) - e && repair_max == I2X(100) - h, "Center maximum/order differs");
											order.push_back(4);
											const fix fuel = center ? std::max(0, std::min(I2X(100) - e, frame * 25)) : 0;
											if (fuel) {
												e += fuel;
												order.push_back(2);
											}
											order.push_back(5);
											const fix repair = center ? std::max(0, std::min(I2X(100) - h, frame * 11)) : 0;
											if (repair) {
												h += repair;
												order.push_back(1);
											}
										}
									}
									require(p.shield == h && p.energy == e && events == order, "Pinned caller/leaf/Set/Update differs");
									++cases;
								}
		if (argc == 2) {
			std::ifstream samples(argv[1]);
			int sr, er, function, frame, h, e, after_h, after_e, count = 0;
			while (samples >> sr >> er >> function >> frame >> h >> e >> after_h >> after_e) {
				segment.m_xDamage[0] = I2X(sr);
				segment.m_xDamage[1] = I2X(er);
				player.info = { OBJ_PLAYER, 0, 0 };
				centers = true;
				center_mode = function == 1 ? 1 : function == 2 ? 2
				                                                : 0;
				game_frame_fuel = frame * 25;
				game_frame_repair = frame * 11;
				gameData.timeData.xFrame = frame;
				auto &p = gameData.multiplayer.players[0];
				p = { h, e };
				player.HandleSegmentFunction();
				require(p.shield == after_h && p.energy == after_e, "Actual native frame differs from extracted caller");
				++count;
			}
			require(samples.eof() && count == 8400, "Incomplete actual native frame samples");
			std::cout << count << " actual native frames matched extracted segment caller\n";
		} else require(argc == 1, "Unexpected oracle arguments");
		// Undefined reference arithmetic is deliberately excluded from exact comparisons
		for (int current : { INT32_MIN, -1, 0, INT32_MAX })
			for (int frame : { INT32_MIN, INT32_MAX })
				for (int rate : { -32768, 32767 }) {
					const auto amount = xl_compat::segment_damage_amount(frame, rate, current, true);
					const auto after = xl_compat::segment_resource_after(current, amount);
					require(!amount ? after == current : after >= -1 && after <= I2X(200), "Wide extension escaped bounds");
				}
		std::cout << cases << " pinned segment leaf/caller/Set/Update comparisons and separate wide extreme controls passed\n";
	} catch (const std::exception &e) {
		std::cerr << e.what() << '\n';
		return 1;
	}
}
