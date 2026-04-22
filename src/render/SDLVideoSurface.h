// SDLVideoSurface.h — Phase 1: 独立 SDL2 窗口 + OpenGL 上下文
// 快速开发用，提供完整的窗口生命周期管理和 GL 上下文绑定
#pragma once
#include "VideoSurface.h"
#include <SDL2/SDL.h>

class SDLVideoSurface : public VideoSurface
{
public:
    SDLVideoSurface() = default;
    ~SDLVideoSurface() override;

    bool create(int width, int height, const char* title) override;
    void destroy() override;

    void makeCurrent() override;
    void releaseCurrent() override;
    void swapBuffers() override;

    void resize(int width, int height) override;
    void* getNativeHandle() override;

    int getWidth() const override { return m_width; }
    int getHeight() const override { return m_height; }

    // SDL 特有：处理窗口事件 (主线程调用)
    SDL_Window* getSDLWindow() const { return m_window; }

    // 设置窗口标题
    void setTitle(const char* title);

private:
    SDL_Window* m_window = nullptr;
    SDL_GLContext m_glContext = nullptr;
    int m_width = 0;
    int m_height = 0;
};
