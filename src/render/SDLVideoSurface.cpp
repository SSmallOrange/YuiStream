// SDLVideoSurface.cpp — SDL2 窗口 + OpenGL 上下文实现
#include "SDLVideoSurface.h"
#include <glad/glad.h>
#include <cstdio>

#ifdef _WIN32
#include <SDL2/SDL_syswm.h>
#endif

SDLVideoSurface::~SDLVideoSurface()
{
    destroy();
}

bool SDLVideoSurface::create(int width, int height, const char* title)
{
    // 确保 SDL 视频子系统已初始化
    if (!(SDL_WasInit(SDL_INIT_VIDEO) & SDL_INIT_VIDEO))
    {
        if (SDL_InitSubSystem(SDL_INIT_VIDEO) < 0)
        {
            printf("[SDLVideoSurface] SDL_InitSubSystem(VIDEO) failed: %s\n", SDL_GetError());
            return false;
        }
    }

    // OpenGL 4.5 Core Profile
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);

    m_window = SDL_CreateWindow(
        title,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        width, height,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!m_window)
    {
        printf("[SDLVideoSurface] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return false;
    }

    m_glContext = SDL_GL_CreateContext(m_window);
    if (!m_glContext)
    {
        printf("[SDLVideoSurface] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
        return false;
    }

    // GLAD 加载 GL 函数指针
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        printf("[SDLVideoSurface] GLAD loader failed\n");
        destroy();
        return false;
    }

    // 关闭 VSync (低延迟模式)
    SDL_GL_SetSwapInterval(0);

    m_width = width;
    m_height = height;

    printf("[SDLVideoSurface] Created %dx%d window\n", width, height);
    printf("[SDLVideoSurface] GL Vendor  : %s\n", glGetString(GL_VENDOR));
    printf("[SDLVideoSurface] GL Renderer: %s\n", glGetString(GL_RENDERER));
    printf("[SDLVideoSurface] GL Version : %s\n", glGetString(GL_VERSION));

    return true;
}

void SDLVideoSurface::destroy()
{
    if (m_glContext)
    {
        SDL_GL_DeleteContext(m_glContext);
        m_glContext = nullptr;
    }
    if (m_window)
    {
        SDL_DestroyWindow(m_window);
        m_window = nullptr;
    }
    m_width = 0;
    m_height = 0;
}

void SDLVideoSurface::makeCurrent()
{
    if (m_window && m_glContext)
    {
        SDL_GL_MakeCurrent(m_window, m_glContext);
    }
}

void SDLVideoSurface::releaseCurrent()
{
    SDL_GL_MakeCurrent(nullptr, nullptr);
}

void SDLVideoSurface::swapBuffers()
{
    if (m_window)
    {
        SDL_GL_SwapWindow(m_window);
    }
}

void SDLVideoSurface::resize(int width, int height)
{
    m_width = width;
    m_height = height;
    // Viewport 由 VideoRenderer 管理
}

void* SDLVideoSurface::getNativeHandle()
{
#ifdef _WIN32
    SDL_SysWMinfo wmInfo;
    SDL_VERSION(&wmInfo.version);
    if (SDL_GetWindowWMInfo(m_window, &wmInfo))
    {
        return wmInfo.info.win.window;  // HWND
    }
#endif
    return nullptr;
}

void SDLVideoSurface::setTitle(const char* title)
{
    if (m_window)
    {
        SDL_SetWindowTitle(m_window, title);
    }
}
