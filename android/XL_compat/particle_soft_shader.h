#ifndef DXX_XL_PARTICLE_SOFT_SHADER_H
#define DXX_XL_PARTICLE_SOFT_SHADER_H

// Pinned XL glareFS soft-particle equations, with explicit portable varyings
inline constexpr char xl_particle_soft_glsl[] = R"GLSL(
uniform sampler2D particleTex, depthTex;
uniform vec2 windowScale;
uniform int blendMode;
varying vec2 particleUV;
varying vec4 particleColor;
float eyeDepth(float depth) {
    return 10000.0 / (5001.0 + (2.0 * depth - 1.0) * -4999.0);
}
void main() {
    float delta = clamp(eyeDepth(gl_FragCoord.z) - eyeDepth(texture2D(depthTex, gl_FragCoord.xy * windowScale).r), 0.0, 5.0);
    float fade = (5.0 - delta) / 5.0;
    vec4 color = texture2D(particleTex, particleUV);
    if (blendMode > 0)
        gl_FragColor = vec4(color.rgb * particleColor.rgb * fade, 1.0);
    else if (blendMode < 0)
        gl_FragColor = vec4(max(color.rgb, 1.0 - fade), 1.0);
    else
        gl_FragColor = vec4(color.rgb * particleColor.rgb, color.a * particleColor.a * fade);
}
)GLSL";
#endif
