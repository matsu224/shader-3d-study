#version 450 core

// Vertex Shaderから渡され、三角形内部で補間された色を受け取る。
layout(location = 0) in vec3 vertexColor;

// このfragmentから、現在の描画先へ書き込む最終色。
layout(location = 0) out vec4 fragColor;

void main()
{
    // Scene PassではTextureや照明を使わず、補間された頂点色をそのまま出力する。
    fragColor = vec4(vertexColor, 1.0);
}
