// macOSでOpenGLが非推奨であることによるwarningだけを抑制する。
#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <cmath>
#include <cstddef>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace
{
// CPU側で1頂点分のdataを表す。
// positionとnormalを同じstructへまとめ、そのままVBOへinterleaved dataとして転送する。
struct Vertex
{
    glm::vec3 position;
    glm::vec3 normal;
};

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
// GLSL file自体と、shaderの入力・uniform・計算内容は変更しない。
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
// 成功時はshader objectのID、失敗時は0を返す。
GLuint CompileShader(GLenum shaderType, const std::string& source, const std::string& label)
{
    // shaderTypeでVertex／Fragmentの種類を指定し、空のshader objectを作る。
    const GLuint shader = glCreateShader(shaderType);

    // std::stringが保持するGLSL sourceをOpenGLへ渡し、compileを実行する。
    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);
    glCompileShader(shader);

    // compile結果を調べ、成功したshader objectを呼び出し側へ返す。
    GLint compileSucceeded = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compileSucceeded);
    if (compileSucceeded == GL_TRUE)
    {
        std::cout << label << " compile succeeded.\n" << std::flush;
        return shader;
    }

    // 失敗時はdriverが生成したcompile logを取得して表示する。
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

    // 失敗したshader objectはProgramへlinkできないため、ここで破棄する。
    glDeleteShader(shader);
    return 0;
}

// compile済みのVertex ShaderとFragment Shaderを、描画に使う1つのProgramへlinkする。
// compileは各shader単体を検証し、linkはshader同士の入出力やuniformも含めて接続する。
GLuint LinkShaderProgram(GLuint vertexShader, GLuint fragmentShader)
{
    // 空のProgramへ2つのshader objectを取り付け、linkを実行する。
    const GLuint shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    // link結果を調べ、成功したProgramのIDを呼び出し側へ返す。
    GLint linkSucceeded = GL_FALSE;
    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &linkSucceeded);
    if (linkSucceeded == GL_TRUE)
    {
        std::cout << "Shader Program link succeeded.\n" << std::flush;
        return shaderProgram;
    }

    // 失敗時はProgramのlink logを取得して表示する。
    GLint logLength = 0;
    glGetProgramiv(shaderProgram, GL_INFO_LOG_LENGTH, &logLength);
    std::cerr << "Shader Program link failed.\n";
    if (logLength > 0)
    {
        std::string linkLog(static_cast<std::size_t>(logLength), '\0');
        GLsizei writtenLength = 0;
        glGetProgramInfoLog(shaderProgram, logLength, &writtenLength, linkLog.data());
        linkLog.resize(static_cast<std::size_t>(writtenLength));
        std::cerr << linkLog << '\n';
    }

    // linkに失敗したProgramは描画に使えないため、ここで破棄する。
    glDeleteProgram(shaderProgram);
    return 0;
}

// 半径1のUV Sphereについて、頂点dataとtriangleのindex dataを作る。
// stackCountは上下方向の分割数、sliceCountは球の周囲方向の分割数。
// positionとnormalが同じになる球を使うことで、normal方向への頂点押し出しを観察しやすくする。
void CreateSphere(
    unsigned int stackCount,
    unsigned int sliceCount,
    std::vector<Vertex>& vertices,
    std::vector<unsigned int>& indices)
{
    constexpr float pi = 3.14159265358979323846f;

    // thetaを上端0～下端PI、phiを一周0～2PIの範囲で動かし、球面上の頂点を作る。
    // 各rowにsliceCount + 1個作るのは、0度と360度の位置を複製して継ぎ目を閉じるため。
    for (unsigned int stack = 0; stack <= stackCount; ++stack)
    {
        const float theta = pi * static_cast<float>(stack) / static_cast<float>(stackCount);
        const float sinTheta = std::sin(theta);
        const float cosTheta = std::cos(theta);

        for (unsigned int slice = 0; slice <= sliceCount; ++slice)
        {
            const float phi = 2.0f * pi * static_cast<float>(slice) / static_cast<float>(sliceCount);
            // 半径1なので、原点から球面へ向かう単位vectorをpositionとnormalの両方に使える。
            const glm::vec3 normal(
                sinTheta * std::cos(phi),
                cosTheta,
                sinTheta * std::sin(phi));
            vertices.push_back({normal, normal});
        }
    }

    // 隣接する2つのrowから四角形を取り出し、2枚のtriangleへ分割する。
    // indexは外側から見て反時計回りになるよう並べ、GL_CCWのfront faceとして扱う。
    const unsigned int rowSize = sliceCount + 1;
    for (unsigned int stack = 0; stack < stackCount; ++stack)
    {
        for (unsigned int slice = 0; slice < sliceCount; ++slice)
        {
            const unsigned int topLeft = stack * rowSize + slice;
            const unsigned int topRight = topLeft + 1;
            const unsigned int bottomLeft = topLeft + rowSize;
            const unsigned int bottomRight = bottomLeft + 1;

            indices.push_back(topLeft);
            indices.push_back(topRight);
            indices.push_back(bottomLeft);

            indices.push_back(topRight);
            indices.push_back(bottomRight);
            indices.push_back(bottomLeft);
        }
    }
}
} // namespace

int main()
{
    // GLFWを初期化し、windowやOpenGL Contextを作成できる状態にする。
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

    // 800x600のwindowと、それに対応するOpenGL Contextを作成する。
    GLFWwindow* window = glfwCreateWindow(800, 600, "05 Toon Outline", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create a GLFW window.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // 作成したContextを現在のthreadで有効にする。
    // OpenGL関数はContextが有効になった後でなければ使用できない。
    glfwMakeContextCurrent(window);

    // swap intervalを1にし、front/back bufferの交換を画面の垂直同期へ合わせる。
    glfwSwapInterval(1);

    // compile／linkに失敗した途中段階でも安全に後片付けできるよう、最初は0で初期化する。
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint shaderProgram = 0;

    try
    {
        // GLSL fileを読み込み、macOS向けにversion行だけを4.10へ置き換える。
        const std::string vertexSource = MakeMacOSCompatibleSource(
            ReadTextFile(std::string(GLSL_DIRECTORY) + "/05_Toon.vert"));
        const std::string fragmentSource = MakeMacOSCompatibleSource(
            ReadTextFile(std::string(GLSL_DIRECTORY) + "/05_Toon.frag"));

        // OpenGL Context作成後に、Vertex ShaderとFragment Shaderを個別にcompileする。
        vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource, "Vertex Shader");
        fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource, "Fragment Shader");
        if (vertexShader == 0 || fragmentShader == 0)
        {
            throw std::runtime_error("Shader compilation failed.");
        }

        // compile済みの2つのshaderを、描画時に使用する1つのProgramへlinkする。
        shaderProgram = LinkShaderProgram(vertexShader, fragmentShader);
        if (shaderProgram == 0)
        {
            throw std::runtime_error("Shader Program link failed.");
        }

        // link後はProgram側に実行可能な形で保持されるため、個別のshader objectは削除できる。
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        vertexShader = 0;
        fragmentShader = 0;
    }
    catch (const std::exception& error)
    {
        // 失敗した位置によって作成済みobjectが異なるため、0でないものだけを破棄する。
        if (vertexShader != 0)
        {
            glDeleteShader(vertexShader);
        }
        if (fragmentShader != 0)
        {
            glDeleteShader(fragmentShader);
        }
        if (shaderProgram != 0)
        {
            glDeleteProgram(shaderProgram);
        }
        std::cerr << error.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Outlineの膨らみが全方向で確認できるよう、三角形ではなくUV Sphereを生成する。
    // verticesはposition/normal、indicesは各triangleが参照する頂点番号を保持する。
    std::vector<Vertex> vertices;
    std::vector<unsigned int> indices;
    CreateSphere(32, 64, vertices, indices);

    // VAO: VBO／EBOと、各vertex attributeの読み方をまとめて保持する。
    GLuint vao = 0;

    // VBO: Vertex配列本体をGPU側へ保存するbuffer。
    GLuint vbo = 0;

    // EBO: triangleを構成する頂点indexをGPU側へ保存するbuffer。
    GLuint ebo = 0;

    // VAOをbindし、以降のbufferとattribute設定の記録先にする。
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // VBOを作成し、CPU側で生成した全VertexをGPU側へ転送する。
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(
        GL_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(vertices.size() * sizeof(Vertex)),
        vertices.data(),
        GL_STATIC_DRAW);

    // EBOを作成し、glDrawElementsが読む頂点indexをGPU側へ転送する。
    // GL_ELEMENT_ARRAY_BUFFERのbind状態は、現在bind中のVAOへ記録される。
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(
        GL_ELEMENT_ARRAY_BUFFER,
        static_cast<GLsizeiptr>(indices.size() * sizeof(unsigned int)),
        indices.data(),
        GL_STATIC_DRAW);

    // Vertex.positionをGLSLのlayout(location = 0) in vec3 positionOSへ接続する。
    // offsetofを使い、Vertex struct先頭からposition memberまでのbyte offsetを指定する。
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(0);

    // Vertex.normalをGLSLのlayout(location = 1) in vec3 normalOSへ接続する。
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(1);

    // GLSLのuniform名から、このProgram内で値を設定するためのlocationを取得する。
    // matrix uniformsはObject、Camera、Projectionの座標変換に使う。
    const GLint modelLocation = glGetUniformLocation(shaderProgram, "model");
    const GLint viewLocation = glGetUniformLocation(shaderProgram, "view");
    const GLint projectionLocation = glGetUniformLocation(shaderProgram, "projection");
    // Toon uniformsは光の方向、明暗の境界、境界の滑らかさ、3段階の色を制御する。
    const GLint lightDirLocation = glGetUniformLocation(shaderProgram, "lightDirWS");
    const GLint threshold1Location = glGetUniformLocation(shaderProgram, "threshold1");
    const GLint threshold2Location = glGetUniformLocation(shaderProgram, "threshold2");
    const GLint softnessLocation = glGetUniformLocation(shaderProgram, "softness");
    const GLint darkColorLocation = glGetUniformLocation(shaderProgram, "darkColor");
    const GLint midColorLocation = glGetUniformLocation(shaderProgram, "midColor");
    const GLint lightColorLocation = glGetUniformLocation(shaderProgram, "lightColor");
    // Outline uniformsは現在の描画がOutlineかどうか、輪郭色、押し出し幅を制御する。
    const GLint outlinePassLocation = glGetUniformLocation(shaderProgram, "outlinePass");
    const GLint outlineColorLocation = glGetUniformLocation(shaderProgram, "outlineColor");
    const GLint outlineWidthLocation = glGetUniformLocation(shaderProgram, "outlineWidth");

    // Material Inspectorの代わりに、C++側で学習用の固定値を用意する。
    // lightDirWSはFragment Shader内でnormalizeするため、ここでは単位vectorでなくてもよい。
    const glm::vec3 lightDirWS(0.4f, 0.7f, 1.0f);
    const glm::vec3 darkColor(0.08f, 0.12f, 0.2f);
    const glm::vec3 midColor(0.25f, 0.55f, 0.85f);
    const glm::vec3 lightColor(0.85f, 0.95f, 1.0f);
    const glm::vec3 outlineColor(0.01f, 0.01f, 0.015f);

    // 固定値はframeごとに変化しないため、描画loopへ入る前に一度だけGPUへ渡す。
    // glUniformは、glUseProgramで現在選択されているProgramを更新する。
    glUseProgram(shaderProgram);
    glUniform3fv(lightDirLocation, 1, glm::value_ptr(lightDirWS));
    glUniform1f(threshold1Location, 0.4f);
    glUniform1f(threshold2Location, 0.7f);
    glUniform1f(softnessLocation, 0.01f);
    glUniform3fv(darkColorLocation, 1, glm::value_ptr(darkColor));
    glUniform3fv(midColorLocation, 1, glm::value_ptr(midColor));
    glUniform3fv(lightColorLocation, 1, glm::value_ptr(lightColor));
    glUniform3fv(outlineColorLocation, 1, glm::value_ptr(outlineColor));
    glUniform1f(outlineWidthLocation, 0.06f);

    // 深度testを有効にし、Unity ShaderのZTest LEqualに対応する比較方法を指定する。
    // Outlineを先に描いても、後から描く通常サイズのToon表面のほうが手前なので上書きできる。
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // face cullingを有効にする。各PassでGL_FRONT／GL_BACKを切り替える。
    glEnable(GL_CULL_FACE);

    // CreateSphereのindexは、外側から見て反時計回りの面を表面として作っている。
    glFrontFace(GL_CCW);

    // windowが閉じられるまで、画面更新とinput event処理を続ける。
    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        // Retina displayなどではwindow sizeと実際の描画buffer sizeが異なるため、毎frame取得する。
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferHeight == 0)
        {
            glfwPollEvents();
            continue;
        }

        // Clip Spaceをframebuffer全体へ対応付け、前frameの色と深度を消去する。
        // Multi Passの途中ではclearせず、2回の描画結果を同じbufferへ重ねる。
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.22f, 0.24f, 0.28f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // ObjectをY軸回転させるModel matrixを作る。
        // Sphereの輪郭自体は変わらないが、World Space normalとToonの明暗境界が回転する。
        const glm::mat4 model = glm::rotate(
            glm::mat4(1.0f),
            static_cast<float>(glfwGetTime()) * 0.35f,
            glm::vec3(0.0f, 1.0f, 0.0f));
        // Cameraを+Z側へ置き、原点のSphereを見るView matrixを作る。
        const glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 3.2f),
            glm::vec3(0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
        // 現在の縦横比を使い、45度のPerspective Projection matrixを作る。
        const glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            0.1f,
            100.0f);

        // frameごとに変化するmatrixをGLSLのuniform mat4へ渡す。
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, glm::value_ptr(projection));
        // 描画するSphereのVBO／EBO／attribute設定をまとめたVAOを選ぶ。
        glBindVertexArray(vao);

        // Pass 1: Outline
        // outlinePass = 1によりVertex Shaderでnormal方向へ頂点を押し出し、
        // Fragment Shaderではlightingを行わずoutlineColorを返す。
        glUniform1i(outlinePassLocation, 1);

        // Unity ShaderのCull Frontと同じ。膨らませた表面を捨て、裏面だけを残す。
        glCullFace(GL_FRONT);

        // EBOに保存したindexを使い、Sphere全体をtriangleとして1回描画する。
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(indices.size()),
            GL_UNSIGNED_INT,
            nullptr);

        // Pass 2: Toon
        // outlinePass = 0により頂点を元の位置へ戻し、Fragment ShaderでToon shadingを行う。
        glUniform1i(outlinePassLocation, 0);

        // 通常描画ではUnity ShaderのCull Backと同じく、裏面を捨てて表面を描く。
        // 元のSphereがOutlineの中央を上書きし、外にはみ出した部分だけが輪郭として残る。
        glCullFace(GL_BACK);
        glDrawElements(
            GL_TRIANGLES,
            static_cast<GLsizei>(indices.size()),
            GL_UNSIGNED_INT,
            nullptr);

        // 描画を終えたback bufferを表示し、keyboardやwindow操作のeventを処理する。
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 作成したOpenGL objectを逆向きに破棄し、windowとGLFWを終了する。
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
