#include "gameplay_audio.h"
#include "gameplay_audio_state.h"
#include "seismic_audio_state.h"
#include "shared_audio_internal.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "game.h"
#include "weapon.h"
#include "rng_shared.h"
extern int Seismic_sound, Seismic_sound_playing, Seismic_tremor_volume;
extern fix64 Next_seismic_sound_time;
}
static_assert(SEISMIC_RUNTIME_DETONATION_SLOTS == 4, "Update the seismic audio save volume bound when adding detonation slots");
xl_compat::SeismicAudioState xl_compat::seismic_audio_snapshot()
{
	return { Next_seismic_sound_time, Seismic_tremor_volume };
}
void xl_compat::restore_seismic_audio(SeismicAudioState state)
{
	Next_seismic_sound_time = state.next_volume_time;
	Seismic_tremor_volume = state.previous_volume;
}
void xl_compat::reset_seismic_audio()
{
	restore_seismic_audio({});
	Seismic_sound_playing = 0;
}
namespace
{
bool playing()
{
	using namespace xl_compat::audio_runtime;
	AudioLock lock;
	return looping.matches(mixer);
}
} // namespace
extern "C" void xl_gameplay_seismic_begin(void)
{
	// The legacy flag is not saved with voices and can outlive a retired channel
	if (xl_level_extensions_active()) Seismic_sound_playing = playing();
}
extern "C" void xl_gameplay_seismic_end(void)
{
	if (!xl_level_extensions_active()) return;
	if (Seismic_tremor_volume <= 0) {
		xl_gameplay_loop_stop();
		Seismic_sound_playing = 0;
	} else if (!playing()) {
		// A restored ongoing level disturbance does not re-enter its start event
		xl_gameplay_loop_start(Seismic_sound, F1_0, -1, -1);
		Seismic_sound_playing = playing();
		if (Seismic_sound_playing) Next_seismic_sound_time = GameTime64 + d_rand_fx() / 2;
	}
}
