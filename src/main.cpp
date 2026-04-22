#include <cstdio>
#include <SDL.h>
#include <glad/glad.h>

// FFmpeg headers
extern "C" {
    #include <libavutil/avutil.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

int main(int argc, char* argv[])
{
    printf("=== YuiStream Test ===\n\n");
    printf("[FFmpeg] libavcodec   : %s\n", avcodec_configuration());
    printf("[FFmpeg] avcodec      : %d.%d.%d\n",
        LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO);
    printf("[FFmpeg] avformat     : %d.%d.%d\n",
        LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);
    printf("[FFmpeg] avutil       : %d.%d.%d\n\n",
        LIBAVUTIL_VERSION_MAJOR, LIBAVUTIL_VERSION_MINOR, LIBAVUTIL_VERSION_MICRO);

    // SDL2 初始化
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) 
    {
        printf("[SDL2] Init failed: %s\n", SDL_GetError());
        return -1;
    }
    printf("[SDL2] Initialized successfully\n");

    // 设置 OpenGL 属性
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    // 创建窗口
    SDL_Window* window = SDL_CreateWindow(
        "YuiStream - Day 0 Smoke Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        1280, 720,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE
    );

    if (!window) 
    {
        printf("[SDL2] Window creation failed: %s\n", SDL_GetError());
        SDL_Quit();
        return -1;
    }
    printf("[SDL2] Window created: 1280x720\n");

    // 创建 OpenGL 上下文
    SDL_GLContext glContext = SDL_GL_CreateContext(window);
    if (!glContext) 
    {
        printf("[SDL2] OpenGL context creation failed: %s\n", SDL_GetError());
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    printf("[SDL2] OpenGL context created\n");

    // GLAD 加载 OpenGL 函数
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress)) 
    {
        printf("[GLAD] Failed to load OpenGL functions\n");
        SDL_GL_DeleteContext(glContext);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    printf("[OpenGL] Version  : %s\n", glGetString(GL_VERSION));
    printf("[OpenGL] Renderer : %s\n", glGetString(GL_RENDERER));
    printf("[OpenGL] Vendor   : %s\n\n", glGetString(GL_VENDOR));

    // 关闭 VSync
    SDL_GL_SetSwapInterval(0);

    printf("Smoke Test Passed! Press ESC or close window to exit\n");

    bool running = true;
    SDL_Event event;

    while (running) 
    {
        while (SDL_PollEvent(&event)) 
        {
            switch (event.type) {
                case SDL_QUIT:
                    running = false;
                    break;
                case SDL_KEYDOWN:
                    if (event.key.keysym.sym == SDLK_ESCAPE)
                        running = false;
                    break;
                case SDL_WINDOWEVENT:
                    if (event.window.event == SDL_WINDOWEVENT_RESIZED) 
                    {
                        int w = event.window.data1;
                        int h = event.window.data2;
                        glViewport(0, 0, w, h);
                    }
                    break;
            }
        }

        glClearColor(0.1f, 0.1f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);

        SDL_GL_SwapWindow(window);
    }

    SDL_GL_DeleteContext(glContext);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("YuiStream exited cleanly.\n");
    return 0;
}
