#ifndef DXX_ANDROID_AUDIO_CAPTURE_H
#define DXX_ANDROID_AUDIO_CAPTURE_H

#include <SDL_audio.h>

/* Diagnostic output tap; called under SDL's mixer lock, after all mixing */
typedef void (*android_audio_capture_callback)(const SDL_AudioSpec *, const Uint8 *, int, long long);
void androidaud_set_capture_callback(android_audio_capture_callback callback);

#endif
