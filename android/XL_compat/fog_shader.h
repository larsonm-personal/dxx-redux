#ifndef DXX_XL_FOG_SHADER_H
#define DXX_XL_FOG_SHADER_H

// The active fog profile supplies the reference projection planes consistently
// to scene rendering and depth decoding. Distances are view-axis world units
inline constexpr const char *xl_fog_math_glsl = R"glsl(
uniform vec2 scenePlanes;
float EyeDepth(float depth) {
    return 2.0 * scenePlanes.x * scenePlanes.y /
        (scenePlanes.x + scenePlanes.y + (2.0 * depth - 1.0) * (scenePlanes.x - scenePlanes.y));
}
vec4 FogColor(vec4 color, float sceneDepth, float nearDepth, float farDepth) {
    if (color.a == 0.0) return vec4(1.0, 1.0, 1.0, 0.0);
    // Reference's clear near/far pair becomes camera-inside when near > far
    float start = nearDepth > farDepth ? 1.0 : EyeDepth(nearDepth);
    float length = EyeDepth(farDepth) - start;
    float visible = sceneDepth - start;
    if (length <= 0.0 || visible <= 0.0) return vec4(1.0, 1.0, 1.0, 0.0);
    return vec4(color.rgb, min(1.0, min(length, visible) / color.a));
}
vec4 FogPair(vec4 first, vec4 second, float sceneDepth, vec4 volume) {
    vec4 a = FogColor(first, sceneDepth, volume.r, volume.g);
    vec4 b = FogColor(second, sceneDepth, volume.b, volume.a);
    return vec4(a.rgb * b.rgb, min(1.0, a.a + b.a));
}
)glsl";

inline constexpr const char *xl_fog_volume_glsl = R"glsl(
uniform sampler2D sceneDepthTex;
uniform vec2 windowSize;
void main() {
    if (gl_FragCoord.z > texture2D(sceneDepthTex, gl_FragCoord.xy / windowSize).r) discard;
    gl_FragColor = vec4(gl_FragCoord.z);
}
)glsl";

inline constexpr const char *xl_fog_composite_glsl = R"glsl(
uniform sampler2D sceneDepthTex, fogTex;
uniform vec2 windowSize;
uniform vec4 fogColor1, fogColor2;
void main() {
    vec2 uv = gl_FragCoord.xy / windowSize;
    gl_FragColor = FogPair(fogColor1, fogColor2,
        EyeDepth(texture2D(sceneDepthTex, uv).r), texture2D(fogTex, uv));
}
)glsl";
#endif
