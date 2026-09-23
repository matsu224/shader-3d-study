Shader "Custom/03_BlinnPhong"
{
    Properties
    {
        [MainColor] _BaseColor("Base Color", Color) = (1, 1, 1, 1)
        [MainTexture] _BaseMap("Base Map", 2D) = "white" {}
        _SpecularColor("Specular Color", Color) = (1, 1, 1, 1)
        _Shininess("Shininess", Float) = 32
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
                half4 _SpecularColor;
                float _Shininess;
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
                float3 L = normalize(light.direction); //シェーディング地点から光源側へ向かう方向
                float3 N = normalize(IN.normalWS);
                float ndotl = dot(N, L);
                float diffuse = saturate(ndotl);
                float3 V = normalize(_WorldSpaceCameraPos - IN.positionWS);
                float3 H = normalize(L + V);
                float ndoth = saturate(dot(N, H));
                float specular =
                    ndotl > 0.0
                    ? pow(ndoth, _Shininess)
                    : 0.0; //光が表面側に当たっているときだけSpecularを出す
                float3 colorA = _BaseColor.rgb * diffuse;
                float3 colorB = _SpecularColor.rgb * specular;
                float3 color = colorA + colorB;

                return float4(color, 1.0);
            }
            ENDHLSL
        }
    }

    Fallback Off
}
