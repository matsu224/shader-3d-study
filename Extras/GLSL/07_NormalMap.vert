#version 450 core

// HLSL Attributes: POSITION / TEXCOORD0 / NORMAL / TANGENT
layout(location = 0) in vec3 positionOS;
layout(location = 1) in vec2 uv;
layout(location = 2) in vec3 normalOS;
layout(location = 3) in vec4 tangentOS; // xyz: direction, w: handedness

// HLSL Varyings: TEXCOORD0 and tangent basis in World Space.
layout(location = 0) out vec2 normalMapUV;
layout(location = 1) out vec3 tangentWS;
layout(location = 2) out vec3 bitangentWS;
layout(location = 3) out vec3 normalWS;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // HLSL: TransformObjectToWorldNormal(normalOS)
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMatrix * normalOS);

    // HLSL: TransformObjectToWorldDir(tangentOS.xyz)
    vec3 T = normalize(mat3(model) * tangentOS.xyz);
    // Keep T perpendicular to N after interpolation/non-uniform scaling.
    T = normalize(T - N * dot(N, T));

    // HLSL: float modelSign = GetOddNegativeScale();
    float modelSign = determinant(mat3(model)) < 0.0 ? -1.0 : 1.0;
    float tangentSign = tangentOS.w * modelSign;
    vec3 B = normalize(cross(N, T)) * tangentSign;

    normalMapUV = uv;
    tangentWS = T;
    bitangentWS = B;
    normalWS = N;

    // HLSL: TransformObjectToHClip(positionOS)
    gl_Position = projection * view * model * vec4(positionOS, 1.0);
}
