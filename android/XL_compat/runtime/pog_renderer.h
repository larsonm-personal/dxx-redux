#ifndef DXX_XL_POG_RENDERER_H
#define DXX_XL_POG_RENDERER_H
#ifdef __cplusplus
extern "C" {
#endif
struct _grs_bitmap;
struct xl_rgba_alpha_state {
	int function;
	float reference;
};
void ogl_xl_rgba_alpha_begin(int active, struct xl_rgba_alpha_state *state);
void ogl_xl_rgba_alpha_end(const struct xl_rgba_alpha_state *state);
const char *ogl_xl_rgba_prepare(int width, int height);
const char *ogl_xl_rgba_load(struct _grs_bitmap *bitmap, int filter, const char *name);
#ifdef __cplusplus
}
#endif
#endif
