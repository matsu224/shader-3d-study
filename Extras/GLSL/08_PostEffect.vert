#version 450 core

// C++側のscreenQuadVerticesにあるClip Space上のposition.xyを受け取る。
// Scene用Shaderと違い、すでに画面全体を覆う[-1, 1]の座標なのでmatrix変換は不要。
layout(location = 0) in vec2 positionCS;

// Full Screen Quadの各頂点に割り当てたUVを受け取る。
layout(location = 1) in vec2 uv;

// Vertex ShaderからFragment ShaderへUVを渡す。
// Quad内部の各fragmentでは、頂点間の値が自動的に補間される。
layout(location = 0) out vec2 screenUV;

void main()
{
    // 入力のXYはすでにClip Spaceなので、そのままgl_Positionへ設定する。
    // Zは画面中央の0、Wは通常の位置を表す1にする。
    gl_Position = vec4(positionCS, 0.0, 1.0);

    // C++側で設定した[0, 1]のUVを、そのままFragment Shaderへ渡す。
    screenUV = uv;
}
