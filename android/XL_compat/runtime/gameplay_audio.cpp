#include "gameplay_audio.h"
#include "linked_audio.h"
#include "shared_audio.h"
#include "preview.h"
#include <algorithm>
extern "C" {
#include "inferno.h"
#include "sounds.h"
#include "args.h"
#include "config.h"
}
static_assert(MAX_SOUNDS == xl_compat::audio_logical_sound_count);
extern "C" int xl_gameplay_sound_sample(int logical)
{
	if (!xl_level_extensions_active()) return -1;
	return xl_compat::audio_translate_sound(logical, Sounds, AltSounds, GameArg.SysLowMem != 0);
}
extern "C" int xl_gameplay_sound_play(int sound, int sound_class, int volume, int pan, int loops)
{
	if (!xl_level_extensions_active() || !volume) return -1;
	// Only PlaySound accepts negative physical IDs; linked creation translates
	const int sample = sound < 0 ? (sound > -MAX_SOUNDS ? -sound : -1) : xl_gameplay_sound_sample(sound);
	return xl_audio_start_sample(sample, sound_class, volume, pan, 0, loops > 0 ? loops - 1 : -1, -1);
}
extern "C" std::int64_t xl_gameplay_sound_object(int sound, int sound_class, int object, int forever, int volume, int range, int loop_start, int loop_end, int decay)
{
	return xl_audio_link_object(xl_gameplay_sound_sample(sound), sound_class, object, forever, volume, range, loop_start, loop_end, decay, 0);
}
extern "C" std::int64_t xl_gameplay_sound_position(int sound, int segment, int side, const vms_vector *position, int forever, int volume, int range)
{
	const int sample = xl_gameplay_sound_sample(sound);
	if (sample < 0 || volume < 0) return -1;
	// Source profile/menu derives custom volume from the integer main slider
	if (sound == SOUND_FORCEFIELD_HUM) volume = xl_compat::audio_forcefield_volume(volume, (std::clamp) (int(GameCfg.DigiVolume), 0, 8) * 8192);
	return xl_audio_link_position(sample, segment, side, position, forever, volume, range);
}
