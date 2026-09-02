#ifndef DXX_ROUTE_CONFIRMATION_DESKTOP_H
#define DXX_ROUTE_CONFIRMATION_DESKTOP_H

#ifdef __cplusplus
extern "C" {
#endif

/* Returns -1 when no desktop route was requested, 0 after starting one, and
 * 1 when a requested route could not start. */
int route_confirmation_desktop_maybe_start(void);
void route_confirmation_desktop_after_frame(void);
int route_confirmation_desktop_is_active(void);
int route_confirmation_desktop_should_exit(void);

#ifdef __cplusplus
}
#endif

#endif
