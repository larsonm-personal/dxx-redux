#ifndef ANDROID_FONT_SCALE_H
#define ANDROID_FONT_SCALE_H

int android_font_scale_active(void);
int android_internal_string_scaled_linear(int x, int y, const char *s, int masked);

#endif /* ANDROID_FONT_SCALE_H */
