#ifndef DXX_XL_SHARED_AUDIO_H
#define DXX_XL_SHARED_AUDIO_H
#ifdef __cplusplus
extern "C" {
#endif
// Physical native sample indices; callers own logical sound translation and class
int xl_audio_start_sample(int sample, int sound_class, int volume, int pan, int looping, int loop_start, int loop_end);
int xl_audio_channel_playing(int slot);
void xl_audio_stop_channel(int slot);
int xl_audio_set_channel_volume(int slot, int volume);
int xl_audio_set_channel_pan(int slot, int pan);
#ifdef __cplusplus
}
#endif
#endif
