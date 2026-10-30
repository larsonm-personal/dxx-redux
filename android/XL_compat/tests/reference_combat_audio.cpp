#include "audio_combat.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
namespace combat_reference
{
using fix = std::int32_t;
#define ENTER(a, b)
#define RETURN return;
#include "xl_reference_audio_constants.h"
#include "xl_reference_combat_constants.h"
struct CFixVector {
};
struct CWeaponInfo {
	int flashSound = 7;
};
std::array<CWeaponInfo, 256> weapons;
bool weapon_info_available = true;
struct CObject {
	int id = 0, index = 0;
	struct {
		int nType = 4, nSegment = 1;
		struct {
			CFixVector vPos;
		} position;
	} info;
	struct {
		struct {
			struct {
				int nObject = 0;
			} parent;
		} laserInfo;
	} cType;
	static std::array<unsigned, 256> m_weaponInfo;
	int Id() const
	{
		return id;
	}
	int Index() const
	{
		return index;
	}
	bool IsGatlingRound() const
	{
		return id == VULCAN_ID || id == GAUSS_ID;
	}
	static bool IsMissile(int)
	{
		return false;
	}
	bool IsSplashDamageWeapon();
	static void InitSplash();
	void WeaponSound();
	void ActorSound();
	void WaterSound();
};
std::array<unsigned, 256> CObject::m_weaponInfo{};
std::array<CObject, 2> objects;
struct {
	struct {
		CObject *pViewer = &objects[0];
	} objData;
} gameData;
struct {
	struct {
		bool bD1Mission = false;
	} app;
} gameStates;
struct Options {
	struct {
		int bGatling = 0, bMissiles = 0, xCustomSoundVolume = 65536;
	} sound;
	int UseHiresSound() const
	{
		return 0;
	}
} options, *gameOpts = &options;
#define WEAPONINFO(id) (weapon_info_available ? &weapons[id] : nullptr)
#define OBJECT(i)      ((i) >= 0 && (i) < 2 ? &objects[i] : nullptr)
#define OBJ_IDX(p)     ((p)->Index())
#define OBJ_PLAYER     4
const char *AddonSoundName(int)
{
	return "addon";
}
struct Request {
	int calls = 0, kind = -1, sound = -1, sound_class = -1, volume = -1;
	int range = 0, loop = 0;
} request;
struct Audio {
	void PlaySound(int sound, int sound_class, int volume, int = 32767, int = 0, int = -1, const char *addon = nullptr)
	{
		if (addon) throw std::runtime_error("Unexpected optional firing addon");
		request = { request.calls + 1, 0, sound, sound_class, volume };
	}
	void CreateSegmentSound(int sound, int, int, CFixVector &, int, int volume, int = 256 * 65536, const char *addon = nullptr)
	{
		if (addon) throw std::runtime_error("Unexpected optional segment addon");
		request = { request.calls + 1, 1, sound, SOUNDCLASS_GENERIC, volume };
	}
	void CreateObjectSound(int sound, int sound_class, int, int loop = 0, int volume = 65536, int range = 256 * 65536, int = -1, int = -1, const char *addon = nullptr, int = 0)
	{
		// Flare's optional addon is separate from the ordinary flash event
		if (addon) return;
		request = { request.calls + 1, 2, sound, sound_class, volume, range, loop };
	}
} audio;
#include "xl_reference_combat_audio.h"
#include "xl_reference_actor_audio.h"
} // namespace combat_reference
void actor_audio_source_controls()
{
	using namespace combat_reference;
	unsigned cases = 0;
	for (int sound : { -1, 7, 22 })
		for (int scale : { 8192, 32768, 65536, 4 * 65536, 127 * 32768 }) {
			const auto check = [&](int expected_sound, int expected_class, int volume, int range, int loop) {
				if (request.calls != 1 || request.kind != 2 || request.sound != expected_sound || request.sound_class != expected_class || request.volume != volume || request.range != range || request.loop != loop)
					throw std::runtime_error("Actual actor sound call changed");
				++cases;
				request = {};
			};
			request = {};
			BossSee(&objects[0], sound, scale);
			check(sound, XL_SOUND_ROBOT, 65536, 512 * 65536, 1);
			BossFinal(&objects[0], sound, scale);
			check(58, XL_SOUND_EXPLOSION, 4 * 65536, 512 * 65536, 0);
			RobotGroan(&objects[0], sound, scale);
			check(sound, XL_SOUND_ROBOT, scale, scale * 256, 0);
			RobotFinal(&objects[0], sound, scale);
			check(11, XL_SOUND_EXPLOSION, 2 * 65536, 512 * 65536, 0);
		}
	std::cout << "PASS: " << cases << " actual boss/robot sound calls with source class, sample, gain, range and loop controls\n";
}
void combat_audio_source_controls()
{
	using namespace combat_reference;
	CObject::InitSplash();
	objects[0].index = 0;
	objects[1].index = 1;
	CObject weapon;
	unsigned count = 0;
	for (int id = 0; id < 256; ++id) {
		weapon.id = id;
		for (int parent : { -1, 0, 1 }) {
			weapon.cType.laserInfo.parent.nObject = parent;
			for (bool available : { false, true })
				for (int flash : { -1, 7, 22 })
					for (int make : { 0, 1 }) {
						request = {};
						weapon_info_available = available;
						weapons[id].flashSound = flash;
						CreateWeaponSound(&weapon, make);
						const bool audible = available && flash >= 0 && make;
						if (request.calls != int(audible)) throw std::runtime_error("Source firing suppression differs");
						if (audible) {
							const auto actual = xl_compat::weapon_fire_audio(id, parent >= 0, parent == 0);
							if (request.sound != flash || request.kind != int(actual.spatial) || request.sound_class != actual.sound_class || request.volume != actual.volume)
								throw std::runtime_error("Weapon firing policy differs from source");
						}
						++count;
					}
		}
		for (bool d1 : { false, true }) {
			gameStates.app.bD1Mission = d1;
			request = {};
			weapon.WeaponSound();
			if (request.calls != 1 || request.sound != xl_compat::weapon_explosion_sound(id, d1) || request.sound_class != XL_SOUND_EXPLOSION || request.volume != 65536)
				throw std::runtime_error("Weapon explosion policy differs from source");
			request = {};
			weapon.WaterSound();
			if (request.calls != 1 || request.sound != xl_compat::weapon_explosion_sound(id, false) || request.sound_class != XL_SOUND_EXPLOSION || request.volume != 65536)
				throw std::runtime_error("Water explosion policy differs from source");
			request = {};
			weapon.ActorSound();
			if (request.calls != 1 || request.sound != xl_compat::actor_explosion_sound(d1) || request.sound_class != XL_SOUND_EXPLOSION || request.volume != 65536)
				throw std::runtime_error("Actor explosion policy differs from source");
			count += 3;
		}
	}
	std::cout << "PASS: " << count << " actual source combat audio decisions, including explicit splash classification and silent firing\n";
}
