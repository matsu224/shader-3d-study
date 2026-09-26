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

## 使用環境

| 環境・依存関係 | 役割 |
| --- | --- |
| C++17 / Apple Clang | C++コードのcompile |
| GLFW | windowとOpenGL Contextの作成、event処理 |
| macOS OpenGL framework | OpenGL APIの提供 |
| GLM | vectorとmatrixの作成 |
| CMake | build設定と依存関係の接続 |

```sh
brew install cmake glfw glm
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
