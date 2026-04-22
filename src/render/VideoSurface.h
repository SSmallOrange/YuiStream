// VideoSurface.h — 窗口/GL 上下文抽象接口
// Phase 1: SDLVideoSurface (独立窗口)
// Phase 2: NativeVideoSurface (Qt HWND 嵌入)
#pragma once

class VideoSurface
{
public:
    virtual ~VideoSurface() = default;

    // 创建窗口 + GL 上下文
    virtual bool create(int width, int height, const char* title) = 0;
    virtual void destroy() = 0;

    // GL 上下文操作 (多线程渲染必需: 渲染线程 makeCurrent，主线程 release)
    virtual void makeCurrent() = 0;
    virtual void releaseCurrent() = 0;
    virtual void swapBuffers() = 0;

    virtual void resize(int width, int height) = 0;

    // 获取原生窗口句柄 (HWND)，供 Phase 2 Qt 集成使用
    virtual void* getNativeHandle() = 0;

    virtual int getWidth() const = 0;
    virtual int getHeight() const = 0;
};
