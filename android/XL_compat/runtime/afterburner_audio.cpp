#include "gameplay_audio.h"
#include "linked_audio.h"
#include "preview.h"
extern "C" {
#include "inferno.h"
#include "object.h"
#include "player.h"
#include "game.h"
#include "kconfig.h"
#include "endlevel.h"
#include "sounds.h"
#include "digi.h"
#include "args.h"
extern ubyte Last_afterburner_state;
extern fix Last_afterburner_charge;
}
extern "C" int xl_gameplay_afterburner(void)
{
	if (!xl_level_extensions_active()) return 0;
	auto &player = Players[Player_num];
	if (!(player.flags & PLAYER_FLAGS_AFTERBURNER)) player.afterburner_charge = 0;
	if (Endlevel_sequence || Player_is_dead) digi_kill_sound_linked_to_object(player.objnum);
	else {
		const int sample = xl_gameplay_sound_sample(SOUND_AFTERBURNER_IGNITE);
		const int owned = xl_audio_object_count(player.objnum, sample, 0);
		if (player.afterburner_charge && Controls.afterburner_state && (player.flags & PLAYER_FLAGS_AFTERBURNER)) {
			// A muted/stopped owner is still this loop; frame sync resumes it
			if (!owned) {
				const bool full_rate = GameArg.SndDigiSampleRate == SAMPLE_RATE_22K;
				xl_gameplay_sound_object(SOUND_AFTERBURNER_IGNITE, XL_SOUND_PLAYER, player.objnum, 1, F1_0, 256 * F1_0,
				                         full_rate ? 32027 : 32027 / 2, full_rate ? 48452 : 48452 / 2, 0);
			}
		} else if (owned) {
			const bool audible = xl_audio_object_count(player.objnum, sample, 1) != 0;
			xl_audio_destroy_object(player.objnum, sample);
			// Remove stopped loops too, so unmuting cannot resurrect a released key
			if (audible) xl_gameplay_sound_object(SOUND_AFTERBURNER_PLAY, XL_SOUND_PLAYER, player.objnum, 0, F1_0, 256 * F1_0, -1, -1, 0);
		}
	}
	Last_afterburner_state = Controls.afterburner_state;
	Last_afterburner_charge = player.afterburner_charge;
	return 1;
}
