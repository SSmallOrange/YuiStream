// GLTexture.h — 精简版 OpenGL 纹理封装
// 参照 Yuicy::OpenGLTexture2D 重写，支持 GL_R8 单通道格式 (YUV 渲染)
// 与 Yuicy 的区别：使用 glTexImage2D (可变纹理) 而非 glTextureStorage2D (不可变),
// 因为视频分辨率可能动态变化，需要 resize 能力
#pragma once
#include <glad/glad.h>
#include <cstdint>

class GLTexture
{
public:
    GLTexture();
    ~GLTexture();

    // 禁止拷贝，允许移动
    GLTexture(const GLTexture&) = delete;
    GLTexture& operator=(const GLTexture&) = delete;
    GLTexture(GLTexture&& other) noexcept;
    GLTexture& operator=(GLTexture&& other) noexcept;

    // 创建/重建纹理 (分辨率变化或首次创建时调用)
    // internalFormat: GL_R8 (YUV 单通道) 或 GL_RGBA8 等
    void create(int width, int height, GLenum internalFormat = GL_R8);

    // 高频更新纹理数据 (每帧调用)
    // data: 像素数据指针 (如 AVFrame->data[0])
    // stride: 行字节跨度 (如 AVFrame->linesize[0])，可能比 width 大 (内存对齐填充)
    void update(const uint8_t* data, int stride);

    // 重新分配纹理尺寸 (保持原有 internalFormat)
    void resize(int width, int height);

    // 绑定到指定纹理槽位
    void bind(uint32_t slot = 0) const;
    void unbind() const;

    uint32_t getRendererID() const { return m_rendererID; }
    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }

private:
    uint32_t m_rendererID = 0;
    int m_width = 0;
    int m_height = 0;
    GLenum m_internalFormat = GL_R8;
    GLenum m_dataFormat = GL_RED;     // 与 internalFormat 对应的数据格式
};
