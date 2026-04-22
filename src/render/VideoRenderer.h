// VideoRenderer.h — OpenGL YUV 渲染器
// 职责：接收 AVFrame (YUV420P)，上传到 GPU 纹理，用 shader 转 RGB 后渲染
// 当前 (Day 5)：提供同步渲染接口 renderFrame()
// 后续 (Day 6)：增加 start(FrameQueue*, Clock*) 启动独立渲染线程 + 音视频同步
#pragma once
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <cstdint>

#include "GLShader.h"
#include "GLTexture.h"

struct AVFrame;
class FrameQueue;
class Clock;
class VideoSurface;

class VideoRenderer
{
public:
    struct Stats
    {
        int renderedFrames = 0;
        int droppedFrames = 0;
        double currentFPS = 0.0;
        double latencyMs = 0.0;
    };

    VideoRenderer();
    ~VideoRenderer();

    // 初始化渲染器：创建 shader、纹理、全屏 quad
    // 必须在 GL 上下文有效时调用
    bool init(VideoSurface* surface, const std::string& shaderDir = "assets/shaders");

    // 同步渲染单帧 (Day 5: 主线程直接调用)
    // 自动处理分辨率变化 (首帧或动态切换)
    void renderFrame(AVFrame* frame);

    // 异步渲染线程 (Day 6: 从 FrameQueue 消费帧，配合 Clock 同步)
    // timeBaseNum/timeBaseDen: AVStream->time_base 的分子/分母
    void start(FrameQueue* frameQueue, Clock* clock, int timeBaseNum, int timeBaseDen);
    void stop();

    // 截图 (glReadPixels → 文件)
    bool screenshot(const std::string& path);

    // Viewport 更新 (窗口 resize 时调用)
    void setViewport(int width, int height);

    const Stats& getStats() const { return m_stats; }
    void resetStats();

    bool isRunning() const { return m_running.load(); }

private:
    void initGL(const std::string& shaderDir);
    void createQuad();

    // 上传 YUV 数据并渲染全屏 quad
    void uploadAndRender(AVFrame* frame);

    // 异步渲染主循环
    void renderLoop();

    VideoSurface* m_surface = nullptr;

    // OpenGL 资源
    GLShader m_yuvShader;
    GLTexture m_texY;       // Y 平面 (亮度, full resolution)
    GLTexture m_texU;       // U 平面 (色度, half resolution)
    GLTexture m_texV;       // V 平面 (色度, half resolution)
    uint32_t m_quadVAO = 0;
    uint32_t m_quadVBO = 0;

    // 视频分辨率 (跟踪分辨率变化)
    int m_videoWidth = 0;
    int m_videoHeight = 0;

    // Viewport 尺寸
    int m_viewportWidth = 0;
    int m_viewportHeight = 0;

    // 异步渲染 (Day 6+)
    FrameQueue* m_frameQueue = nullptr;
    Clock* m_clock = nullptr;
    int m_timeBaseNum = 0;
    int m_timeBaseDen = 1;
    std::thread m_thread;
    std::atomic<bool> m_running{false};

    Stats m_stats;
};
