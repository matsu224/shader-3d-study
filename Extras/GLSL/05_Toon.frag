#version 450 core

layout(location = 0) in vec3 normalWS;
layout(location = 0) out vec4 fragColor;

// HLSL: GetMainLight().direction; World Space, point toward light.
uniform vec3 lightDirWS;
uniform float threshold1;
uniform float threshold2;
uniform float softness;
uniform vec3 darkColor;
uniform vec3 midColor;
uniform vec3 lightColor;

void main()
{
    // N and L are both in World Space.
    vec3 N = normalize(normalWS);
    vec3 L = normalize(lightDirWS);

    // HLSL: saturate(dot(N, L))
    float ndotl = clamp(dot(N, L), 0.0, 1.0);
    float toon1 = smoothstep(threshold1 - softness,
                             threshold1 + softness,
                             ndotl);
    float toon2 = smoothstep(threshold2 - softness,
                             threshold2 + softness,
                             ndotl);

    vec3 color = darkColor;
    color = mix(color, midColor, toon1);   // HLSL: lerp
    color = mix(color, lightColor, toon2); // HLSL: lerp
    fragColor = vec4(color, 1.0);
}
