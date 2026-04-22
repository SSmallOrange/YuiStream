// GLShader.cpp — 精简版 OpenGL Shader 实现
#include "GLShader.h"
#include <cstdio>
#include <fstream>
#include <vector>

GLShader::~GLShader()
{
    if (m_rendererID)
    {
        glDeleteProgram(m_rendererID);
        m_rendererID = 0;
    }
}

bool GLShader::loadFromFile(const std::string& vertPath, const std::string& fragPath)
{
    std::string vertSrc = readFile(vertPath);
    std::string fragSrc = readFile(fragPath);

    if (vertSrc.empty())
    {
        printf("[GLShader] Failed to read vertex shader: %s\n", vertPath.c_str());
        return false;
    }
    if (fragSrc.empty())
    {
        printf("[GLShader] Failed to read fragment shader: %s\n", fragPath.c_str());
        return false;
    }

    return loadFromSource(vertSrc, fragSrc);
}

bool GLShader::loadFromSource(const std::string& vertSrc, const std::string& fragSrc)
{
    // 清理旧 program
    if (m_rendererID)
    {
        glDeleteProgram(m_rendererID);
        m_rendererID = 0;
        m_uniformLocationCache.clear();
    }

    GLuint vertShader = compileShader(GL_VERTEX_SHADER, vertSrc);
    if (vertShader == 0)
    {
        return false;
    }

    GLuint fragShader = compileShader(GL_FRAGMENT_SHADER, fragSrc);
    if (fragShader == 0)
    {
        glDeleteShader(vertShader);
        return false;
    }

    bool success = linkProgram(vertShader, fragShader);

    // shader 已附加到 program，可以安全删除
    glDeleteShader(vertShader);
    glDeleteShader(fragShader);

    return success;
}

GLuint GLShader::compileShader(GLenum type, const std::string& source)
{
    GLuint shader = glCreateShader(type);
    const char* src = source.c_str();
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);

    GLint isCompiled = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &isCompiled);
    if (isCompiled == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &maxLength);

        std::vector<char> infoLog(maxLength);
        glGetShaderInfoLog(shader, maxLength, &maxLength, infoLog.data());

        const char* typeName = (type == GL_VERTEX_SHADER) ? "VERTEX" : "FRAGMENT";
        printf("[GLShader] %s compile error:\n%s\n", typeName, infoLog.data());

        glDeleteShader(shader);
        return 0;
    }

    return shader;
}

bool GLShader::linkProgram(GLuint vertShader, GLuint fragShader)
{
    GLuint program = glCreateProgram();
    glAttachShader(program, vertShader);
    glAttachShader(program, fragShader);
    glLinkProgram(program);

    GLint isLinked = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &isLinked);
    if (isLinked == GL_FALSE)
    {
        GLint maxLength = 0;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &maxLength);

        std::vector<char> infoLog(maxLength);
        glGetProgramInfoLog(program, maxLength, &maxLength, infoLog.data());

        printf("[GLShader] Link error:\n%s\n", infoLog.data());

        glDeleteProgram(program);
        return false;
    }

    // 链接成功后 detach shader (最佳实践)
    glDetachShader(program, vertShader);
    glDetachShader(program, fragShader);

    m_rendererID = program;
    return true;
}

void GLShader::bind() const
{
    glUseProgram(m_rendererID);
}

void GLShader::unbind() const
{
    glUseProgram(0);
}

void GLShader::setInt(const std::string& name, int value)
{
    GLint location = getUniformLocation(name);
    if (location != -1)
    {
        glUniform1i(location, value);
    }
}

void GLShader::setFloat(const std::string& name, float value)
{
    GLint location = getUniformLocation(name);
    if (location != -1)
    {
        glUniform1f(location, value);
    }
}

GLint GLShader::getUniformLocation(const std::string& name)
{
    auto it = m_uniformLocationCache.find(name);
    if (it != m_uniformLocationCache.end())
    {
        return it->second;
    }

    GLint location = glGetUniformLocation(m_rendererID, name.c_str());
    if (location == -1)
    {
        printf("[GLShader] Warning: uniform '%s' not found (program=%u)\n",
               name.c_str(), m_rendererID);
    }
    m_uniformLocationCache[name] = location;
    return location;
}

std::string GLShader::readFile(const std::string& filepath)
{
    std::ifstream file(filepath, std::ios::in | std::ios::binary);
    if (!file)
    {
        return "";
    }

    file.seekg(0, std::ios::end);
    std::string content;
    content.resize(static_cast<size_t>(file.tellg()));
    file.seekg(0, std::ios::beg);
    file.read(&content[0], content.size());
    return content;
}
