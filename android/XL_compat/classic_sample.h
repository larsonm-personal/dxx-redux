#ifndef DXX_XL_CLASSIC_SAMPLE_H
#define DXX_XL_CLASSIC_SAMPLE_H
#include "audio_voice.h"

namespace xl_compat
{
// Preserve the pinned 44.1 kHz S16 source profile before device-rate conversion
AudioSample prepare_classic_sample(const std::uint8_t *bytes, std::size_t length, unsigned source_rate, unsigned output_rate);
} // namespace xl_compat
#endif
