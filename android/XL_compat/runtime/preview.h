#ifndef DXX_XL_PREVIEW_H
#define DXX_XL_PREVIEW_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
void xl_preview_vertex(int vertex, int32_t light, int32_t *r, int32_t *g, int32_t *b);
int xl_preview_start(void);
void xl_preview_before_swap(void);
uint64_t xl_preview_vertex_calls(void);
#ifdef __cplusplus
}
#endif
#endif
