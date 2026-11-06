#include <cstdint>
#include <stdexcept>
#include "../runtime/boss_profile.h"

namespace
{
struct tBossProps {
	uint8_t bTeleports, bSpewMore, bSpewBotsEnergy, bSpewBotsKinetic;
	uint8_t bSpewBotsTeleport, bInvulEnergy, bInvulKinetic, bInvulSpot;
};
constexpr int NUM_D2_BOSSES = 8;
#include "xl_reference_boss_properties.h"
} // namespace

namespace death_reference
{
#define ENTER(...)
#define RETURN return;
#define I2X(x) ((x) * 65536)
constexpr int BOSS_DEATH_DURATION = I2X(6), SOUND_BADASS_EXPLOSION_ACTOR = 58, SOUNDCLASS_EXPLOSION = 0;
struct tRobotInfo {
	bool bEndsLevel = false;
	int deathrollSound = 0;
} robot;
int finished = 0, removed = 0, countdown = 0, sounds = 0, explosions = 0;
struct CObject {
	int Index() const
	{
		return 0;
	}
	void Explode(int)
	{
		++explosions;
	}
} object;
tRobotInfo *ROBOTINFO(CObject *)
{
	return &robot;
}
struct State {
	int m_nDyingStartTime = 0, m_bDyingSoundPlaying = 0;
} state;
struct Bosses {
	int Find(int) const
	{
		return 0;
	}
	State &operator[](int)
	{
		return state;
	}
	void Remove(int)
	{
		++removed;
	}
};
struct {
	Bosses bossData;
} gameData;
struct Audio {
	void CreateObjectSound(int, int, int, int, int, int)
	{
		++sounds;
	}
} audio;
int DoRobotDyingFrame(CObject *, int, int, int *, int, int, int)
{
	return finished;
}
void DoReactorDestroyedStuff(void *)
{
	++countdown;
}
#include "xl_reference_boss_death.h"
#undef ENTER
#undef RETURN
#undef I2X
void check(bool ends)
{
	for (int done : { 0, 1 }) {
		finished = done;
		removed = countdown = sounds = explosions = 0;
		robot.bEndsLevel = ends;
		DoBossDyingFrame(&object);
		if (removed != done || sounds != done || explosions != done || countdown != (done && ends))
			throw std::runtime_error("Pinned death completion does not match signed objective handling");
	}
}
} // namespace death_reference

void check_signed_boss_profile()
{
	int accepted = 0;
	for (int raw = -128; raw <= 127; ++raw) {
		struct Reader {
			int8_t value;
			int8_t ReadByte() const
			{
				return value;
			}
		} cf{ static_cast<int8_t>(raw) };
		struct Robot {
			int8_t bossFlag = 0;
			bool bEndsLevel = false;
		} botInfo[1];
		const int h = 0;
#include "xl_reference_boss_role.h"
		const bool supported = raw == -1 || raw == -23 || (raw >= 21 && raw <= 28);
		if (xl_compat::supported_boss_profile(raw) != supported)
			throw std::runtime_error("Boss admission widened beyond the qualified profiles");
		if (!supported) continue;
		++accepted;
		death_reference::check(xl_compat::boss_ends_level(raw));
		if (xl_compat::effective_boss_profile(raw) != botInfo[0].bossFlag ||
		    xl_compat::boss_ends_level(raw) != botInfo[0].bEndsLevel)
			throw std::runtime_error("Signed boss decoding differs from the pinned reader");
	}
	const auto &signed23 = bossProps[0][2];
	if (!signed23.bTeleports || signed23.bSpewMore || signed23.bSpewBotsEnergy || !signed23.bSpewBotsKinetic ||
	    signed23.bSpewBotsTeleport || !signed23.bInvulEnergy || signed23.bInvulKinetic || signed23.bInvulSpot)
		throw std::runtime_error("Signed profile 23 properties differ from qualified collision/spawn behavior");
	const auto &profile = bossProps[0][1];
	if (accepted != 10 || !profile.bTeleports || profile.bSpewBotsTeleport ||
	    profile.bInvulEnergy || profile.bInvulKinetic || profile.bInvulSpot)
		throw std::runtime_error("Sphere profile requires different transport or collision semantics");
}
