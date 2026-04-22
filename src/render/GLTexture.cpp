// GLTexture.cpp — 精简版 OpenGL 纹理实现
#include "GLTexture.h"
#include <cstdio>
#include <utility>

GLTexture::GLTexture()
{
    glGenTextures(1, &m_rendererID);
}

GLTexture::~GLTexture()
{
    if (m_rendererID)
    {
        glDeleteTextures(1, &m_rendererID);
        m_rendererID = 0;
    }
}

GLTexture::GLTexture(GLTexture&& other) noexcept
    : m_rendererID(other.m_rendererID)
    , m_width(other.m_width)
    , m_height(other.m_height)
    , m_internalFormat(other.m_internalFormat)
    , m_dataFormat(other.m_dataFormat)
{
    other.m_rendererID = 0;
}

GLTexture& GLTexture::operator=(GLTexture&& other) noexcept
{
    if (this != &other)
    {
        if (m_rendererID)
        {
            glDeleteTextures(1, &m_rendererID);
        }
        m_rendererID = other.m_rendererID;
        m_width = other.m_width;
        m_height = other.m_height;
        m_internalFormat = other.m_internalFormat;
        m_dataFormat = other.m_dataFormat;
        other.m_rendererID = 0;
    }
    return *this;
}

void GLTexture::create(int width, int height, GLenum internalFormat)
{
    m_width = width;
    m_height = height;
    m_internalFormat = internalFormat;

    // 根据内部格式推导上传数据格式
    switch (internalFormat)
    {
    case GL_R8:     m_dataFormat = GL_RED;  break;
    case GL_RG8:    m_dataFormat = GL_RG;   break;
    case GL_RGB8:   m_dataFormat = GL_RGB;  break;
    case GL_RGBA8:  m_dataFormat = GL_RGBA; break;
    default:        m_dataFormat = GL_RED;  break;
    }

    glBindTexture(GL_TEXTURE_2D, m_rendererID);

    // 使用 glTexImage2D 创建可变纹理 (支持后续 resize)
    glTexImage2D(GL_TEXTURE_2D, 0, m_internalFormat,
                 m_width, m_height, 0,
                 m_dataFormat, GL_UNSIGNED_BYTE, nullptr);

    // 线性过滤，适合视频缩放
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    // 边缘 clamp，避免视频边缘出现接缝
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    glBindTexture(GL_TEXTURE_2D, 0);
}

void GLTexture::update(const uint8_t* data, int stride)
{
    if (!data || m_width <= 0 || m_height <= 0)
    {
        return;
    }

    glBindTexture(GL_TEXTURE_2D, m_rendererID);

    // 关键：处理 stride != width 的情况
    // FFmpeg 解码帧的 linesize 通常大于实际宽度 (内存对齐到 32/64 字节)
    // 不设置此参数会导致画面绿条/错位
    glPixelStorei(GL_UNPACK_ROW_LENGTH, stride);

    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0,
                    m_width, m_height,
                    m_dataFormat, GL_UNSIGNED_BYTE, data);

    // 重置为默认值，避免影响其他纹理操作
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
}

void GLTexture::resize(int width, int height)
{
    if (width == m_width && height == m_height)
    {
        return;
    }

    // 重新分配纹理存储 (保持原有格式)
    create(width, height, m_internalFormat);
}

void GLTexture::bind(uint32_t slot) const
{
    glActiveTexture(GL_TEXTURE0 + slot);
    glBindTexture(GL_TEXTURE_2D, m_rendererID);
}

void GLTexture::unbind() const
{
    glBindTexture(GL_TEXTURE_2D, 0);
}
