# C++ / OpenGL接続学習

GLSLをC++から利用する流れを、shaderごとの独立したサンプルで確認します。コードはAIで生成し、処理の流れとC++／GLSL間の対応を読み解くために使用します。

## サンプル

### 00_Basic

`Extras/GLSL/00_Basic.vert`と`00_Basic.frag`を使用します。

- position / normal attribute
- model / view / projection uniform
- normalWSのRGB表示

### 02_Lambert

`00_Basic`と同じattribute／matrix接続に、次の2つを追加します。

- `lightDirWS`：World Spaceでsurfaceからlightへ向かう方向
- `baseColor`：Lambert Diffuseへ掛ける基本色

C++側では両方を`glm::vec3`で作り、`glGetUniformLocation`と`glUniform3fv`でFragment Shaderへ渡します。形状とmodel回転は`00_Basic`と同じにし、lightを正面方向へ固定します。

Objectが回転するとnormalWSが変化し、固定したlightとの`max(dot(N, L), 0.0)`によって三角形の明るさが変化します。これにより、`00_Basic`のnormal可視化からLambert shadingへ進んだ差分を確認できます。

### 05_Toon

`Extras/GLSL/05_Toon.vert`と`05_Toon.frag`を使い、UV Sphereを同じShader Programで2回描画します。

- Pass 1: `outlinePass = 1`としてnormal方向へ頂点を押し出し、`glCullFace(GL_FRONT)`でOutlineを描画
- Pass 2: `outlinePass = 0`として元の頂点位置へ戻し、`glCullFace(GL_BACK)`で通常のToonを描画

Unity URP版のPassはRenderer Featureと`LightMode`で選択しますが、OpenGL版ではC++側の2回の`glDrawElements`と描画stateの切り替えがMulti Passに相当します。

### 07_NormalMap

`Extras/GLSL/07_NormalMap.vert`と`07_NormalMap.frag`を使い、UV／normal／tangentを持つ板へNormal Mapを適用します。

- libpngで`Assets/Study/Textures/metal_grate_rusty_nor_gl_4k.png`をRGBA8として読み込む
- PNGの画像dataをOpenGL Textureへ転送し、`sampler2D normalMap`へTexture Unit 0で接続する
- Vertex ShaderでWorld SpaceのTangent／Bitangent／Normalを作る
- Fragment ShaderでNormal MapのTangent Space法線をTBNによりWorld Spaceへ変換する
- 変換した法線とWorld Spaceの光方向でLambert Diffuseを計算する

TextureはローカルのUnity Assetを参照し、Git管理には追加しません。

## 使用環境

| 環境・依存関係 | 役割 |
| --- | --- |
| C++17 / Apple Clang | C++コードのcompile |
| GLFW | windowとOpenGL Contextの作成、event処理 |
| macOS OpenGL framework | OpenGL APIの提供 |
| GLM | vectorとmatrixの作成 |
| CMake | build設定と依存関係の接続 |
| libpng | 07でNormal Map PNGをRGBA8へ展開 |

```sh
brew install cmake glfw glm libpng
```

既存GLSLは`#version 450 core`のまま保持します。macOSのOpenGLは4.1までなので、各C++サンプルは読み込んだメモリ上の文字列について、compile前にversion行だけを`410 core`へ合わせます。

## Buildと実行

repositoryのルートで実行します。

### 00_Basic

```sh
cmake -S Extras/CppShader/00_Basic -B build/cpp-shader/00-basic
cmake --build build/cpp-shader/00-basic
./build/cpp-shader/00-basic/cpp_shader_basic
```

### 02_Lambert

```sh
cmake -S Extras/CppShader/02_Lambert -B build/cpp-shader/02-lambert
cmake --build build/cpp-shader/02-lambert
./build/cpp-shader/02-lambert/cpp_shader_lambert
```

### 05_Toon

```sh
cmake -S Extras/CppShader/05_Toon -B build/cpp-shader/05-toon
cmake --build build/cpp-shader/05-toon
./build/cpp-shader/05-toon/cpp_shader_toon
```

### 07_NormalMap

```sh
cmake -S Extras/CppShader/07_NormalMap -B build/cpp-shader/07-normal-map
cmake --build build/cpp-shader/07-normal-map
./build/cpp-shader/07-normal-map/cpp_shader_normal_map
```

## VS CodeでDebug

ローカルの`.vscode/settings.json`では、CMake Toolsの対象を`02_Lambert`、build先を通常buildと分けた`build/cpp-shader/02-lambert-debug`、build typeを`Debug`に設定しています。上記の通常buildと互いに上書きしません。

MicrosoftのCMake ToolsとC/C++拡張を使用し、次の流れで実行します。

1. CMakeパネルで`cpp_shader_lambert`を対象にする
2. C++の行番号左側をクリックしてBreakpointを置く
3. CMakeパネルの`Debug`にある「三角＋虫」ボタンで起動する
4. 停止後、変数表示やWatchで値を確認し、Step Over / Step Into / Continueを使う

`00_Basic`をDebugする場合は、`.vscode/settings.json`の`cmake.sourceDirectory`を`Extras/CppShader/00_Basic`、`cmake.buildDirectory`を`build/cpp-shader/00-basic-debug`へ切り替え、CMake ToolsでConfigureします。
