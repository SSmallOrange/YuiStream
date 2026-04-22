// VideoRenderer.cpp — OpenGL YUV 渲染器实现
#include "VideoRenderer.h"
#include "VideoSurface.h"
#include "../core/FrameQueue.h"
#include <glad/glad.h>
#include <cstdio>
#include <chrono>

extern "C" {
    #include <libavutil/frame.h>
}

// 全屏 quad 顶点数据 (NDC 坐标 + UV)
// TRIANGLE_STRIP: 4 顶点覆盖整个 [-1,1] NDC 空间
// UV 翻转 Y 轴：视频帧数据从上到下存储，OpenGL 纹理坐标从下到上
static const float kQuadVertices[] = {
    // Position     TexCoord
    -1.0f,  1.0f,   0.0f, 0.0f,    // 左上
    -1.0f, -1.0f,   0.0f, 1.0f,    // 左下
     1.0f,  1.0f,   1.0f, 0.0f,    // 右上
     1.0f, -1.0f,   1.0f, 1.0f,    // 右下
};

VideoRenderer::VideoRenderer() = default;

VideoRenderer::~VideoRenderer()
{
    stop();

    // GL 资源清理 (需要在有效 GL 上下文中)
    if (m_quadVAO)
    {
        glDeleteVertexArrays(1, &m_quadVAO);
        m_quadVAO = 0;
    }
    if (m_quadVBO)
    {
        glDeleteBuffers(1, &m_quadVBO);
        m_quadVBO = 0;
    }
}

bool VideoRenderer::init(VideoSurface* surface, const std::string& shaderDir)
{
    if (!surface)
    {
        printf("[VideoRenderer] Error: surface is null\n");
        return false;
    }

    m_surface = surface;
    m_viewportWidth = surface->getWidth();
    m_viewportHeight = surface->getHeight();

    initGL(shaderDir);

    if (!m_yuvShader.isValid())
    {
        return false;
    }

    printf("[VideoRenderer] Initialized (viewport=%dx%d)\n",
           m_viewportWidth, m_viewportHeight);
    return true;
}

void VideoRenderer::initGL(const std::string& shaderDir)
{
    // 加载 YUV shader
    std::string vertPath = shaderDir + "/yuv420p.vert";
    std::string fragPath = shaderDir + "/yuv420p.frag";

    if (!m_yuvShader.loadFromFile(vertPath, fragPath))
    {
        printf("[VideoRenderer] Failed to load YUV shader from: %s\n", shaderDir.c_str());
        return;
    }
    printf("[VideoRenderer] YUV shader loaded (program=%u)\n", m_yuvShader.getRendererID());

    // 创建全屏 quad
    createQuad();

    // 设置初始 Viewport
    glViewport(0, 0, m_viewportWidth, m_viewportHeight);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
}

void VideoRenderer::createQuad()
{
    glGenVertexArrays(1, &m_quadVAO);
    glGenBuffers(1, &m_quadVBO);

    glBindVertexArray(m_quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(kQuadVertices), kQuadVertices, GL_STATIC_DRAW);

    // layout(location = 0) — a_Position (vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // layout(location = 1) — a_TexCoord (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

void VideoRenderer::renderFrame(AVFrame* frame)
{
    if (!frame || !m_yuvShader.isValid())
    {
        return;
    }

    uploadAndRender(frame);
    m_surface->swapBuffers();
    m_stats.renderedFrames++;
}

void VideoRenderer::uploadAndRender(AVFrame* frame)
{
    // 检测分辨率变化 (首帧或流切换时重建纹理)
    if (m_videoWidth != frame->width || m_videoHeight != frame->height)
    {
        m_videoWidth = frame->width;
        m_videoHeight = frame->height;

        // 创建/重建 YUV 三平面纹理
        m_texY.create(m_videoWidth, m_videoHeight, GL_R8);
        m_texU.create(m_videoWidth / 2, m_videoHeight / 2, GL_R8);
        m_texV.create(m_videoWidth / 2, m_videoHeight / 2, GL_R8);

        printf("[VideoRenderer] Textures created: Y=%dx%d, U=%dx%d, V=%dx%d\n",
               m_texY.getWidth(), m_texY.getHeight(),
               m_texU.getWidth(), m_texU.getHeight(),
               m_texV.getWidth(), m_texV.getHeight());
    }

    // 上传 YUV 数据到 GPU
    // data[0] = Y 平面, linesize[0] = Y stride
    // data[1] = U 平面, linesize[1] = U stride (宽高各为 Y 的一半)
    // data[2] = V 平面, linesize[2] = V stride
    m_texY.update(frame->data[0], frame->linesize[0]);
    m_texU.update(frame->data[1], frame->linesize[1]);
    m_texV.update(frame->data[2], frame->linesize[2]);

    // 绑定 shader + 纹理 → 渲染全屏 quad
    glClear(GL_COLOR_BUFFER_BIT);

    m_yuvShader.bind();

    m_texY.bind(0);
    m_texU.bind(1);
    m_texV.bind(2);

    m_yuvShader.setInt("u_TexY", 0);
    m_yuvShader.setInt("u_TexU", 1);
    m_yuvShader.setInt("u_TexV", 2);

    glBindVertexArray(m_quadVAO);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void VideoRenderer::setViewport(int width, int height)
{
    m_viewportWidth = width;
    m_viewportHeight = height;
    glViewport(0, 0, width, height);
}

void VideoRenderer::resetStats()
{
    m_stats = {};
}

void VideoRenderer::start(FrameQueue* frameQueue, Clock* clock, int timeBaseNum, int timeBaseDen)
{
    if (m_running.load())
    {
        return;
    }

    m_frameQueue = frameQueue;
    m_clock = clock;
    m_timeBaseNum = timeBaseNum;
    m_timeBaseDen = timeBaseDen;
    m_running.store(true);

    // 重要: 主线程释放 GL 上下文，渲染线程会在 renderLoop 中 makeCurrent
    m_surface->releaseCurrent();

    m_thread = std::thread(&VideoRenderer::renderLoop, this);
}

void VideoRenderer::stop()
{
    m_running.store(false);
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    m_frameQueue = nullptr;
    m_clock = nullptr;
}

void VideoRenderer::renderLoop()
{
    printf("[VideoRenderer] Render thread started\n");

    // GL 上下文绑定到渲染线程 (OpenGL 规定一个上下文只能在一个线程上活动)
    m_surface->makeCurrent();

    // PTS 帧节奏控制
    // 原理：以第一帧为基准，根据后续帧的 PTS 差值计算应该展示的时刻
    // Week 2 会用音频 Clock 替代，当前先用 PTS 自身做简易同步
    bool firstFrame = true;
    int64_t basePTS = 0;
    auto baseTime = std::chrono::steady_clock::now();
    double timeBaseDouble = (m_timeBaseDen > 0)
        ? static_cast<double>(m_timeBaseNum) / m_timeBaseDen
        : 0.0;

    // FPS 统计
    auto lastStatsTime = std::chrono::steady_clock::now();
    int framesInSecond = 0;

    while (m_running.load())
    {
        // 从 FrameQueue 取帧 (100ms 超时，定期检查 m_running)
        AVFrame* frame = m_frameQueue->pop(100);
        if (!frame)
        {
            if (m_frameQueue->isClosed())
            {
                printf("[VideoRenderer] FrameQueue closed, exiting render loop\n");
                break;
            }
            continue;  // 超时，继续
        }

        // --- PTS 帧节奏控制 ---
        if (firstFrame)
        {
            basePTS = frame->pts;
            baseTime = std::chrono::steady_clock::now();
            firstFrame = false;
        }
        else if (frame->pts != AV_NOPTS_VALUE && timeBaseDouble > 0.0)
        {
            // 计算这一帧相对于第一帧的应该展示时间
            double ptsDiffSec = (frame->pts - basePTS) * timeBaseDouble;
            auto targetTime = baseTime + std::chrono::duration<double>(ptsDiffSec);
            auto now = std::chrono::steady_clock::now();

            if (targetTime > now)
            {
                // 帧到达太早 → 等待到正确时刻
                std::this_thread::sleep_until(targetTime);
            }
            // 帧到达太晚 → 不丢帧、直接渲染 (丢帧策略留到 Week 2 配合 Clock)
        }

        uploadAndRender(frame);
        m_surface->swapBuffers();
        m_stats.renderedFrames++;
        framesInSecond++;

        av_frame_free(&frame);  // pop 返回的帧由调用者释放

        // FPS 统计
        auto now = std::chrono::steady_clock::now();
        double elapsed = std::chrono::duration<double>(now - lastStatsTime).count();
        if (elapsed >= 1.0)
        {
            m_stats.currentFPS = framesInSecond / elapsed;
            framesInSecond = 0;
            lastStatsTime = now;
        }
    }

    m_surface->releaseCurrent();
    printf("[VideoRenderer] Render thread stopped (rendered=%d, dropped=%d)\n",
           m_stats.renderedFrames, m_stats.droppedFrames);
}

bool VideoRenderer::screenshot(const std::string& path)
{
    // TODO: Week 3 — glReadPixels → PNG
    printf("[VideoRenderer] Screenshot not yet implemented\n");
    return false;
}
