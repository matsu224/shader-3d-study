// macOSでOpenGLが非推奨であることによるwarningだけを抑制する。
#define GL_SILENCE_DEPRECATION
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <OpenGL/gl3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <png.h>

#include <algorithm>
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
// memberの並びをGLSLのposition／uv／normal／tangent attributeへ対応させる。
struct Vertex
{
    glm::vec3 position;
    glm::vec2 uv;
    glm::vec3 normal;
    glm::vec4 tangent;
};

// libpngで展開したRGBA8画像を保持する。
// pixelsは左下を原点とするOpenGLのUVに合わせ、読み込み時に上下反転しておく。
struct ImageRgba8
{
    int width = 0;
    int height = 0;
    std::vector<unsigned char> pixels;
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

    glDeleteShader(shader);
    return 0;
}

// compile済みのVertex ShaderとFragment Shaderを、描画に使う1つのProgramへlinkする。
// compileは各shader単体を検証し、linkはshader同士の入出力やuniformも含めて接続する。
GLuint LinkShaderProgram(GLuint vertexShader, GLuint fragmentShader)
{
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

    glDeleteProgram(shaderProgram);
    return 0;
}

// PNG fileをlibpngの簡易APIで読み込み、必ずRGBA8へ展開する。
// PNGは左上、OpenGLのUVは左下を原点として扱うため、最後にrowを上下反転する。
ImageRgba8 LoadPngRgba8(const std::string& path)
{
    png_image pngImage{};
    pngImage.version = PNG_IMAGE_VERSION;

    if (png_image_begin_read_from_file(&pngImage, path.c_str()) == 0)
    {
        throw std::runtime_error("Failed to read PNG header: " + path + "\n" + pngImage.message);
    }

    // 元画像のchannel数に関係なく、OpenGLへ渡しやすいRGBA8へ変換する。
    pngImage.format = PNG_FORMAT_RGBA;

    ImageRgba8 image;
    image.width = static_cast<int>(pngImage.width);
    image.height = static_cast<int>(pngImage.height);
    image.pixels.resize(PNG_IMAGE_SIZE(pngImage));

    if (png_image_finish_read(&pngImage, nullptr, image.pixels.data(), 0, nullptr) == 0)
    {
        const std::string message = pngImage.message;
        png_image_free(&pngImage);
        throw std::runtime_error("Failed to decode PNG: " + path + "\n" + message);
    }
    png_image_free(&pngImage);

    // 1pixelはRGBAの4byte。上半分と下半分の同じ位置にあるbyteを交換する。
    const std::size_t rowByteCount = static_cast<std::size_t>(image.width) * 4;
    for (int y = 0; y < image.height / 2; ++y)
    {
        const std::size_t topRow = static_cast<std::size_t>(y) * rowByteCount;
        const std::size_t bottomRow =
            static_cast<std::size_t>(image.height - 1 - y) * rowByteCount;

        for (std::size_t x = 0; x < rowByteCount; ++x)
        {
            std::swap(image.pixels[topRow + x], image.pixels[bottomRow + x]);
        }
    }

    return image;
}

// CPU側のRGBA8画像からOpenGL Texture Objectを作る。
// Normal Mapは色ではなくvector dataなので、sRGB変換しないGL_RGBA8として保存する。
GLuint CreateNormalMapTexture(const ImageRgba8& image)
{
    GLuint texture = 0;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // UVが0～1を越えた場合は繰り返し、縮小時はmipmapを線形補間してsampleする。
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 4byte境界で並ぶRGBA8 dataをmipmap level 0へ転送する。
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_RGBA8,
        image.width,
        image.height,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image.pixels.data());

    // 遠くで細かなNormal Mapがちらつきにくいよう、縮小表示用mipmapを生成する。
    glGenerateMipmap(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
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
    GLFWwindow* window = glfwCreateWindow(800, 600, "07 Normal Map", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create a GLFW window.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // 作成したContextを現在のthreadで有効にし、表示更新を垂直同期へ合わせる。
    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    // 途中で失敗しても、作成済みのOpenGL objectだけを安全に破棄できるよう0で初期化する。
    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint shaderProgram = 0;
    GLuint normalMapTexture = 0;

    try
    {
        // GLSL fileを読み込み、macOS向けにversion行だけを4.10へ置き換える。
        const std::string vertexSource = MakeMacOSCompatibleSource(
            ReadTextFile(std::string(GLSL_DIRECTORY) + "/07_NormalMap.vert"));
        const std::string fragmentSource = MakeMacOSCompatibleSource(
            ReadTextFile(std::string(GLSL_DIRECTORY) + "/07_NormalMap.frag"));

        // Vertex ShaderとFragment Shaderを個別にcompileする。
        vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource, "Vertex Shader");
        fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource, "Fragment Shader");
        if (vertexShader == 0 || fragmentShader == 0)
        {
            throw std::runtime_error("Shader compilation failed.");
        }

        // compile済みshaderを1つのProgramへlinkする。
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

        // Unity側と同じNormal Map PNGを読み込み、GPUのTexture Objectへ転送する。
        const ImageRgba8 normalMapImage = LoadPngRgba8(NORMAL_MAP_PATH);
        normalMapTexture = CreateNormalMapTexture(normalMapImage);
        std::cout << "Normal Map loaded: " << normalMapImage.width << "x"
                  << normalMapImage.height << "\n" << std::flush;
    }
    catch (const std::exception& error)
    {
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
        if (normalMapTexture != 0)
        {
            glDeleteTextures(1, &normalMapTexture);
        }

        std::cerr << error.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // Camera側を向く1枚の板を、2枚のtriangleとして作る。
    // UVを0～2にしてNormal Mapを縦横2回ずつrepeatし、凹凸の変化を確認しやすくする。
    // normalは+Z、tangent.xyzは+X、tangent.wはTBNの向きを表す+1。
    const Vertex vertices[] = {
        // position                 // uv          // normal              // tangent
        {{-1.2f, -1.2f, 0.0f},     {0.0f, 0.0f}, {0.0f, 0.0f, 1.0f},    {1.0f, 0.0f, 0.0f, 1.0f}},
        {{ 1.2f, -1.2f, 0.0f},     {2.0f, 0.0f}, {0.0f, 0.0f, 1.0f},    {1.0f, 0.0f, 0.0f, 1.0f}},
        {{ 1.2f,  1.2f, 0.0f},     {2.0f, 2.0f}, {0.0f, 0.0f, 1.0f},    {1.0f, 0.0f, 0.0f, 1.0f}},
        {{-1.2f,  1.2f, 0.0f},     {0.0f, 2.0f}, {0.0f, 0.0f, 1.0f},    {1.0f, 0.0f, 0.0f, 1.0f}},
    };

    // 外側から見て反時計回りになるよう、4頂点を2枚のtriangleへ分ける。
    const unsigned int indices[] = {
        0, 1, 2,
        0, 2, 3,
    };

    // VAOはVBO／EBOとattributeの読み方、VBOは頂点data、EBOは頂点indexを保持する。
    GLuint vao = 0;
    GLuint vbo = 0;
    GLuint ebo = 0;

    // VAOをbindし、以降のbufferとattribute設定の記録先にする。
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // 4頂点分のVertex dataをGPU側のVBOへ転送する。
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    // 2枚のtriangleが参照するindexをEBOへ転送する。
    glGenBuffers(1, &ebo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);

    // Vertex.positionをGLSLのlayout(location = 0) in vec3 positionOSへ接続する。
    glVertexAttribPointer(
        0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, position)));
    glEnableVertexAttribArray(0);

    // Vertex.uvをGLSLのlayout(location = 1) in vec2 uvへ接続する。
    glVertexAttribPointer(
        1, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, uv)));
    glEnableVertexAttribArray(1);

    // Vertex.normalをGLSLのlayout(location = 2) in vec3 normalOSへ接続する。
    glVertexAttribPointer(
        2, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, normal)));
    glEnableVertexAttribArray(2);

    // Vertex.tangentをGLSLのlayout(location = 3) in vec4 tangentOSへ接続する。
    glVertexAttribPointer(
        3, 4, GL_FLOAT, GL_FALSE, sizeof(Vertex),
        reinterpret_cast<void*>(offsetof(Vertex, tangent)));
    glEnableVertexAttribArray(3);

    // GLSLのuniform名から、このProgram内で値を設定するためのlocationを取得する。
    const GLint modelLocation = glGetUniformLocation(shaderProgram, "model");
    const GLint viewLocation = glGetUniformLocation(shaderProgram, "view");
    const GLint projectionLocation = glGetUniformLocation(shaderProgram, "projection");
    const GLint normalMapLocation = glGetUniformLocation(shaderProgram, "normalMap");
    const GLint lightDirLocation = glGetUniformLocation(shaderProgram, "lightDirWS");
    const GLint baseColorLocation = glGetUniformLocation(shaderProgram, "baseColor");

    // UnityのMaterialとMain Lightの代わりに、C++側で固定値を用意する。
    // 光はsurfaceからlightへ向かうWorld Space方向としてFragment Shaderへ渡す。
    const glm::vec3 lightDirWS(0.35f, 0.45f, 1.0f);
    const glm::vec3 baseColor(0.75f, 0.78f, 0.82f);

    glUseProgram(shaderProgram);
    glUniform3fv(lightDirLocation, 1, glm::value_ptr(lightDirWS));
    glUniform3fv(baseColorLocation, 1, glm::value_ptr(baseColor));

    // sampler2D normalMapへTexture Unit 0を使うことを伝える。
    // glUniform1iへTexture ObjectのIDではなく、Texture Unit番号の0を渡す点に注意する。
    glUniform1i(normalMapLocation, 0);

    // Texture Unit 0へNormal MapのTexture Objectをbindする。
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, normalMapTexture);

    // 板を傾けても奥行き関係が正しく処理されるよう、深度testを有効にする。
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);

    // 裏側のtriangleを描かないようface cullingを有効にし、反時計回りを表面とする。
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // windowが閉じられるまで、画面更新とinput event処理を続ける。
    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        // Retina displayではwindow sizeと描画buffer sizeが異なるため、毎frame取得する。
        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferHeight == 0)
        {
            glfwPollEvents();
            continue;
        }

        // Clip Spaceをframebuffer全体へ対応付け、前frameの色と深度を消去する。
        glViewport(0, 0, framebufferWidth, framebufferHeight);
        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // 板を少し傾け、Y軸方向へ左右に揺らす。
        // 360度回転させるとCull Backによって裏側を向く時間が長いため、正面が見える範囲で往復させる。
        // このModel変換に合わせてT/B/Nも変わり、凹凸に当たる光の変化を確認できる。
        glm::mat4 model = glm::rotate(
            glm::mat4(1.0f),
            glm::radians(-12.0f),
            glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(
            model,
            std::sin(static_cast<float>(glfwGetTime()) * 0.6f) * glm::radians(25.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));

        // Cameraを+Z側へ置き、原点の板を見るView matrixを作る。
        const glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 3.8f),
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

        // VAOからposition／uv／normal／tangentとindex設定を復元し、板全体を描画する。
        glBindVertexArray(vao);
        glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, nullptr);

        // 描画を終えたback bufferを表示し、keyboardやwindow操作のeventを処理する。
        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 作成したOpenGL objectを破棄し、windowとGLFWを終了する。
    glDeleteBuffers(1, &ebo);
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteTextures(1, &normalMapTexture);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
