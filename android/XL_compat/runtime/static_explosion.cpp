#include "static_explosion_runtime.h"
#include "../static_explosion.h"
#include "gameplay_audio.h"
#include "preview.h"
#include <cstring>
extern "C" {
#include "inferno.h"
#include "object.h"
#include "vclip.h"
#include "piggy.h"
#include "sounds.h"
#include "args.h"
#include "game.h"
#include "gr.h"
#include "playsave.h"
#include "dxxerror.h"
}
namespace
{
const vclip &checked_clip(const object &object)
{
	const auto &state = object.rtype.vclip_info;
	if (state.vclip_num < 0 || state.vclip_num >= Num_vclips) throw std::runtime_error("XL static explosion references a missing clip");
	const auto &clip = Vclip[state.vclip_num];
	if (clip.num_frames <= 0 || clip.num_frames > VCLIP_MAX_FRAMES || clip.frame_time <= 0 || state.framenum < 0 || state.framenum >= clip.num_frames)
		throw std::runtime_error("XL static explosion has an invalid animation state");
	return clip;
}
} // namespace
extern "C" int xl_static_explosion_sound(void)
{
	// Redux's missing-name sentinel is 255; use the registry to retain XL's -1
	int sample = -1;
	for (int index = 0; const char *name = piggy_sound_name(index); ++index)
		if (!std::strcmp(name, "explode2")) {
			sample = index;
			break;
		}
	return xl_compat::static_explosion_logical_sound(sample, GameArg.SysLowMem ? AltSounds : Sounds, MAX_SOUNDS);
}
extern "C" void xl_static_explosions_start(void)
{
	if (!xl_level_extensions_active()) return;
	const int sound = xl_static_explosion_sound();
	if (sound < 0) return;
	for (int index = 0; index <= Highest_object_index; ++index) {
		auto &object = Objects[index];
		if (object.type != 19) continue;
		object.render_type = RT_POWERUP;
		object.rtype.vclip_info.vclip_num = object.id;
		xl_gameplay_sound_object(sound, 1, index, 1, F1_0, 256 * F1_0, -1, -1, 0);
	}
}
extern "C" int xl_static_explosion_animate(object *object)
{
	if (!xl_level_extensions_active() || object->type != 19) return 0;
	try {
		const auto &clip = checked_clip(*object);
		auto &state = object->rtype.vclip_info;
		const auto next = xl_compat::advance_static_explosion({ state.frametime, state.framenum }, clip.num_frames, clip.frame_time, FrameTime);
		state.frametime = next.remaining;
		state.framenum = static_cast<sbyte>(next.frame);
	} catch (const std::exception &error) {
		Error("XL static explosion update failed: %s", error.what());
	}
	return 1;
}
extern "C" int xl_static_explosion_draw(object *object)
{
	if (!xl_level_extensions_active() || object->type != 19) return 0;
	try {
		const auto &clip = checked_clip(*object);
		gr_settransblend(GR_FADE_OFF, PlayerCfg.AlphaEffects ? GR_BLEND_ADDITIVE_C : GR_BLEND_NORMAL);
		draw_object_blob(object, clip.frames[object->rtype.vclip_info.framenum]);
	} catch (const std::exception &error) {
		Error("XL static explosion draw failed: %s", error.what());
	}
	return 1;
}
