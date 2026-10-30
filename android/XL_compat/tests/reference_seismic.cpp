// Actual pinned action and seismic consumers with independent event adapters
#include "../seismic.h"
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>
namespace seismic_reference
{
using fix = std::int32_t;
constexpr int I2X(int n)
{
	return n * 65536;
}
constexpr float X2F(int n)
{
	return float(n) / 65536.0f;
}
constexpr int SHORT_RAND_MAX = 32767;
constexpr bool IsMultiGame = false, COMPETITION = false;
struct CFixVector {
	int distance = 0;
	static int Dist(const CFixVector &a, const CFixVector &)
	{
		return a.distance;
	}
};
struct Object {
	struct {
		struct {
			struct {
				struct {
					int x = 0, z = 0;
				} coord;
			} v;
		} rotVel;
	} physics;
	struct {
		decltype(physics) physInfo;
	} mType;
	CFixVector position;
	const CFixVector &Position() const
	{
		return position;
	}
} objects[2];
Object *OBJECT(int id)
{
	return &objects[id];
}
struct {
	struct {
		int xGame = 0;
	} timeData;
	struct {
		Object *pConsole = objects;
	} objData;
	struct {
		int nObjNum = 1;
	} escortData;
} gameData;
struct {
	struct {
		struct {
			int nShakeFrequency = 0, nShakeDuration = 0, nStartTime = 0, nEndTime = 0, nVolume = 0, nMagnitude = 0, nNextSoundTime = 0, nSound = 0;
			bool bSound = false;
		} seismic;
	} gameplay;
	struct {
		bool bNostalgia = false;
		struct {
			bool bTick = true;
		} tick40fps;
	} app;
	struct {
		bool bSeismic = true;
	} limitFPS;
} gameStates;
struct CTrigger {
	int value, time;
	int GetValue() const
	{
		return value;
	}
	int GetTime(int) const
	{
		return time;
	}
	int DoShakeMine();
};
std::vector<std::pair<int, int>> sounds;
struct {
	void PlayLoopingSound(int, int volume, int, int)
	{
		sounds.emplace_back(1, volume);
	}
	void StopLoopingSound()
	{
		sounds.emplace_back(2, 0);
	}
	void ChangeLoopingVolume(int volume)
	{
		sounds.emplace_back(3, volume);
	}
} audio;
void MultiSendSeismic(int, int)
{
	throw std::runtime_error("Unexpected multiplayer seismic event");
}
unsigned cursor = 0;
std::vector<int> draws;
int RandShort()
{
	const int value = int((cursor++ * 997u + 17u) % 32768);
	draws.push_back(value);
	return value;
}
#include "xl_reference_seismic.h"
void require(bool ok, const char *message)
{
	if (!ok) throw std::runtime_error(message);
}
void compare(const xl_compat::SeismicState &state)
{
	const auto &s = gameStates.gameplay.seismic;
	require(state.start == s.nStartTime && state.end == s.nEndTime && state.pending == s.nShakeDuration && state.strength == s.nShakeFrequency &&
	            state.magnitude == s.nMagnitude && state.volume == s.nVolume && state.sound == s.bSound && state.next_sound == s.nNextSoundTime,
	        "Seismic state differs from actual source");
	for (unsigned i = 0; i < 4; ++i) require(state.detonations[i] == eshakerDetonateTimes[i] && state.scales[i] == eshakerDetonateScales[i], "Seismic detonation state differs from source");
}
} // namespace seismic_reference
void seismic_source_controls()
{
	using namespace seismic_reference;
	unsigned cases = 0;
	for (int strength : { -65536, 0, 1, 32768, 65536, 196608, INT32_MAX })
		for (int seconds : { -1, 0, 1, 3, 10, 32760 })
			for (int distance : { 0, 199, 200, 400, 2000 }) {
				gameStates = {};
				gameStates.limitFPS.bSeismic = true;
				InitShakerDetonates();
				InitSeismicDisturbances();
				objects[0] = objects[1] = {};
				cursor = 0;
				xl_compat::SeismicState state;
				CTrigger action{ strength, seconds };
				require(action.DoShakeMine() == 1 && xl_compat::seismic_action(state, strength, seconds), "Seismic action rejected source-valid parameters");
				compare(state);
				for (int step = 0; step < 110; ++step) {
					gameData.timeData.xGame = 65536 + step * 4096;
					gameStates.app.tick40fps.bTick = step % 3 != 1;
					if (step < 6 || step == 33 || step == 64) {
						CFixVector where{ distance * 65536 };
						ShakerRockStuff(&where);
						xl_compat::seismic_detonate(state, gameData.timeData.xGame, float(distance));
						compare(state);
					}
					if (step == 9 || step == 10 || step == 50) {
						action.value = step == 9 ? 0 : strength;
						action.DoShakeMine();
						xl_compat::seismic_action(state, action.value, seconds);
						compare(state);
					}
					objects[0].mType.physInfo.rotVel = {};
					objects[1].mType.physInfo.rotVel = {};
					sounds.clear();
					draws.clear();
					DoSeismicStuff();
					unsigned at = 0;
					const auto random = [&] { require(at < draws.size(), "Port consumes extra seismic randomness"); return draws[at++]; };
					const auto result = xl_compat::advance_seismic(state, gameData.timeData.xGame, gameStates.app.tick40fps.bTick, random, random);
					compare(state);
					require(at == draws.size() && result.rotation_x == objects[0].mType.physInfo.rotVel.v.coord.x && result.rotation_z == objects[0].mType.physInfo.rotVel.v.coord.z &&
					            result.rotation_x * 4 == objects[1].mType.physInfo.rotVel.v.coord.x && result.rotation_z * 4 == objects[1].mType.physInfo.rotVel.v.coord.z,
					        "Seismic rotations or random consumption differ from source");
					require(result.sound_count == sounds.size(), "Seismic sound count differs from source");
					for (unsigned i = 0; i < result.sound_count; ++i) require(result.sounds[i].operation == sounds[i].first && result.sounds[i].volume == sounds[i].second, "Seismic sound request differs from source");
					++cases;
				}
			}
	xl_compat::SeismicState before;
	before.pending = 123;
	require(!xl_compat::seismic_action(before, 1, 32768) && before.pending == 123, "Overflowing seismic duration changed state");
	std::cout << "PASS: " << cases << " actual source seismic action/frame controls, player/companion rotations, Earthshaker distance, slots, sound requests and random consumption\n";
}
