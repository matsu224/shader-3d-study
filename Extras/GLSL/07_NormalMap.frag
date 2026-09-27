#version 450 core

layout(location = 0) in vec2 normalMapUV;
layout(location = 1) in vec3 tangentWS;
layout(location = 2) in vec3 bitangentWS;
layout(location = 3) in vec3 normalWS;
layout(location = 0) out vec4 fragColor;

// HLSL: TEXTURE2D(_NormalMap) + SAMPLER(sampler_NormalMap)
uniform sampler2D normalMap;
// HLSL: GetMainLight().direction; World Space, point toward light.
uniform vec3 lightDirWS;
// HLSL: _BaseColor.rgb
uniform vec3 baseColor;

void main()
{
    // Textureが保持するRGBは0～1だが、法線vectorには-1～1が必要なので範囲を変換する。
    // HLSL: UnpackNormal(SAMPLE_TEXTURE2D(...)).
    // This learning version assumes an RGB normal map: [0, 1] -> [-1, 1].
    vec3 normalTS = texture(normalMap, normalMapUV).xyz * 2.0 - 1.0;
    normalTS = normalize(normalTS);

    // Vertex Shaderから補間されたT/B/Nは長さや直交関係が崩れるため、pixelごとに整える。
    // T/B/N are the Tangent-Space axes expressed in World Space.
    vec3 T = normalize(tangentWS);
    vec3 N = normalize(normalWS);
    T = normalize(T - N * dot(N, T));
    vec3 B = normalize(bitangentWS - N * dot(N, bitangentWS));

    // Normal Mapから読んだTangent Space法線を、T/B/Nの各軸へ分解してWorld Spaceへ移す。
    // Tangent Space -> World Space. Equivalent to mat3(T, B, N) * normalTS.
    vec3 mappedNormalWS = normalize(normalTS.x * T
                                  + normalTS.y * B
                                  + normalTS.z * N);

    // 法線と光方向が両方World Spaceになったため、dotによるLambert計算ができる。
    // Both vectors are now in World Space, so Lambert is valid.
    vec3 L = normalize(lightDirWS);
    float ndotl = max(dot(mappedNormalWS, L), 0.0);
    fragColor = vec4(baseColor * ndotl, 1.0);
}
