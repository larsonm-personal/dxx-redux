#ifndef DXX_ROUTE_CONFIRMATION_RESULT_H
#define DXX_ROUTE_CONFIRMATION_RESULT_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

int route_confirmation_write_json(const char *path, const char *mission,
                                  int level, char *error,
                                  size_t error_size);

#ifdef __cplusplus
}
#endif

#endif
