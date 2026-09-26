#version 450 core

// HLSL: Varyings.normalWS : TEXCOORD
layout(location = 0) in vec3 normalWS;
// HLSL: fragment return value : SV_Target
layout(location = 0) out vec4 fragColor;

void main()
{
    vec3 N = normalize(normalWS);
    // World-space normal [-1, 1] -> display color [0, 1].
    fragColor = vec4(N * 0.5 + 0.5, 1.0);
}
