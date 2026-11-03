#ifndef DXX_XL_OOF_RENDERER_H
#define DXX_XL_OOF_RENDERER_H
#ifdef __cplusplus
extern "C" {
#endif
struct vms_vector;
struct vms_matrix;
struct g3s_lrgb;
int xl_oof_draw(int model, struct vms_vector *position, struct vms_matrix *orientation, struct g3s_lrgb *light);
void xl_oof_renderer_clear(void);
#ifdef __cplusplus
}
#endif
#endif
