#include "camera_flash.h"
#include <iostream>
#include <stdexcept>
namespace camera_flash_reference
{
using fix = std::int32_t;
constexpr int SOUND_CLOAK_OFF = 1, SOUNDCLASS_GENERIC = 0, ND_STATE_RECORDING = 1;
constexpr int N_LOCALPLAYER = 0, PK1 = 1, PK2 = 8;
template <class T>
T Clamp(T v, T a, T b)
{
	return std::clamp(v, a, b);
}
template <class T>
T Max(T a, T b)
{
	return (std::max) (a, b);
}
template <class T>
T Min(T a, T b)
{
	return (std::min) (a, b);
}
struct Color {
	std::array<float, 3> rgb{};
	float &Red()
	{
		return rgb[0];
	}
	float &Green()
	{
		return rgb[1];
	}
	float &Blue()
	{
		return rgb[2];
	}
	bool IsZero() const
	{
		return rgb == std::array<float, 3>{};
	}
};
struct CObject {
	struct {
		int nId = 0, nSignature = 0;
	} info;
	struct {
		struct {
			struct {
				int nSignature = 0;
			} parent;
		} laserInfo;
	} cType;
};
struct {
	struct {
		fix xFrame = 1092;
		std::int64_t xGame = 0;
	} timeData;
	struct {
		int nState = 0;
	} demoData;
	struct {
		CObject *pConsole = nullptr;
	} objData;
} gameData;
struct {
	std::optional<int> sound;
	void PlaySound(int, int, int volume)
	{
		sound = volume;
	}
} audio;
int random_calls = 0, random_value = 0;
int RandShort()
{
	++random_calls;
	return random_value;
}
std::optional<std::array<int, 3>> recorded;
void NDRecordPaletteEffect(std::int16_t r, std::int16_t g, std::int16_t b)
{
	recorded = std::array<int, 3>{ r, g, b };
}
struct CPaletteManager {
	struct {
		int nSuspended = 0;
		bool bDoEffect = false;
		Color effect, flash;
		fix xFadeDuration[2]{}, xFadeDelay = 0;
		std::int64_t xLastEffectTime = 0;
	} m_data;
	void StartEffect(float, float, float, bool = false);
	void StartEffect(std::int32_t, std::int32_t, std::int32_t, bool = false);
	void StopEffect();
	void BumpEffect(float, float, float);
	void BumpEffect(std::int32_t, std::int32_t, std::int32_t);
	void UpdateEffect();
	void FadeEffect();
	float FadeScale();
	void SetFadeDuration(fix);
	void SetFadeDelay(fix);
	std::int8_t RedEffect(bool = false), GreenEffect(bool = false), BlueEffect(bool = false);
} paletteManager;
#include "xl_reference_camera_flash.h"
} // namespace camera_flash_reference
void camera_flash_palette_source_controls()
{
	using namespace camera_flash_reference;
	unsigned pulses = 0, ticks = 0;
	for (int force : { 0, 65536, 65537, 131072, 131073, 1048576, 2621440 })
		for (int flash : { 0, 1, 4, 8, 16 })
			for (bool self : { false, true })
				for (int id : { 0, 1 }) {
					paletteManager = {};
					CObject actor, weapon, player;
					actor.info.nId = id;
					player.info.nSignature = 123;
					weapon.cType.laserInfo.parent.nSignature = self ? 123 : 456;
					gameData.objData.pConsole = &player;
					player_pulse(&actor, &weapon, force, flash);
					xl_compat::CameraFlashPalette actual;
					auto pulse = xl_compat::camera_flash_pulse(force, flash, self);
					if (!id && pulse.intensity) {
						actual.bump({ pulse.intensity, pulse.intensity, pulse.intensity });
						actual.delay = pulse.delay;
					}
					if (actual.color != paletteManager.m_data.effect.rgb || actual.delay != paletteManager.m_data.xFadeDelay)
						throw std::runtime_error("Camera flash player pulse differs from source");
					++pulses;
				}
	for (int initial : { 0, 1, 20, 30, 60 })
		for (int delay : { 0, 1, 8192, 262144 })
			for (int frame : { 546, 1092, 2184, 16384 })
				for (int seed : { 0, 4096, 4097, 32767 }) {
					paletteManager = {};
					paletteManager.StartEffect(initial, initial / 2, initial / 3);
					paletteManager.SetFadeDelay(delay);
					xl_compat::CameraFlashPalette actual;
					actual.start({ float(initial) / 64, float(initial / 2) / 64, float(initial / 3) / 64 });
					actual.delay = delay;
					gameData.timeData.xGame = 0;
					gameData.timeData.xFrame = frame;
					for (int tick = 0; tick < 520; ++tick) {
						if (tick == 30 || tick == 90) {
							const std::array<int, 3> bump{ tick == 30 ? 4 : -8, 3, 12 };
							paletteManager.BumpEffect(bump[0], bump[1], bump[2]);
							actual.bump(bump);
						}
						if (tick == 60) {
							// Save stores faded integer colors; restore starts a fresh half-second fade
							std::array<int, 3> saved{ paletteManager.RedEffect(true), paletteManager.GreenEffect(true), paletteManager.BlueEffect(true) };
							if (actual.saved_color() != saved) throw std::runtime_error("Camera flash saved color differs from source");
							paletteManager.StartEffect(saved[0], saved[1], saved[2]);
							actual.start({ float(saved[0]) / 64, float(saved[1]) / 64, float(saved[2]) / 64 });
						}
						gameData.timeData.xGame += frame;
						if (tick == 10) gameData.timeData.xGame = 0;
						random_calls = 0;
						random_value = seed;
						audio.sound.reset();
						recorded.reset();
						gameData.demoData.nState = ND_STATE_RECORDING;
						paletteManager.FadeEffect();
						paletteManager.UpdateEffect();
						int calls = 0;
						auto result = actual.frame(frame, gameData.timeData.xGame, [&] {++calls;return seed; });
						if (actual.color != paletteManager.m_data.effect.rgb || actual.delay != paletteManager.m_data.xFadeDelay ||
						    actual.remaining != paletteManager.m_data.xFadeDuration[0] || actual.last_sound != paletteManager.m_data.xLastEffectTime ||
						    calls != random_calls || result.sound != audio.sound || result.demo != recorded || actual.saved_color() != std::array<int, 3>{ paletteManager.RedEffect(true), paletteManager.GreenEffect(true), paletteManager.BlueEffect(true) })
							throw std::runtime_error("Camera flash palette timing differs from source");
						++ticks;
					}
				}
	std::cout << "Camera flash source palette: " << pulses << " player pulses and " << ticks << " composed/save-restored frames passed\n";
}
