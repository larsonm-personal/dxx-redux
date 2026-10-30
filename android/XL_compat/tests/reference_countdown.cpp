// Pinned countdown functions execute against independent state and output adapters
#include "../countdown.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <utility>
#include <vector>

namespace countdown_reference
{
using fix = std::int32_t;
#include "xl_reference_countdown_constants.h"
constexpr int SOUNDCLASS_GENERIC = 0;
constexpr int I2X(int value)
{
	return value * 65536;
}
constexpr int X2I(int value)
{
	return value >> 16;
}
constexpr int F2X(double value)
{
	return int(value * 65536.0);
}
int FixMul(int a, int b)
{
	return int((std::int64_t(a) * b) >> 16);
}
static_assert(SOUND_COUNTDOWN_0_SECS == 100 && SOUND_COUNTDOWN_13_SECS == 113 && SOUND_COUNTDOWN_29_SECS == 114 &&
              SOUND_CONTROL_CENTER_WARNING_SIREN == 32 && SOUND_MINE_BLEW_UP == 33 &&
              DIFFICULTY_LEVEL_COUNT == 5 && DEFAULT_CONTROL_CENTER_EXPLOSION_TIME == 30);
constexpr int COUNTDOWN_VOICE_TIME = F2X(12.75), GM_MULTI_ROBOTS = 4;
constexpr bool IS_D2_OEM = false, IS_MAC_SHARE = false, IS_SHAREWARE = false, IsMultiGame = false;
struct CTrigger {
	struct {
		int time[2]{};
	} m_info;
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
	} physInfo;
};
struct PlayerObject {
	Object mType;
} player;
struct {
	struct {
		bool bDestroyed = false;
		struct {
			int nTimer = 0, nSecsLeft = 0, nTotalTime = 0;
		} countdown;
	} reactorData;
	struct {
		int xRealFrame = 0;
		unsigned tLast = 0;
	} timeData;
	struct {
		PlayerObject *pConsole = &player;
	} objData;
	struct {
		bool GameMode(int) const
		{
			return false;
		}
	} appData;
	struct {
		struct {
			void Activate(const char *) {}
			void Deactivate() {}
		} frame;
	} renderData;
} gameData;
struct {
	struct {
		bool bPlayerIsDead = false, bD1Mission = false;
		int nDifficultyLevel = 0, nBaseCtrlCenExplTime = 30;
		unsigned nSDLTicks[2]{};
		struct {
			unsigned nTime = 0, nLastTick = 0;
			bool bTick = false;
		} tick40fps;
	} app;
	struct {
		bool bCountDown = true;
	} limitFPS;
} gameStates;
struct {
	int nCurrentMission = 1, nBuiltInMission[1]{ 0 }, nCurrentLevel = 1, nLastLevel = 2;
} missionManager;
std::vector<std::pair<int, int>> sounds;
struct {
	void PlaySound(int id, int = SOUNDCLASS_GENERIC, int volume = I2X(1))
	{
		sounds.emplace_back(id, volume);
	}
} audio;
int last_flash = -1, deaths = 0, clears = 0, slow_off = 0;
bool paused = false;
struct {
	int blue = 0;
	void StartEffect(int r, int g, int b)
	{
		if (r != g || r != b) throw std::runtime_error("Reference countdown flash is not grayscale");
		last_flash = blue = (std::clamp) (b, 0, 64);
	}
	int BlueEffect() const
	{
		return blue;
	}
	void ResetEffect()
	{
		blue = 0;
	}
} paletteManager;
struct CCanvas {
	static CCanvas *Current()
	{
		static CCanvas canvas;
		return &canvas;
	}
	void Clear(int)
	{
		++clears;
	}
};
int RGBA_PAL2(int, int, int)
{
	return 0;
}
bool TimeStopped()
{
	return paused;
}
bool IAmGameHost()
{
	return false;
}
void MultiSendCountdown()
{
	throw std::runtime_error("Unexpected reference multiplayer countdown");
}
void DoPlayerDead()
{
	++deaths;
}
void SlowMotionOff()
{
	++slow_off;
}
unsigned now_ms = 0;
unsigned SDL_GetTicks()
{
	return now_ms;
}
std::uint32_t random_state = 1;
unsigned random_calls = 0;
int random_value(std::uint32_t &state)
{
	state ^= state << 13;
	state ^= state >> 17;
	state ^= state << 5;
	return int((state >> 17) & 32767) - 16384;
}
int SRandShort()
{
	++random_calls;
	return random_value(random_state);
}
#include "xl_reference_countdown.h"
} // namespace countdown_reference

void countdown_source_controls()
{
	using namespace countdown_reference;
	const auto require = [](bool condition, const char *reason) { if (!condition) throw std::runtime_error(reason); };
	unsigned cases = 0;
	for (int authored : { -99, 0, 1, 99, 1000, 32767 })
		for (int base : { 0, 1, 30, 60, 500 })
			for (int difficulty = 0; difficulty < 5; ++difficulty)
				for (bool d1 : { false, true })
					for (int timer : { -1, 0, 1, 400 * 65536 })
						for (bool destroy : { false, true }) {
							CTrigger action;
							action.m_info.time[0] = authored;
							gameStates.app.nBaseCtrlCenExplTime = base;
							gameStates.app.nDifficultyLevel = difficulty;
							gameStates.app.bD1Mission = d1;
							gameData.reactorData.bDestroyed = false;
							gameData.reactorData.countdown.nSecsLeft = 17;
							slow_off = 0;
							InitCountdown(&action, destroy, timer);
							xl_compat::Countdown owned;
							owned.seconds = 17;
							require(xl_compat::initialize_countdown(owned, authored, base, difficulty, timer, destroy, d1), "Valid source countdown initializer rejected");
							const auto &actual = gameData.reactorData.countdown;
							require(owned.total == actual.nTotalTime && owned.timer == actual.nTimer && owned.seconds == actual.nSecsLeft && bool(owned.destroyed) == gameData.reactorData.bDestroyed && slow_off == int(destroy),
							        "Countdown initialization differs from pinned source");
							++cases;
						}
	for (int initial_phase = 0; initial_phase < 25; ++initial_phase) {
		int phase = initial_phase;
		now_ms = 100;
		gameStates.app.tick40fps.nLastTick = now_ms - initial_phase;
		for (int elapsed : { 0, 1, 5, 8, 16, 17, 24, 25, 26, 33, 100, 0, 7, 18, 200 }) {
			now_ms += elapsed;
			GetSlowTicks();
			require(xl_compat::countdown_tick(phase, elapsed) == gameStates.app.tick40fps.bTick && phase == int(now_ms - gameStates.app.tick40fps.nLastTick),
			        "Countdown cadence retained overshoot or differs at the 25 ms boundary");
			++cases;
		}
	}
	unsigned terminal_episodes = 0;
	for (int duration : { 0, 1, 5, 14, 30, 99 })
		for (int difficulty : { 0, 2, 4 })
			for (int milliseconds : { 5, 16, 25, 83, 1000 })
				for (bool limited : { false, true }) {
					gameData.reactorData.bDestroyed = false;
					gameStates.app.bPlayerIsDead = false;
					DoCountdownFrame();
					gameStates.app.bD1Mission = false;
					gameStates.app.nDifficultyLevel = difficulty;
					gameStates.limitFPS.bCountDown = limited;
					missionManager.nCurrentMission = 1;
					xl_compat::Countdown owned;
					xl_compat::initialize_countdown(owned, duration, duration ? 30 : 0, difficulty);
					gameData.reactorData.bDestroyed = true;
					gameData.reactorData.countdown = { owned.timer, owned.seconds, owned.total };
					player.mType.physInfo.rotVel.v.coord = {};
					now_ms = 0;
					gameStates.app.tick40fps.nLastTick = 0;
					std::uint32_t owned_rng = random_state = 12345;
					unsigned owned_calls = 0;
					random_calls = 0;
					int x = 0, z = 0;
					bool terminal = false;
					for (int frame = 0; frame < (duration + duration / 3 + 12) * 1000 / milliseconds; ++frame) {
						now_ms += milliseconds;
						GetSlowTicks();
						gameData.timeData.xRealFrame = milliseconds * 65536 / 1000;
						paused = frame % 19 == 3;
						gameStates.app.bPlayerIsDead = frame % 29 == 7;
						sounds.clear();
						last_flash = -1;
						deaths = clears = 0;
						DoCountdownFrame();
						xl_compat::CountdownInput input{ gameData.timeData.xRealFrame, difficulty, gameStates.app.tick40fps.bTick, limited, paused, gameStates.app.bPlayerIsDead, false };
						const auto output = xl_compat::advance_countdown(owned, input, [&] { ++owned_calls; return random_value(owned_rng); });
						x += output.rotation_x;
						z += output.rotation_z;
						std::vector<std::pair<int, int>> expected;
						for (unsigned i = 0; i < output.sound_count; ++i) expected.emplace_back(output.sounds[i].id, output.sounds[i].volume);
						const auto &actual = gameData.reactorData.countdown;
						require(owned.timer == actual.nTimer && owned.seconds == actual.nSecsLeft && owned.total == actual.nTotalTime && sounds == expected && last_flash == output.flash && deaths == int(output.death) && clears == deaths &&
						            x == player.mType.physInfo.rotVel.v.coord.x && z == player.mType.physInfo.rotVel.v.coord.z && random_calls == owned_calls && random_state == owned_rng,
						        "Countdown frames differ in timing, warnings, motion, RNG, flash or death");
						++cases;
						if (output.death) {
							terminal = true;
							++terminal_episodes;
							break;
						}
					}
					require(terminal, "Reference countdown episode never reached terminal death");
				}
	std::cout << "PASS: " << cases << " pinned countdown initialization, cadence and frame comparisons; " << terminal_episodes << " terminal episodes\n";
}
