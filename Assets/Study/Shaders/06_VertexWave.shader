Shader "Custom/06_VertexWave"
{
    Properties
    {
        [MainColor] _BaseColor("Base Color", Color) = (1, 1, 1, 1)
        [MainTexture] _BaseMap("Base Map", 2D) = "white" {}
        _Amplitude("Amplitude", Range(0, 1)) = 0.2
        _Frequency("Frequency", Float) = 5.0
        _Speed("Speed", Float) = 2.0
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
                float _Amplitude;
                float _Frequency;
                float _Speed;
            CBUFFER_END

            Varyings vert(Attributes IN)
            {
                Varyings OUT;

                float4 pos = IN.positionOS;
                float phasex = pos.x * _Frequency + _Time.y * _Speed;
                float phasez = pos.z * _Frequency + _Time.y * _Speed;
                pos.y += (sin(phasex) + sin(phasez)) * _Amplitude;
                float slopex = cos(phasex) * _Frequency * _Amplitude; //波の傾きからオブジェクト空間法線を計算
                float slopez = cos(phasez) * _Frequency * _Amplitude; //波の傾きからオブジェクト空間法線を計算
                float3 normalOS = normalize(float3(-slopex, 1.0, -slopez));

                OUT.positionHCS = TransformObjectToHClip(pos);
                OUT.positionWS = TransformObjectToWorld(pos);
                OUT.positionOS = pos;
                OUT.normalOS = normalOS;
                OUT.normalWS = TransformObjectToWorldNormal(normalOS);

                OUT.uv = TRANSFORM_TEX(IN.uv, _BaseMap);

                return OUT;
            }

            half4 frag(Varyings IN) : SV_Target
            {
                float3 N = normalize(IN.normalWS);
                float3 Ncolor = N * 0.5 + 0.5;

                return half4(Ncolor, 1.0);
            }
            ENDHLSL
        }
    }

    Fallback Off
}
