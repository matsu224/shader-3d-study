# shader-3d-study

Unity URPを中心に、GLSLとC++／OpenGLも用いてシェーダを学習するためのリポジトリです。

3D mesh shaderの基本的な処理とPost Effectを、小さなシェーダに分けて実装しています。

## 学習方法

### Unity / HLSL

[Assets/Study](Assets/Study) 以下には、Unity URP上でShader／Materialを段階的に実装し、Scene上の表示結果を確認しながら学習した本体を置いています。AI生成コードの読解を主目的とする`Extras`とは異なり、Shaderの処理を自分で組み立てて検証する学習です。

### Extras/GLSL

[Extras/GLSL](Extras/GLSL) には、Unity/HLSLとの対応を比較するため、AIで変換・生成した素のGLSLを置いています。コードを一から実装することより、HLSLとの対応、Shader間の入出力、座標空間、計算内容を読み解くために使用します。

`08_PostEffect`のみ対応するUnity版を持たず、C++／OpenGLから使用するGLSLとして実装しています。

### Extras/CppShader

[Extras/CppShader](Extras/CppShader) には、GLSLの読み込み、compile、link、描画への接続を確認するため、AIが生成したC++／OpenGLコードを置いています。C++やOpenGLを一から実装することより、生成されたコードを読み解き、C++とGLSLの接続方法を理解するために使用します。

## Unity / HLSLで実装したシェーダ

### 00_Unlit

- シェーダ：
  - [00_Unlit.shader](Assets/Study/Shaders/00_Unlit.shader)
- マテリアル：
  - [M_00_Unlit.mat](Assets/Study/Materials/M_00_Unlit.mat)

最小構成のUnlit Shader。

Vertex ShaderからFragment Shaderまでの基本的なデータの流れや、UV・頂点位置などの入力を確認するために実装。

---

### 01_Normal

- シェーダ：
  - [01_Normal.shader](Assets/Study/Shaders/01_Normal.shader)
- マテリアル：
  - [M_01_Normal.mat](Assets/Study/Materials/M_01_Normal.mat)

World Space NormalをRGBとして可視化。

中心となる処理：

```hlsl
float3 N = normalize(IN.normalWS);
float3 color = N * 0.5 + 0.5;
```

Normalの各成分は基本的に `[-1, 1]` のため、RGBとして表示するために `[0, 1]` へ変換する。

---

### 02_Lambert

- シェーダ：
  - [02_Lambert.shader](Assets/Study/Shaders/02_Lambert.shader)
- マテリアル：
  - [M_02_Lambert.mat](Assets/Study/Materials/M_02_Lambert.mat)

World SpaceのSurface NormalとLight DirectionからLambert Diffuseを計算。

中心となる処理：

```hlsl
float ndotl = saturate(dot(N, L));
```

`N` と `L` を同じ座標空間に揃え、方向の一致度から拡散反射の明るさを求める。

---

### 03_BlinnPhong

- シェーダ：
  - [03_BlinnPhong.shader](Assets/Study/Shaders/03_BlinnPhong.shader)
- マテリアル：
  - [M_03_BlinnPhong.mat](Assets/Study/Materials/M_03_BlinnPhong.mat)

Blinn-PhongモデルによるSpecular Highlightを実装。

```hlsl
float3 H = normalize(L + V);
float specular = pow(saturate(dot(N, H)), shininess);
```

Surface Normal、Light Direction、View DirectionからHalf Vectorを求め、鏡面反射成分を計算する。

---

### 04_Fresnel

- シェーダ：
  - [04_Fresnel.shader](Assets/Study/Shaders/04_Fresnel.shader)
- マテリアル：
  - [M_04_Fresnel.mat](Assets/Study/Materials/M_04_Fresnel.mat)

Surface NormalとView Directionを利用したFresnel系の輪郭表現を実装。

```hlsl
float fresnel = 1.0 - saturate(dot(N, V));
```

`pow` を使い、輪郭部分の強さや幅を調整できるようにする。

---

### 05_Toon

- シェーダ：
  - [05_Toon.shader](Assets/Study/Shaders/05_Toon.shader)
- マテリアル：
  - [M_05_Toon.mat](Assets/Study/Materials/M_05_Toon.mat)

Lambert Diffuseを複数の閾値で区切ったToon Shadingと、頂点押し出しによるOutlineを実装。

```hlsl
float ndotl = saturate(dot(N, L));
float toon1 = smoothstep(_Threshold1 - _Softness, _Threshold1 + _Softness, ndotl);
float toon2 = smoothstep(_Threshold2 - _Softness, _Threshold2 + _Softness, ndotl);
```

Outline Passでは頂点をObject SpaceのNormal方向へ押し出し、`Cull Front`で裏面だけを描画する。通常サイズのToon Passを重ねることで、外側にはみ出した部分を輪郭として表示する。

---

### 06_VertexWave

- シェーダ：
  - [06_VertexWave.shader](Assets/Study/Shaders/06_VertexWave.shader)
- マテリアル：
  - [M_06_VertexWave.mat](Assets/Study/Materials/M_06_VertexWave.mat)

Vertex Shaderで頂点位置を時間変化させるシェーダ。

```hlsl
positionOS.y +=
    sin(positionOS.x * frequency + time * speed)
    * amplitude;
```

Object Spaceの頂点位置に `sin` を利用した変位を加え、時間によって波が移動する表現を実装する。

---

### 07_NormalMap

- シェーダ：
  - [07_NormalMap.shader](Assets/Study/Shaders/07_NormalMap.shader)
- マテリアル：
  - [M_07_NormalMap.mat](Assets/Study/Materials/M_07_NormalMap.mat)

Normal MappingとTangent Spaceを確認するためのシェーダ。

Normal Mapから取得した `[0, 1]` の値を、方向ベクトルとして利用するため `[-1, 1]` に変換する。

```hlsl
float3 normalTS = normalTex.rgb * 2.0 - 1.0;
```

Tangent、Bitangent、Normalから構成されるTBN basisを利用し、Tangent SpaceのNormalをLightingに利用できる座標空間へ変換する。

## GLSL / C++・OpenGLで確認したサンプル

以下は[Extras/GLSL](Extras/GLSL)のShaderと[Extras/CppShader](Extras/CppShader)の実行コードを組み合わせた、比較・読解用の独立したサンプルです。

### 00_Basic

- 対応するUnity側のテーマ：`01_Normal`のNormal可視化
- GLSL：
  - [00_Basic.vert](Extras/GLSL/00_Basic.vert)
  - [00_Basic.frag](Extras/GLSL/00_Basic.frag)
- C++：
  - [main.cpp](Extras/CppShader/00_Basic/main.cpp)

Vertex Attribute、Model／View／Projection Matrixの接続と、World Space NormalのRGB表示を確認する。

### 02_Lambert

- 対応するUnity側のテーマ：`02_Lambert`
- GLSL：
  - [02_Lambert.vert](Extras/GLSL/02_Lambert.vert)
  - [02_Lambert.frag](Extras/GLSL/02_Lambert.frag)
- C++：
  - [main.cpp](Extras/CppShader/02_Lambert/main.cpp)

Light DirectionとBase Colorをuniformで渡し、World SpaceのNormalとの内積からLambert Diffuseを計算する。

### 05_Toon

- 対応するUnity側のテーマ：`05_Toon`
- GLSL：
  - [05_Toon.vert](Extras/GLSL/05_Toon.vert)
  - [05_Toon.frag](Extras/GLSL/05_Toon.frag)
- C++：
  - [main.cpp](Extras/CppShader/05_Toon/main.cpp)

Vertex extrusion、Face Culling、同じShader Programを使った2回の描画によりToon ShadingとOutlineを確認する。

### 07_NormalMap

- 対応するUnity側のテーマ：`07_NormalMap`
- GLSL：
  - [07_NormalMap.vert](Extras/GLSL/07_NormalMap.vert)
  - [07_NormalMap.frag](Extras/GLSL/07_NormalMap.frag)
- C++：
  - [main.cpp](Extras/CppShader/07_NormalMap/main.cpp)

PNGの読み込み、Textureとの接続、TBN basisによるTangent Space Normalの変換を確認する。

### 08_PostEffect

- 対応するUnity側のテーマ：なし（Unity側のShader／Materialは未実装）
- Scene用GLSL：
  - [08_PostEffectScene.vert](Extras/GLSL/08_PostEffectScene.vert)
  - [08_PostEffectScene.frag](Extras/GLSL/08_PostEffectScene.frag)
- Post Effect用GLSL：
  - [08_PostEffect.vert](Extras/GLSL/08_PostEffect.vert)
  - [08_PostEffect.frag](Extras/GLSL/08_PostEffect.frag)
- C++：
  - [main.cpp](Extras/CppShader/08_PostEffect/main.cpp)

Sceneを独自FramebufferのColor Textureへ描画し、そのTextureをFull Screen Quadから読み直す2 Pass構成。

`effectMode`により、Original、Grayscale、Vignette、Chromatic Aberration、Neighbor Samplingを切り替える。Neighbor Samplingでは`textureSize()`から1 texel分のUV幅を求め、中央と上下左右をsampleする。

Buildと実行方法は[CppShader README](Extras/CppShader/README.md)を参照。
