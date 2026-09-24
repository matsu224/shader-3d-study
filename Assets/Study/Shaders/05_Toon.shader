Shader "Custom/05_Toon"
{
    Properties
    {
        [MainColor] _BaseColor("Base Color", Color) = (1, 1, 1, 1)
        [MainTexture] _BaseMap("Base Map", 2D) = "white" {}
        _Threshold1("Threshold 1", Range(0, 1)) = 0.4
        _Threshold2("Threshold 2", Range(0, 1)) = 0.7
        _Softness("Softness", Range(0, 0.5)) = 0.01
        _DarkColor("Dark Color", Color) = (0.1, 0.1, 0.1, 1)
        _MidColor("Mid Color", Color) = (0.5, 0.5, 0.5, 1)
        _LightColor("Light Color", Color) = (1, 1, 1, 1)
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
            };

            struct Varyings
            {
                float4 positionHCS : SV_POSITION;
                float2 uv : TEXCOORD0;
                float3 positionWS : TEXCOORD1;
                float3 normalWS : TEXCOORD2;
                float4 positionOS : TEXCOORD3;
                float3 normalOS : TEXCOORD4;
            };

            TEXTURE2D(_BaseMap);
            SAMPLER(sampler_BaseMap);

            CBUFFER_START(UnityPerMaterial)
                half4 _BaseColor;
                float4 _BaseMap_ST;
                float _Threshold1;
                float _Threshold2;
                float _Softness;
                half4 _DarkColor;
                half4 _MidColor;
                half4 _LightColor;
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
                return OUT;
            }

            half4 frag(Varyings IN) : SV_Target
            {
                Light light = GetMainLight();
                float3 N = normalize(IN.normalWS);
                float3 L = normalize(light.direction);
                float ndotl = saturate(dot(N, L));
                //float toon1 = step(_Threshold1, ndotl);
                //float toon2 = step(_Threshold2, ndotl);
                float toon1 = smoothstep(_Threshold1 - _Softness, _Threshold1 + _Softness, ndotl);
                float toon2 = smoothstep(_Threshold2 - _Softness, _Threshold2 + _Softness, ndotl);

                float3 color = _DarkColor.rgb;
                color = lerp(color, _MidColor.rgb, toon1);
                color = lerp(color, _LightColor.rgb, toon2);

                return float4(color, 1.0);
            }
            ENDHLSL
        }
    }

    Fallback Off
}
