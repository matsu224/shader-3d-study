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
    // Object Space normalをWorld Spaceへ変換する。
    // inverse transposeにより、Model matrixへ非一様Scaleが含まれてもnormalの向きを補正できる。
    // HLSL: TransformObjectToWorldNormal(normalOS)
    mat3 normalMatrix = transpose(inverse(mat3(model)));
    vec3 N = normalize(normalMatrix * normalOS);

    // Tangentは面上のUVのU方向を表す。normalとは異なり、方向vectorとしてModel変換する。
    // HLSL: TransformObjectToWorldDir(tangentOS.xyz)
    vec3 T = normalize(mat3(model) * tangentOS.xyz);

    // 変換後の誤差や非一様Scaleによる傾きを除き、TをNへ再度直交させる。
    T = normalize(T - N * dot(N, T));

    // tangentOS.wとModel matrixの反転状態から、Bitangentをどちら向きに作るか決める。
    // HLSL: float modelSign = GetOddNegativeScale();
    float modelSign = determinant(mat3(model)) < 0.0 ? -1.0 : 1.0;
    float tangentSign = tangentOS.w * modelSign;
    vec3 B = normalize(cross(N, T)) * tangentSign;

    // Fragment Shaderが各pixelでTBNを組み立てられるよう、UVと3つの軸を渡す。
    normalMapUV = uv;
    tangentWS = T;
    bitangentWS = B;
    normalWS = N;

    // Object Spaceの頂点をModel -> View -> Projectionの順でClip Spaceへ変換する。
    // HLSL: TransformObjectToHClip(positionOS)
    gl_Position = projection * view * model * vec4(positionOS, 1.0);
}
