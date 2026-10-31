#ifndef DXX_XL_LIGHTNING_BLUR_SHADER_H
#define DXX_XL_LIGHTNING_BLUR_SHADER_H
// Pinned active glow.cpp blurFS, with an explicit portable texture varying
inline constexpr char xl_lightning_blur_glsl[] = R"GLSL(
uniform sampler2D glowSource;
uniform float direction, scale, brightness;
varying vec2 glowUV;
void main() {
    float xScale = (1.0 - direction) * scale, yScale = direction * scale;
    vec3 tc = texture2D(glowSource, glowUV).rgb * 0.2270270270;
    vec2 v = vec2(xScale, yScale);
    tc += texture2D(glowSource, glowUV + v).rgb * 0.3162162162;
    tc += texture2D(glowSource, glowUV - v).rgb * 0.3162162162;
    v = vec2(2.0 * xScale, 2.0 * yScale);
    tc += texture2D(glowSource, glowUV + v).rgb * 0.0702702703;
    tc += texture2D(glowSource, glowUV - v).rgb * 0.0702702703;
    gl_FragColor = vec4(tc, 1.0) * brightness;
}
)GLSL";
#endif
