#version 450 core

// Vertex Shaderから渡され、Full Screen Quad全体で補間されたUVを受け取る。
layout(location = 0) in vec2 screenUV;

// このfragmentから、現在の描画先へ書き込む最終色。
layout(location = 0) out vec4 fragColor;

// Sceneを画面外へ描画した結果が保存されているColor Texture。
uniform sampler2D screenTexture;

// C++側から、現在表示するPost Effectの種類を整数で受け取る。
// 0はOriginal、1はGrayscale、2はVignette、3はChromatic Aberration、
// 4はNeighbor Samplingとして扱う。
uniform int effectMode;

// Scene Textureの現在のUVを1回sampleし、色を加工せずそのまま返す。
vec3 applyOriginal(vec2 uv)
{
    return texture(screenTexture, uv).rgb;
}

// Scene Textureの現在のUVを1回sampleし、RGBを人間の明るさ知覚に近い輝度へ変換する。
vec3 applyGrayscale(vec2 uv)
{
    vec3 sceneColor = texture(screenTexture, uv).rgb;

    // 人間の目はGreenを明るく、Blueを暗く感じるため、RGBを同じ割合では混ぜない。
    float luminance = dot(sceneColor, vec3(0.2126, 0.7152, 0.0722));

    // 1つの輝度値をRGBすべてへ設定し、色相を持たないGrayscaleにする。
    return vec3(luminance);
}

// Scene Textureを1回sampleし、画面中央からの距離に応じて外側を暗くする。
vec3 applyVignette(vec2 uv)
{
    vec3 sceneColor = texture(screenTexture, uv).rgb;

    // UVの原点は左下の(0, 0)なので、0.5を引いて画面中央を新しい原点にする。
    // centeredUVは画面中央で(0, 0)、左下で(-0.5, -0.5)になる。
    vec2 centeredUV = uv - vec2(0.5);

    // 画面中央から現在のfragmentまでの距離を求める。
    // 中央では0、四隅では約0.707になる。
    float distanceFromCenter = length(centeredUV);

    // 距離0.15までは0、0.60以降は1、その間は滑らかに変化する値を作る。
    // 中央寄りから暗くし始め、Scene中央の三角形にも効果が見える強さにする。
    float edgeAmount = smoothstep(0.15, 0.60, distanceFromCenter);

    // edgeAmountを反転し、中央で1、外側で0に近づく明るさmaskにする。
    float vignetteMask = 1.0 - edgeAmount;

    // SceneのRGBへmaskを掛け、中央の色を保ちながら外側だけを暗くする。
    return sceneColor * vignetteMask;
}

// RGB channelを水平方向に少し異なるUVからsampleし、色ずれを作る。
vec3 applyChromaticAberration(vec2 uv)
{
    // UV上で横幅の1%だけずらす。Textureの解像度に関係なく、画面幅に対する割合になる。
    const float channelOffset = 0.01;
    vec2 horizontalOffset = vec2(channelOffset, 0.0);

    // Redは右側、Greenは現在位置、Blueは左側のUVから、それぞれのchannelだけを読む。
    // 3回のtexture()は異なる位置をsampleしている。
    float red = texture(screenTexture, uv + horizontalOffset).r;
    float green = texture(screenTexture, uv).g;
    float blue = texture(screenTexture, uv - horizontalOffset).b;

    // 異なる位置から取得した3つのchannelを、現在のfragmentの最終RGBとして再構成する。
    return vec3(red, green, blue);
}

// Textureの解像度からtexel幅を求め、中央と上下左右の色をsampleして平均する。
vec3 applyNeighborSampling(vec2 uv)
{
    // textureSize(..., 0)はmipmap level 0のTexture解像度をpixel数で返す。
    // その逆数を取ると、UV空間における横1 texel・縦1 texel分の移動量になる。
    vec2 texelSize = 1.0 / vec2(textureSize(screenTexture, 0));

    // 1 texelだけでは高解像度画面で差が見えにくいため、確認用に4 texel離して読む。
    // 基準となる1 texelのUV幅自体は、上のtextureSize()から正確に求めている。
    const float sampleDistanceInTexels = 4.0;
    vec2 horizontalOffset = vec2(texelSize.x * sampleDistanceInTexels, 0.0);
    vec2 verticalOffset = vec2(0.0, texelSize.y * sampleDistanceInTexels);

    // 現在位置に加えて、左・右・下・上の4方向をそれぞれsampleする。
    vec3 centerColor = texture(screenTexture, uv).rgb;
    vec3 leftColor = texture(screenTexture, uv - horizontalOffset).rgb;
    vec3 rightColor = texture(screenTexture, uv + horizontalOffset).rgb;
    vec3 bottomColor = texture(screenTexture, uv - verticalOffset).rgb;
    vec3 topColor = texture(screenTexture, uv + verticalOffset).rgb;

    // 5つの色を同じ重みで平均し、周辺色を混ぜた結果を現在のfragmentへ返す。
    // Neighbor Samplingの確認が目的なので、複雑なkernelや重み付けはまだ使用しない。
    return (centerColor + leftColor + rightColor + bottomColor + topColor) / 5.0;
}

void main()
{
    vec3 finalColor;

    // effectModeは全fragmentで同じuniformなので、画面全体が同じEffectを使用する。
    if (effectMode == 1)
    {
        finalColor = applyGrayscale(screenUV);
    }
    else if (effectMode == 2)
    {
        finalColor = applyVignette(screenUV);
    }
    else if (effectMode == 3)
    {
        finalColor = applyChromaticAberration(screenUV);
    }
    else if (effectMode == 4)
    {
        finalColor = applyNeighborSampling(screenUV);
    }
    else
    {
        // 0および未定義の番号はOriginalへ戻し、常に有効な色を出力する。
        finalColor = applyOriginal(screenUV);
    }

    fragColor = vec4(finalColor, 1.0);
}
