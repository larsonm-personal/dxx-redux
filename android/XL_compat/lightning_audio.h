#ifndef DXX_XL_LIGHTNING_AUDIO_H
#define DXX_XL_LIGHTNING_AUDIO_H
namespace xl_compat
{
inline constexpr const char *lightning_sound_name = "08:lightning.wav";
enum class LightningSoundAction { none,
	                              create,
	                              destroy };

inline LightningSoundAction lightning_sound_update(int &sound, bool enabled, bool active_nodes)
{
	if (!enabled || !sound) return LightningSoundAction::none;
	if (active_nodes) {
		if (sound >= 0) return LightningSoundAction::none;
		sound = 1;
		return LightningSoundAction::create;
	}
	if (sound < 0) return LightningSoundAction::none;
	sound = -1;
	return LightningSoundAction::destroy;
}
inline void lightning_sound_mute(int &sound)
{
	if (sound) sound = -1;
}
} // namespace xl_compat
#endif
