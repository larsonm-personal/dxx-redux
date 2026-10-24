#ifndef DXX_XL_CORONA_PROBE_H
#define DXX_XL_CORONA_PROBE_H
#include <stdint.h>
// Explicit Android game-thread diagnostics; scene hook is inert until requested
#ifdef __cplusplus
extern "C" {
#endif
void xl_corona_pixel_probe(void);
const char *xl_corona_pixel_probe_result(void);
void xl_corona_scene_probe_request(void);
void xl_corona_scene_probe(const int32_t eye[3], int eye_segment);
const char *xl_corona_scene_probe_result(void);
void xl_corona_object_probe_request(void);
void xl_corona_object_probe(int after_objects, const int32_t eye[3]);
const char *xl_corona_object_probe_result(void);
#ifdef __cplusplus
}
#endif
#endif
