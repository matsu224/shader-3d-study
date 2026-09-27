#version 450 core

// C++側のsceneVerticesにあるposition.xyzを受け取る。
layout(location = 0) in vec3 positionOS;

// C++側のsceneVerticesにあるcolor.rgbを受け取る。
layout(location = 1) in vec3 color;

// Vertex ShaderからFragment Shaderへ頂点色を渡す。
// 三角形内部では、3頂点の色が各fragmentの位置に応じて自動的に補間される。
layout(location = 0) out vec3 vertexColor;

// Object SpaceからClip Spaceへ変換するための3つのmatrix。
uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    // 頂点色は座標変換せず、そのままFragment Shaderへ渡す。
    vertexColor = color;

    // Object Spaceの頂点を、World → View → Clip Spaceの順に変換する。
    gl_Position = projection * view * model * vec4(positionOS, 1.0);
}
