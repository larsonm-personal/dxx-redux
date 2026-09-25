#ifndef SOUND_MIXER_CONVERT_H
#define SOUND_MIXER_CONVERT_H

#include <SDL_audio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* Convert original unsigned 8-bit mono PCM at its recorded rate
 * The successful output is owned by the caller and released with free */
int sound_mixer_convert(const Uint8 *input, size_t length, int source_rate,
                        int output_rate, Uint16 format, int channels,
                        Uint8 **output, Uint32 *output_length);

#ifdef __cplusplus
}
#endif
#endif
