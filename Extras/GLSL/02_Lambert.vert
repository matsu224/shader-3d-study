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

void main()
{
    // HLSL: TransformObjectToWorldNormal(normalOS)
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    normalWS = normalMatrix * normalOS;

    // HLSL: TransformObjectToHClip(positionOS)
    gl_Position = projection * view * model * vec4(positionOS, 1.0);
}
