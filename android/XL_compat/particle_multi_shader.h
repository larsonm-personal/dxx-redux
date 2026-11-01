#ifndef DXX_XL_PARTICLE_MULTI_SHADER_H
#define DXX_XL_PARTICLE_MULTI_SHADER_H

// Pinned non-array particleFS equations; runtime ownership and submission are
// separate consumers. The caller uses source alpha-controlled blend factors
inline constexpr char xl_particle_multi_glsl[] = R"GLSL(
uniform sampler2D particleTex, sparkTex, bubbleTex, depthTex;
uniform vec2 windowScale;
uniform vec3 dMax;
uniform int softDepth, bSuspended;
varying vec3 particleUV;
varying vec4 particleColor;
float eyeDepth(float depth) {
    return 10000.0 / (5001.0 + (2.0 * depth - 1.0) * -4999.0);
}
void main() {
    if (bSuspended != 0) {
        gl_FragColor = texture2D(particleTex, particleUV.xy) * particleColor;
    } else {
        int kind = int(floor(particleUV.z + 0.5));
        float fade = 1.0;
        if (softDepth != 0) {
            kind = int(clamp(floor(particleUV.z + 0.5), 0.0, 2.0));
            float maximum = dMax[kind];
            float delta = clamp(eyeDepth(gl_FragCoord.z) - eyeDepth(texture2D(depthTex, gl_FragCoord.xy * windowScale).r), 0.0, maximum);
            fade = (maximum - delta) / maximum;
        }
        vec4 color = kind == 0 ? texture2D(sparkTex, particleUV.xy) : kind == 1 ? texture2D(particleTex, particleUV.xy) : texture2D(bubbleTex, particleUV.xy);
        if (kind == 0) color.rgb *= color.a;
        color *= particleColor * fade;
        if (particleColor.a == 0.0)
            gl_FragColor = vec4(color.rgb, 1.0);
        else
            gl_FragColor = vec4(color.rgb * color.a, 1.0 - color.a);
    }
}
)GLSL";
#endif
