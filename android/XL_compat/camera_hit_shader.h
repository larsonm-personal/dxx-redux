#ifndef DXX_XL_CAMERA_HIT_SHADER_H
#define DXX_XL_CAMERA_HIT_SHADER_H
// Source sphere hit fragment equation; callers provide local vertex coordinates
inline constexpr char xl_camera_hit_glsl[] = R"GLSL(
uniform sampler2D sphereTex;
uniform vec4 vHit[3];
uniform vec3 fRad;
varying vec3 hitVertex;
varying vec2 hitUV;
varying vec4 hitColor;
#ifndef XL_SHIELD_UV
#define XL_SHIELD_UV hitUV
#endif
void main() {
    vec3 scale;
    scale.x = 1.0 - clamp(length(hitVertex - vec3(vHit[0])) * fRad.x, 0.0, 1.0);
    scale.y = 1.0 - clamp(length(hitVertex - vec3(vHit[1])) * fRad.y, 0.0, 1.0);
    scale.z = 1.0 - clamp(length(hitVertex - vec3(vHit[2])) * fRad.z, 0.0, 1.0);
    gl_FragColor = texture2D(sphereTex, XL_SHIELD_UV) * hitColor * max(scale.x, max(scale.y, scale.z));
}
)GLSL";
#endif
