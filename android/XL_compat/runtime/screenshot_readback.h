#ifndef DXX_XL_SCREENSHOT_READBACK_H
#define DXX_XL_SCREENSHOT_READBACK_H

#ifdef __cplusplus
extern "C" {
#endif

/* Desktop GL state for one native screenshot; image writing stays in arch_ogl */
typedef struct xl_screenshot_readback_state {
	int previous_read_buffer;
	int previous_read_framebuffer;
	int capture_read_buffer;
	int framebuffer_readback;
} xl_screenshot_readback_state;

#ifndef OGLES
void xl_screenshot_readback_begin(xl_screenshot_readback_state *state);
void xl_screenshot_readback_end(const xl_screenshot_readback_state *state);
#endif

#ifdef __cplusplus
}
#endif
#endif
