#version 450 core

layout(location = 0) in vec3 normalWS;
layout(location = 0) out vec4 fragColor;

// HLSL: GetMainLight().direction, supplied by the application instead.
// Direction is from the shaded point toward the light, in World Space.
uniform vec3 lightDirWS;
// HLSL: _BaseColor.rgb
uniform vec3 baseColor;

void main()
{
    // N and L must both be in World Space.
    vec3 N = normalize(normalWS);
    vec3 L = normalize(lightDirWS);

    // HLSL: saturate(dot(N, L))
    float ndotl = max(dot(N, L), 0.0);
    fragColor = vec4(baseColor * ndotl, 1.0);
}
