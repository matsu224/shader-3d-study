Shader "Custom/07_NormalMap"
{
    Properties
    {
        [MainColor] _BaseColor("Base Color", Color) = (1, 1, 1, 1)
        [MainTexture] _BaseMap("Base Map", 2D) = "white" {}
        _NormalMap("Normal Map", 2D) = "bump" {}
    }

    SubShader
    {
        Tags
        {
            "RenderType" = "Opaque"
            "Queue" = "Geometry"
            "RenderPipeline" = "UniversalPipeline"
        }

        Pass
        {
            Name "Forward"
            Tags { "LightMode" = "UniversalForwardOnly" }

            HLSLPROGRAM

            #pragma vertex vert
            #pragma fragment frag

            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Core.hlsl"
            #include "Packages/com.unity.render-pipelines.universal/ShaderLibrary/Lighting.hlsl"

            struct Attributes
            {
                float4 positionOS : POSITION;
                float2 uv : TEXCOORD0;
                float3 normalOS : NORMAL;
                float4 tangentOS : TANGENT;
            };

            struct Varyings
            {
                float4 positionHCS : SV_POSITION;
                float2 uv : TEXCOORD0;
                float3 positionWS : TEXCOORD1;
                float3 normalWS : TEXCOORD2;
                float4 positionOS : TEXCOORD3;
                float3 normalOS : TEXCOORD4;
                float3 tangentWS : TEXCOORD5;
                float3 bitangentWS : TEXCOORD6;
                float2 normalMapUV : TEXCOORD7;
            };

            TEXTURE2D(_BaseMap);
            SAMPLER(sampler_BaseMap);
            TEXTURE2D(_NormalMap);
            SAMPLER(sampler_NormalMap);

            CBUFFER_START(UnityPerMaterial)
                half4 _BaseColor;
                float4 _BaseMap_ST;
                float4 _NormalMap_ST;
            CBUFFER_END

            Varyings vert(Attributes IN)
            {
                Varyings OUT;
                OUT.uv = TRANSFORM_TEX(IN.uv, _BaseMap);
                OUT.positionHCS = TransformObjectToHClip(IN.positionOS.xyz);
                OUT.positionWS = TransformObjectToWorld(IN.positionOS.xyz);
                OUT.normalWS = TransformObjectToWorldNormal(IN.normalOS);
                OUT.positionOS = IN.positionOS;
                OUT.normalOS = IN.normalOS;
                OUT.normalMapUV = TRANSFORM_TEX(IN.uv, _NormalMap);

                float3 N =
                    normalize(
                        TransformObjectToWorldNormal(IN.normalOS)
                    );

                float3 T =
                    normalize(
                        TransformObjectToWorldDir(IN.tangentOS.xyz)
                    );

                float tangentSign =
                    IN.tangentOS.w * GetOddNegativeScale();

                float3 B =
                    normalize(cross(N, T)) * tangentSign;

                OUT.normalWS = N;
                OUT.tangentWS = T;
                OUT.bitangentWS = B;

                return OUT;
            }

            half4 frag(Varyings IN) : SV_Target
            {
                //Normal Mapから法線を読む
                float2 normalMapUV =
                    IN.normalMapUV;

                float3 normalTS =
                    UnpackNormal(
                        SAMPLE_TEXTURE2D(
                            _NormalMap,
                            sampler_NormalMap,
                            normalMapUV
                        )
                    );

                //T/B/Nを正規化
                float3 T = normalize(IN.tangentWS);
                float3 B = normalize(IN.bitangentWS);
                float3 N = normalize(IN.normalWS);

                //Tangent Space NormalをWorld Spaceへ変換
                float3 normalWS =
                    normalize(
                        T * normalTS.x +
                        B * normalTS.y +
                        N * normalTS.z
                    );

                //Lambert
                Light light = GetMainLight();
                float3 L = normalize(light.direction);

                float ndotl = saturate(dot(normalWS, L));

                float3 color = _BaseColor.rgb * ndotl;

                return half4(color, 1.0);
            }
            ENDHLSL
        }
    }

    Fallback Off
}

//TBNなどさらに理解を深める
