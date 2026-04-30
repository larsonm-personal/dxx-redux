#ifndef INPUT_DEMO_CODEC_H
#define INPUT_DEMO_CODEC_H

#include <stddef.h>
#include <stdint.h>

#include <string>
#include <vector>

bool input_demo_base64_encode(const unsigned char *data,
                              size_t data_size,
                              std::string *encoded,
                              std::string *error);

bool input_demo_base64_decode(const std::string &encoded,
                              std::vector<uint8_t> *decoded,
                              std::string *error);

bool input_demo_sha256_hex(const unsigned char *data,
                           size_t data_size,
                           std::string *hex,
                           std::string *error);

#endif