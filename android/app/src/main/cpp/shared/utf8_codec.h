#ifndef DXX_UTF8_CODEC_H
#define DXX_UTF8_CODEC_H

#include <stddef.h>
#include <stdint.h>

int dxx_utf16_to_utf8(const uint16_t *input, size_t input_units,
                      char *output, size_t output_size, size_t *output_bytes,
                      int reject_null);
int dxx_utf8_to_utf16(const char *input, size_t input_bytes,
                      uint16_t *output, size_t output_units, size_t *written_units);

#endif
