#include "gameplay_audio.h"
#include "linked_audio.h"
#include "shared_audio.h"
#include "shared_audio_internal.h"
#include "gameplay_audio_state.h"
#include "preview.h"
#include <algorithm>
extern "C" {
#include "inferno.h"
#include "sounds.h"
#include "args.h"
#include "config.h"
#include "newdemo.h"
#include "digi.h"
int digi_unxlat_sound(int sound);
}
static_assert(MAX_SOUNDS == xl_compat::audio_logical_sound_count);
namespace xl_compat::audio_runtime
{
LoopingAudioState looping;
}
extern "C" void xl_gameplay_loop_stop(void)
{
	using namespace xl_compat::audio_runtime;
	AudioLock lock;
	if (looping.matches(mixer)) mixer.stop(looping.channel);
	looping = {};
}
extern "C" void xl_gameplay_loop_start(int sound, int volume, int loop_start, int loop_end)
{
	using namespace xl_compat::audio_runtime;
	const int sample = xl_gameplay_sound_sample(sound);
	// Invalid translation leaves the previous loop intact, as in the source
	if (sample < 0 || volume < 0) return;
	xl_gameplay_loop_stop();
	// Keep fixed-point volume full-width; the source's int16_t field truncates it
	const int channel = xl_audio_start_sample(sample, XL_SOUND_GENERIC, volume, 32767, 1, loop_start, loop_end);
	AudioLock lock;
	if (channel >= 0) looping = { channel, mixer.voices[unsigned(channel)].serial };
}
extern "C" void xl_gameplay_loop_volume(int volume)
{
	using namespace xl_compat::audio_runtime;
	if (!xl_level_extensions_active() || volume < 0) return;
	AudioLock lock;
	if (looping.matches(mixer)) mixer.set_volume(looping.channel, volume, priority_volume);
	else looping = {};
}
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
	return xl_audio_link_object_recorded(xl_gameplay_sound_sample(sound), sound_class, object, forever, volume, range, loop_start, loop_end, decay, 0, sound);
}
extern "C" std::int64_t xl_gameplay_sound_position(int sound, int segment, int side, const vms_vector *position, int forever, int volume, int range)
{
	const int sample = xl_gameplay_sound_sample(sound);
	if (sample < 0 || volume < 0) return -1;
	// Source profile/menu derives custom volume from the integer main slider
	if (sound == SOUND_FORCEFIELD_HUM) volume = xl_compat::audio_forcefield_volume(volume, (std::clamp) (int(GameCfg.DigiVolume), 0, 8) * 8192);
	return xl_audio_link_position_recorded(sample, segment, side, position, forever, volume, range, sound);
}

extern "C" int xl_gameplay_link_object(int sound, int object, int forever, int volume, int range, int loop_start, int loop_end)
{
	const auto token = xl_gameplay_sound_object(sound, XL_SOUND_GENERIC, object, forever, volume, range, loop_start, loop_end, 0);
	// No engine caller resolves this token; ownership and serialization use the
	// full serial. Keep successful legacy return values nonnegative after wrap
	return token < 0 ? -1 : int(std::uint64_t(token) & INT32_MAX);
}
extern "C" int xl_gameplay_link_position(int sound, int segment, int side, const vms_vector *position, int forever, int volume, int range)
{
	const auto token = xl_gameplay_sound_position(sound, segment, side, position, forever, volume, range);
	return token < 0 ? -1 : int(std::uint64_t(token) & INT32_MAX);
}
extern "C" void xl_gameplay_record_objects(void)
{
	using namespace xl_compat;
	using namespace xl_compat::audio_runtime;
	for (unsigned i = 0; i < owners.count; ++i) {
		const auto &entry = owners.entries[i];
		if ((entry.flags & (sound_link_object | sound_forever)) == (sound_link_object | sound_forever))
			newdemo_record_link_sound_to_object3(digi_unxlat_sound(entry.sound), entry.object, entry.max_volume, entry.max_distance, entry.loop_start, entry.loop_end);
	}
}
