#version 450 core

// HLSL: float3 positionOS : POSITION
layout(location = 0) in vec3 positionOS;
// HLSL: float3 normalOS : NORMAL
layout(location = 1) in vec3 normalOS;

// HLSL: Varyings.normalWS : TEXCOORD
layout(location = 0) out vec3 normalWS;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;
uniform int outlinePass;
uniform float outlineWidth;

void main()
{
    // HLSL: TransformObjectToWorldNormal(normalOS)
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    normalWS = normalMatrix * normalOS;

    // Outline passだけ、Unity版と同じくObject Spaceのnormal方向へ頂点を押し出す。
    vec3 expandedPositionOS = positionOS;
    if (outlinePass != 0)
    {
        expandedPositionOS += normalOS * outlineWidth;
    }

    // HLSL: TransformObjectToHClip(expandedPositionOS)
    gl_Position = projection * view * model * vec4(expandedPositionOS, 1.0);
}
