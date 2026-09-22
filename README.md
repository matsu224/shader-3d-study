# shader-3d-study

Unity URPを用いた3Dシェーダの学習用リポジトリです。

ゲームフリークのシェーダ開発インターンに向けた事前学習として、3D mesh shaderの基本的な処理を、小さなシェーダに分けて実装しています。

## 実装結果

**動画URL：** TBD

以下のシェーダをそれぞれ別のオブジェクト・マテリアルに適用し、1つのUnity Scene上で確認できるようにしています。

- Unlit
- Normal visualization
- Lambert diffuse
- Blinn-Phong specular
- Fresnel
- Toon shading
- Vertex deformation
- Normal mapping

## 実装したシェーダ

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

Lambert Diffuseの連続的な明るさを離散化してToon Shadingを実装。

```hlsl
float ndotl = saturate(dot(N, L));
float toon = step(0.5, ndotl);
```

2階調・複数階調の陰影に加え、Fresnelを利用したRimやToon Specularも確認する。

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
