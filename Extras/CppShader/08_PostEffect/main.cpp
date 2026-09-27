// macOSでOpenGLが非推奨であることによるwarningだけを抑制する。
#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace
{
// WindowのFramebuffer sizeが変わったとき、OpenGLの描画範囲も同じ大きさに更新する。
void FramebufferSizeCallback(GLFWwindow*, int width, int height)
{
    glViewport(0, 0, width, height);
}

// 指定されたtext file全体を、1つのstd::stringとして読み込む。
std::string ReadTextFile(const std::string& path)
{
    std::ifstream file(path);
    if (!file)
    {
        throw std::runtime_error("Failed to open file: " + path);
    }

    std::ostringstream contents;
    contents << file.rdbuf();
    return contents.str();
}

// macOSのOpenGL 4.1でcompileできるよう、読み込んだ文字列のversionだけを合わせる。
// GLSL file自体は他環境でも使えるよう、#version 450 coreのまま保持する。
std::string MakeMacOSCompatibleSource(std::string source)
{
    const std::string version450 = "#version 450 core";
    if (source.rfind(version450, 0) == 0)
    {
        source.replace(0, version450.size(), "#version 410 core");
    }

    return source;
}

// C++の文字列として読み込んだGLSLを、1つのshader objectとしてcompileする。
GLuint CompileShader(GLenum shaderType, const std::string& source, const std::string& label)
{
    // shaderの種類を指定し、空のshader objectを作成する。
    const GLuint shader = glCreateShader(shaderType);

    // C++文字列のGLSL sourceをshader objectへ渡す。
    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);

    // GLSL sourceをGPUが実行できる形式へcompileする。
    glCompileShader(shader);

    // compile結果を取得し、成功またはcompile logを表示する。
    GLint compileSucceeded = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileSucceeded);

    if (compileSucceeded == GL_TRUE)
    {
        std::cout << label << " compile succeeded.\n" << std::flush;
        return shader;
    }

    GLint logLength = 0;
    glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &logLength);

    std::cerr << label << " compile failed.\n";
    if (logLength > 0)
    {
        std::string compileLog(static_cast<std::size_t>(logLength), '\0');
        GLsizei writtenLength = 0;
        glGetShaderInfoLog(shader, logLength, &writtenLength, compileLog.data());
        compileLog.resize(static_cast<std::size_t>(writtenLength));
        std::cerr << compileLog << '\n';
    }

    return shader;
}

// compile済みのVertex ShaderとFragment Shaderを、1つのShader Programへlinkする。
GLuint LinkShaderProgram(
    GLuint vertexShader,
    GLuint fragmentShader,
    const std::string& label)
{
    // compileは各shader単体を検証し、linkはshader同士の入出力も含めて接続する。
    const GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // link結果を取得し、成功またはlink logを表示する。
    GLint linkSucceeded = GL_FALSE;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkSucceeded);

    if (linkSucceeded == GL_TRUE)
    {
        std::cout << label << " link succeeded.\n" << std::flush;
        return shaderProgram;
    }

    GLint logLength = 0;
    glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);

    std::cerr << label << " link failed.\n";
    if (logLength > 0)
    {
        std::string linkLog(static_cast<std::size_t>(logLength), '\0');
        GLsizei writtenLength = 0;
        glGetProgramInfoLog(shaderProgram, logLength, &writtenLength, linkLog.data());
        linkLog.resize(static_cast<std::size_t>(writtenLength));
        std::cerr << linkLog << '\n';
    }

    return shaderProgram;
}
} // namespace

int main()
{
    // GLFWを初期化し、WindowとOpenGL Contextを作成できる状態にする。
    if (glfwInit() != GLFW_TRUE)
    {
        std::cerr << "Failed to initialize GLFW.\n";
        return EXIT_FAILURE;
    }

    // macOSで利用可能なOpenGL 4.1 Core Profileを指定する。
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);

    // Windowを作り、それに対応するOpenGL Contextを現在のthreadで使えるようにする。
    GLFWwindow* window = glfwCreateWindow(800, 600, "08 Post Effect", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create a GLFW window.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    glfwMakeContextCurrent(window);
    glfwSetFramebufferSizeCallback(window, FramebufferSizeCallback);

    GLuint sceneVertexShader = 0;
    GLuint sceneFragmentShader = 0;
    GLuint sceneShaderProgram = 0;
    GLuint postEffectVertexShader = 0;
    GLuint postEffectFragmentShader = 0;
    GLuint postEffectShaderProgram = 0;

    try
    {
        // Pass 1で使う、Scene描画専用のVertex ShaderとFragment Shaderを読み込む。
        const std::string vertexSource = MakeMacOSCompatibleSource(
            ReadTextFile(std::string(GLSL_DIRECTORY) + "/08_PostEffectScene.vert"));
        const std::string fragmentSource = MakeMacOSCompatibleSource(
            ReadTextFile(std::string(GLSL_DIRECTORY) + "/08_PostEffectScene.frag"));

        // 2つのGLSLを個別にcompileした後、1つのScene Shader Programへlinkする。
        sceneVertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource, "Scene Vertex Shader");
        sceneFragmentShader = CompileShader(
            GL_FRAGMENT_SHADER, fragmentSource, "Scene Fragment Shader");
        sceneShaderProgram = LinkShaderProgram(
            sceneVertexShader,
            sceneFragmentShader,
            "Scene Shader Program");

        // link後はProgram側に実行内容が保持されるため、個別のshader objectは削除できる。
        glDeleteShader(sceneVertexShader);
        glDeleteShader(sceneFragmentShader);
        sceneVertexShader = 0;
        sceneFragmentShader = 0;

        // Full Screen Quadを描画するPost Effect用GLSLを読み込む。
        // Fragment ShaderはSceneのColor Textureをsampleし、画面へ表示する。
        const std::string postEffectVertexSource = MakeMacOSCompatibleSource(
            ReadTextFile(std::string(GLSL_DIRECTORY) + "/08_PostEffect.vert"));
        const std::string postEffectFragmentSource = MakeMacOSCompatibleSource(
            ReadTextFile(std::string(GLSL_DIRECTORY) + "/08_PostEffect.frag"));

        // Scene用とは別に、Full Screen Quad専用のShader Programを作る。
        postEffectVertexShader = CompileShader(
            GL_VERTEX_SHADER, postEffectVertexSource, "Post Effect Vertex Shader");
        postEffectFragmentShader = CompileShader(
            GL_FRAGMENT_SHADER, postEffectFragmentSource, "Post Effect Fragment Shader");
        postEffectShaderProgram = LinkShaderProgram(
            postEffectVertexShader,
            postEffectFragmentShader,
            "Post Effect Shader Program");

        // link後は個別のshader objectが不要になるため、Scene用と同様に削除する。
        glDeleteShader(postEffectVertexShader);
        glDeleteShader(postEffectFragmentShader);
        postEffectVertexShader = 0;
        postEffectFragmentShader = 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // 1頂点をposition.xyz + color.rgbの順で並べたinterleaved data。
    // 3頂点のY座標の平均を0にし、三角形の重心が回転軸の原点と一致するようにする。
    // 各頂点を赤・緑・青にし、三角形内部では3色が自動的に補間されるようにする。
    const float sceneVertices[] = {
        // position             // color
        -0.75f, -0.50f, 0.0f,   1.0f, 0.15f, 0.10f,
         0.75f, -0.50f, 0.0f,   0.10f, 1.0f, 0.20f,
         0.0f,   1.00f, 0.0f,   0.15f, 0.30f, 1.0f,
    };

    // VAOは「どのVBOを、どの頂点attributeとして読むか」という設定を保持する。
    GLuint sceneVao = 0;

    // VBOは頂点のpositionやcolorなど、頂点データ本体をGPU側に保存する。
    GLuint sceneVbo = 0;

    // Scene用VAOをbindし、これ以降の頂点attribute設定の記録先にする。
    glGenVertexArrays(1, &sceneVao);
    glBindVertexArray(sceneVao);

    // Scene用VBOへ、CPU側にある3頂点のデータを一度だけ転送する。
    glGenBuffers(1, &sceneVbo);
    glBindBuffer(GL_ARRAY_BUFFER, sceneVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(sceneVertices), sceneVertices, GL_STATIC_DRAW);

    // 1頂点にはpositionの3要素とcolorの3要素があるため、合計6個のfloatが並ぶ。
    constexpr GLsizei sceneVertexStride = 6 * sizeof(float);

    // VBO先頭のposition.xyzを、Vertex Shaderのlayout(location = 0)へ接続する。
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sceneVertexStride, nullptr);
    glEnableVertexAttribArray(0);

    // positionの直後にあるcolor.rgbを、Vertex Shaderのlayout(location = 1)へ接続する。
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sceneVertexStride,
        reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Full Screen Quadは2枚の三角形、合計6頂点で画面全体を覆う。
    // positionはClip Spaceの[-1, 1]、UVはTexture Samplingで使う[0, 1]に対応する。
    const float screenQuadVertices[] = {
        // position    // uv
        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f, -1.0f,  1.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,

        -1.0f, -1.0f,  0.0f, 0.0f,
         1.0f,  1.0f,  1.0f, 1.0f,
        -1.0f,  1.0f,  0.0f, 1.0f,
    };

    GLuint screenQuadVao = 0;
    GLuint screenQuadVbo = 0;

    // Sceneとは頂点構造が異なるため、Full Screen Quad専用のVAOとVBOを作る。
    glGenVertexArrays(1, &screenQuadVao);
    glBindVertexArray(screenQuadVao);

    glGenBuffers(1, &screenQuadVbo);
    glBindBuffer(GL_ARRAY_BUFFER, screenQuadVbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(screenQuadVertices),
        screenQuadVertices,
        GL_STATIC_DRAW);

    // 1頂点はposition.xyとuv.xyで構成されるため、合計4個のfloatが並ぶ。
    constexpr GLsizei screenQuadVertexStride = 4 * sizeof(float);

    // VBO先頭のposition.xyを、Post Effect Vertex Shaderのlocation 0へ接続する。
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, screenQuadVertexStride, nullptr);
    glEnableVertexAttribArray(0);

    // positionの直後にあるuv.xyを、Post Effect Vertex Shaderのlocation 1へ接続する。
    glVertexAttribPointer(
        1,
        2,
        GL_FLOAT,
        GL_FALSE,
        screenQuadVertexStride,
        reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // Pass 1でSceneを画面外へ描くための独自Framebufferを作る。
    // Color TextureとDepth Renderbufferを接続し、描画可能な構成にする。
    GLuint sceneFramebuffer = 0;
    glGenFramebuffers(1, &sceneFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);

    // Windowの論理sizeではなく、実際にpixelを保持するFramebuffer sizeを取得する。
    // Retina displayでは、例えば800x600のWindowでも1600x1200になる場合がある。
    int sceneFramebufferWidth = 0;
    int sceneFramebufferHeight = 0;
    glfwGetFramebufferSize(window, &sceneFramebufferWidth, &sceneFramebufferHeight);

    // Sceneの最終色を保存する2D Textureを作る。
    // dataへnullptrを渡すことで、ここでは画像を読み込まず、保存領域だけを確保する。
    GLuint sceneColorTexture = 0;
    glGenTextures(1, &sceneColorTexture);
    glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        sceneFramebufferWidth,
        sceneFramebufferHeight,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        nullptr);

    // 後でFull Screen Quadからsampleするときの拡大・縮小filterを設定する。
    // mipmapは作らないため、縮小時もmipmapを要求しないGL_LINEARを使う。
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // UVがわずかに[0, 1]を越えた場合、反対側の端が繰り返されないよう端の色で固定する。
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // 作成したTextureを、独自FramebufferのColor Attachment 0へ接続する。
    glFramebufferTexture2D(
        GL_FRAMEBUFFER,
        GL_COLOR_ATTACHMENT0,
        GL_TEXTURE_2D,
        sceneColorTexture,
        0);

    // Sceneの奥行き判定に使うDepth Renderbufferを作る。
    // 色は後でTextureとしてsampleするが、Depthは今回はsampleしないためRenderbufferで保持する。
    GLuint sceneDepthRenderbuffer = 0;
    glGenRenderbuffers(1, &sceneDepthRenderbuffer);
    glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthRenderbuffer);
    glRenderbufferStorage(
        GL_RENDERBUFFER,
        GL_DEPTH_COMPONENT24,
        sceneFramebufferWidth,
        sceneFramebufferHeight);

    // Depth Renderbufferを、独自FramebufferのDepth Attachmentへ接続する。
    glFramebufferRenderbuffer(
        GL_FRAMEBUFFER,
        GL_DEPTH_ATTACHMENT,
        GL_RENDERBUFFER,
        sceneDepthRenderbuffer);

    // このFramebufferへ色を出力するとき、Color Attachment 0を描画先として使う。
    const GLenum sceneDrawBuffers[] = {GL_COLOR_ATTACHMENT0};
    glDrawBuffers(1, sceneDrawBuffers);

    // 必要なAttachmentが正しく接続され、描画可能な状態かをOpenGLへ問い合わせる。
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr << "Scene framebuffer is incomplete.\n";

        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glDeleteRenderbuffers(1, &sceneDepthRenderbuffer);
        glDeleteTextures(1, &sceneColorTexture);
        glDeleteFramebuffers(1, &sceneFramebuffer);
        glDeleteBuffers(1, &screenQuadVbo);
        glDeleteVertexArrays(1, &screenQuadVao);
        glDeleteBuffers(1, &sceneVbo);
        glDeleteVertexArrays(1, &sceneVao);
        glDeleteProgram(postEffectShaderProgram);
        glDeleteProgram(sceneShaderProgram);
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    std::cout
        << "Scene framebuffer is complete: "
        << sceneFramebufferWidth << 'x' << sceneFramebufferHeight << "\n"
        << std::flush;

    // Attachmentの設定が終わったため、初期化中の描画先を標準Framebufferへ戻しておく。
    // 描画ループではPassごとにsceneFramebufferと標準Framebufferをbindし直す。
    glBindTexture(GL_TEXTURE_2D, 0);
    glBindRenderbuffer(GL_RENDERBUFFER, 0);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    // GLSL内のuniform名から、各Programへ値を渡すためのlocationを取得する。
    const GLint modelLocation = glGetUniformLocation(sceneShaderProgram, "model");
    const GLint viewLocation = glGetUniformLocation(sceneShaderProgram, "view");
    const GLint projectionLocation = glGetUniformLocation(sceneShaderProgram, "projection");
    const GLint screenTextureLocation = glGetUniformLocation(
        postEffectShaderProgram, "screenTexture");
    const GLint effectModeLocation = glGetUniformLocation(
        postEffectShaderProgram, "effectMode");

    // sampler2DのscreenTextureがTexture Unit 0を参照するよう、一度だけ設定する。
    // Texture object本体は描画時にGL_TEXTURE0へbindする。
    glUseProgram(postEffectShaderProgram);
    glUniform1i(screenTextureLocation, 0);

    // 画面全体へ適用するEffect番号。起動時は加工しないOriginalを表示する。
    // 0: Original、1: Grayscale、2: Vignette、3: Chromatic Aberration。
    // 4: Neighbor Sampling。
    int effectMode = 0;

    // Windowが閉じられるまで、入力・Scene描画・画面表示を毎frame繰り返す。
    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        {
            glfwSetWindowShouldClose(window, GLFW_TRUE);
        }

        if (glfwGetKey(window, GLFW_KEY_0) == GLFW_PRESS)
        {
            effectMode = 0;
        }

        if (glfwGetKey(window, GLFW_KEY_1) == GLFW_PRESS)
        {
            effectMode = 1;
        }

        if (glfwGetKey(window, GLFW_KEY_2) == GLFW_PRESS)
        {
            effectMode = 2;
        }

        if (glfwGetKey(window, GLFW_KEY_3) == GLFW_PRESS)
        {
            effectMode = 3;
        }

        if (glfwGetKey(window, GLFW_KEY_4) == GLFW_PRESS)
        {
            effectMode = 4;
        }

        // Retina displayを含む現在のFramebuffer sizeを取得し、描画範囲に反映する。
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);

        // 最小化中は幅や高さが0になる可能性があるため、保存領域の再確保と描画を止める。
        if (framebufferWidth == 0 || framebufferHeight == 0)
        {
            glfwPollEvents();
            continue;
        }

        // WindowのFramebuffer sizeが変わった場合、Color TextureとDepth Renderbufferも作り直す。
        // Attachment object自体は再利用し、内部のpixel保存領域だけを新しいsizeへ変更する。
        if (framebufferWidth != sceneFramebufferWidth
            || framebufferHeight != sceneFramebufferHeight)
        {
            sceneFramebufferWidth = framebufferWidth;
            sceneFramebufferHeight = framebufferHeight;

            glBindTexture(GL_TEXTURE_2D, sceneColorTexture);
            glTexImage2D(
                GL_TEXTURE_2D,
                0,
                GL_RGBA8,
                sceneFramebufferWidth,
                sceneFramebufferHeight,
                0,
                GL_RGBA,
                GL_UNSIGNED_BYTE,
                nullptr);

            glBindRenderbuffer(GL_RENDERBUFFER, sceneDepthRenderbuffer);
            glRenderbufferStorage(
                GL_RENDERBUFFER,
                GL_DEPTH_COMPONENT24,
                sceneFramebufferWidth,
                sceneFramebufferHeight);

            glBindTexture(GL_TEXTURE_2D, 0);
            glBindRenderbuffer(GL_RENDERBUFFER, 0);
        }

        // ObjectをZ軸周りに回転させ、Post Effect前のSceneに動きと色の変化を用意する。
        const glm::mat4 model = glm::rotate(
            glm::mat4(1.0f),
            static_cast<float>(glfwGetTime()) * 0.5f,
            glm::vec3(0.0f, 0.0f, 1.0f));

        // CameraはZ正方向から原点を見る。三角形は原点付近に置かれている。
        const glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 2.5f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        // Windowの縦横比を使い、SceneをPerspective ProjectionでClip Spaceへ変換する。
        const glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            0.1f,
            100.0f);

        // Pass 1では独自Framebufferを描画先にし、Sceneの色とDepthを画面外へ保存する。
        glBindFramebuffer(GL_FRAMEBUFFER, sceneFramebuffer);
        glViewport(0, 0, sceneFramebufferWidth, sceneFramebufferHeight);

        // Scene PassではDepth Testを有効にし、ColorとDepthの両方を毎frame初期化する。
        glEnable(GL_DEPTH_TEST);
        // Vignetteによる中央と外側の明暗差を確認しやすい、少し明るい背景色にする。
        glClearColor(0.30f, 0.34f, 0.42f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Scene用Programを選び、C++で作った3つのmatrixをVertex Shaderへ渡す。
        glUseProgram(sceneShaderProgram);
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, glm::value_ptr(projection));

        // Scene用VAOを選び、3頂点を1枚の三角形として現在の描画先へ描く。
        glBindVertexArray(sceneVao);
        glDrawArrays(GL_TRIANGLES, 0, 3);

        // Pass 2ではWindowの標準Framebufferへ戻し、画面全体を描画範囲にする。
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, framebufferWidth, framebufferHeight);

        // Full Screen Quadは1枚の画像として上書きするため、Depth Testは不要。
        glDisable(GL_DEPTH_TEST);
        glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        // Pass 1の描画結果を持つColor TextureをTexture Unit 0へbindする。
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, sceneColorTexture);

        // 選択中のEffect番号をuniform intとしてPost Effect Fragment Shaderへ渡す。
        glUseProgram(postEffectShaderProgram);
        glUniform1i(effectModeLocation, effectMode);

        // Full Screen Quadを描き、各fragmentで選択中のEffect関数を実行する。
        glBindVertexArray(screenQuadVao);
        glDrawArrays(GL_TRIANGLES, 0, 6);

        // 次のframeで同じTextureを描画先として使う前に、Texture Unitから外しておく。
        glBindTexture(GL_TEXTURE_2D, 0);

        // 描画済みのback bufferを表示し、WindowやKeyboardのeventを処理する。
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 作成したOpenGL objectを削除してから、WindowとGLFWを終了する。
    glDeleteRenderbuffers(1, &sceneDepthRenderbuffer);
    glDeleteTextures(1, &sceneColorTexture);
    glDeleteFramebuffers(1, &sceneFramebuffer);
    glDeleteBuffers(1, &screenQuadVbo);
    glDeleteVertexArrays(1, &screenQuadVao);
    glDeleteBuffers(1, &sceneVbo);
    glDeleteVertexArrays(1, &sceneVao);
    glDeleteProgram(postEffectShaderProgram);
    glDeleteProgram(sceneShaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
