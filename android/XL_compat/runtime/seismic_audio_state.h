#ifndef DXX_XL_SEISMIC_AUDIO_STATE_H
#define DXX_XL_SEISMIC_AUDIO_STATE_H
#include <cstdint>
namespace xl_compat
{
struct SeismicAudioState {
	std::int64_t next_volume_time = 0;
	int previous_volume = 0;
};
SeismicAudioState seismic_audio_snapshot();
void restore_seismic_audio(SeismicAudioState state);
void reset_seismic_audio();
} // namespace xl_compat
#endif
