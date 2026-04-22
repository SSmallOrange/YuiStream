// GLShader.h — 精简版 OpenGL Shader 封装
// 参照 Yuicy::OpenGLShader 重写，移除 Asset 基类/Ref<>/glm 依赖
// 只保留视频渲染所需的 uniform 类型：int, float
#pragma once
#include <glad/glad.h>
#include <string>
#include <cstdint>
#include <unordered_map>

class GLShader
{
public:
    GLShader() = default;
    ~GLShader();

    // 禁止拷贝
    GLShader(const GLShader&) = delete;
    GLShader& operator=(const GLShader&) = delete;

    // 从文件加载 (vertex + fragment 分离文件)
    bool loadFromFile(const std::string& vertPath, const std::string& fragPath);

    // 从源码字符串加载
    bool loadFromSource(const std::string& vertSrc, const std::string& fragSrc);

    void bind() const;
    void unbind() const;

    // Uniform 设置 (视频渲染只需 int 和 float)
    void setInt(const std::string& name, int value);
    void setFloat(const std::string& name, float value);

    uint32_t getRendererID() const { return m_rendererID; }
    bool isValid() const { return m_rendererID != 0; }

private:
    // 编译单个 shader
    GLuint compileShader(GLenum type, const std::string& source);

    // 链接 program
    bool linkProgram(GLuint vertShader, GLuint fragShader);

    // 读取文件内容
    static std::string readFile(const std::string& filepath);

    // 缓存 uniform location，避免每帧重复查询
    GLint getUniformLocation(const std::string& name);

    uint32_t m_rendererID = 0;
    mutable std::unordered_map<std::string, GLint> m_uniformLocationCache;
};
