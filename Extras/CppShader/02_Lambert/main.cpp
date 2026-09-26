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
GLuint CompileShader(GLenum shaderType, const std::string& source, const std::string& label)
{
    // shaderの種類を指定し、空のshader objectを作成する。
    const GLuint shader = glCreateShader(shaderType);

    // C++文字列のGLSL sourceをshader objectへ渡す。
    const char* sourcePointer = source.c_str();
    glShaderSource(shader, 1, &sourcePointer, nullptr);

    // GLSL sourceをcompileする。
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
GLuint LinkShaderProgram(GLuint vertexShader, GLuint fragmentShader)
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
        std::cout << "Shader Program link succeeded.\n" << std::flush;
        return shaderProgram;
    }

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

    return shaderProgram;
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
    GLFWwindow* window = glfwCreateWindow(800, 600, "02 Lambert", nullptr, nullptr);
    if (window == nullptr)
    {
        std::cerr << "Failed to create a GLFW window.\n";
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // 作成したwindowのOpenGL Contextを、現在のthreadで使用するContextにする。
    glfwMakeContextCurrent(window);

    GLuint vertexShader = 0;
    GLuint fragmentShader = 0;
    GLuint shaderProgram = 0;

    try
    {
        // GLSL fileをC++の文字列として読み込む。
        const std::string vertexSource = MakeMacOSCompatibleSource(
            ReadTextFile(std::string(GLSL_DIRECTORY) + "/02_Lambert.vert"));
        const std::string fragmentSource = MakeMacOSCompatibleSource(
            ReadTextFile(std::string(GLSL_DIRECTORY) + "/02_Lambert.frag"));

        // OpenGL Context作成後に、Vertex ShaderとFragment Shaderを個別にcompileする。
        vertexShader = CompileShader(GL_VERTEX_SHADER, vertexSource, "Vertex Shader");
        fragmentShader = CompileShader(GL_FRAGMENT_SHADER, fragmentSource, "Fragment Shader");

        // compile済みの2つのshaderを、描画時に使用する1つのProgramへlinkする。
        shaderProgram = LinkShaderProgram(vertexShader, fragmentShader);

        // link後はProgram側に結果が保持されるため、個別のshader objectは削除できる。
        glDeleteShader(vertexShader);
        glDeleteShader(fragmentShader);
        vertexShader = 0;
        fragmentShader = 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << error.what() << '\n';
        glfwDestroyWindow(window);
        glfwTerminate();
        return EXIT_FAILURE;
    }

    // 00_Basicと同じ、position.xyz + normal.xyzを持つ三角形。
    const float triangleVertices[] = {
        // position             // normal
        -0.7f, -0.6f, 0.0f,     0.0f, 0.0f, 1.0f,
         0.7f, -0.6f, 0.0f,     0.0f, 0.0f, 1.0f,
         0.0f,  0.7f, 0.0f,     0.0f, 0.0f, 1.0f,
    };

    // VAO: どのVBOを、どのattributeとして読むかという設定をまとめて保持する。
    GLuint vao = 0;

    // VBO: positionなどの頂点データ本体をGPU側に保存するbuffer。
    GLuint vbo = 0;

    // VAOを作成してbindし、以降の頂点attribute設定の記録先にする。
    glGenVertexArrays(1, &vao);
    glBindVertexArray(vao);

    // VBOを作成してbindし、C++側の3頂点をGPU側のbufferへ転送する。
    glGenBuffers(1, &vbo);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(triangleVertices), triangleVertices, GL_STATIC_DRAW);

    constexpr GLsizei stride = 6 * sizeof(float);

    // VBO先頭のposition.xyzを、GLSLのlayout(location = 0)へ接続する。
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, stride, nullptr);

    // location 0を有効にし、draw時にVBOから頂点データを読み込めるようにする。
    glEnableVertexAttribArray(0);

    // positionの3要素後にあるnormal.xyzを、GLSLのlayout(location = 1)へ接続する。
    glVertexAttribPointer(
        1, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    // GLSLのuniform名から、このProgram内のmatrix uniform locationを取得する。
    const GLint modelLocation = glGetUniformLocation(shaderProgram, "model");
    const GLint viewLocation = glGetUniformLocation(shaderProgram, "view");
    const GLint projectionLocation = glGetUniformLocation(shaderProgram, "projection");

    // Lambertで追加されたWorld Spaceのlight方向とsurfaceの基本色のlocation。
    const GLint lightDirLocation = glGetUniformLocation(shaderProgram, "lightDirWS");
    const GLint baseColorLocation = glGetUniformLocation(shaderProgram, "baseColor");

    // 固定値のlight方向と基本色は、描画loopへ入る前に一度だけ渡せばよい。
    const glm::vec3 lightDirWS(0.0f, 0.0f, 1.0f);
    const glm::vec3 baseColor(0.9f, 0.45f, 0.15f);
    glUseProgram(shaderProgram);
    glUniform3fv(lightDirLocation, 1, glm::value_ptr(lightDirWS));
    glUniform3fv(baseColorLocation, 1, glm::value_ptr(baseColor));

    // windowが閉じられるまで、画面の更新と入力などのevent処理を続ける。
    while (glfwWindowShouldClose(window) == GLFW_FALSE)
    {
        glClearColor(0.08f, 0.09f, 0.12f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        int framebufferWidth = 0;
        int framebufferHeight = 0;
        glfwGetFramebufferSize(window, &framebufferWidth, &framebufferHeight);
        if (framebufferHeight == 0)
        {
            glfwPollEvents();
            continue;
        }

        glViewport(0, 0, framebufferWidth, framebufferHeight);

        // GLMでObject、Camera、Perspective Projectionのmatrixを作る。
        // 00_Basicと同様にObjectをY軸回転させ、World Spaceのnormalを変化させる。
        const glm::mat4 model = glm::rotate(
            glm::mat4(1.0f),
            static_cast<float>(glfwGetTime()) * 0.5f,
            glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 view = glm::lookAt(
            glm::vec3(0.0f, 0.0f, 2.5f),
            glm::vec3(0.0f, 0.0f, 0.0f),
            glm::vec3(0.0f, 1.0f, 0.0f));
        const glm::mat4 projection = glm::perspective(
            glm::radians(45.0f),
            static_cast<float>(framebufferWidth) / static_cast<float>(framebufferHeight),
            0.1f,
            100.0f);

        // C++側のmatrixをuniform mat4としてVertex Shaderへ渡す。
        glUseProgram(shaderProgram);
        glUniformMatrix4fv(modelLocation, 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(viewLocation, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projectionLocation, 1, GL_FALSE, glm::value_ptr(projection));

        // 描画に使うVAOを選び、position/normalをVBOから読む設定を有効にする。
        glBindVertexArray(vao);

        // 3頂点を1枚の三角形として描画する。この呼び出しをきっかけにshaderがGPU上で実行される。
        glDrawArrays(GL_TRIANGLES, 0, 3);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    // 使用したOpenGL object、window、GLFWを終了処理する。
    glDeleteBuffers(1, &vbo);
    glDeleteVertexArrays(1, &vao);
    glDeleteProgram(shaderProgram);
    glfwDestroyWindow(window);
    glfwTerminate();
    return EXIT_SUCCESS;
}
