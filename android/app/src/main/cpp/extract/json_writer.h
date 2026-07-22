#ifndef DXX_ANDROID_JSON_WRITER_H
#define DXX_ANDROID_JSON_WRITER_H

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/*
 * Writes one complete JSON string value. Valid UTF-8 is preserved, JSON
 * metacharacters and controls are escaped, and each malformed UTF-8 byte is
 * represented deterministically as the corresponding U+00XX code point
 */
int json_write_string(FILE *output, const char *value);

#ifdef __cplusplus
}
#endif

#endif
