#include <cstdio>
#include <cstring>
#include <string>
#include <memory>

#include <SDL2/SDL.h>
#include <glad/glad.h>

#include "core/Demuxer.h"
#include "core/VideoDecoder.h"
#include "render/GLTexture.h"
#include "render/GLShader.h"

extern "C" {
    #include <libavutil/avutil.h>
    #include <libavutil/pixdesc.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

// 全屏 quad 顶点数据 (NDC 坐标 + 纹理坐标)
// 使用 TRIANGLE_STRIP: 4 顶点画满整个屏幕
// 注意：OpenGL 纹理坐标 Y 轴向上，视频帧 Y 轴向下，所以 TexCoord.y 需要翻转
static float quadVertices[] = {
    // Position     TexCoord (翻转 Y: 上→0, 下→1)
    -1.0f,  1.0f,   0.0f, 0.0f,    // 左上
    -1.0f, -1.0f,   0.0f, 1.0f,    // 左下
     1.0f,  1.0f,   1.0f, 0.0f,    // 右上
     1.0f, -1.0f,   1.0f, 1.0f,    // 右下
};

// 创建全屏 quad 的 VAO/VBO
static void createQuadVAO(uint32_t& vao, uint32_t& vbo)
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);

    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // layout(location = 0) — a_Position (vec2)
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);

    // layout(location = 1) — a_TexCoord (vec2)
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));

    glBindVertexArray(0);
}

int main(int argc, char* argv[])
{
    printf("=== YuiStream — Day 4: GLTexture + GLShader + YUV Rendering Test ===\n\n");

    printf("[FFmpeg] avcodec  : %d.%d.%d\n",
        LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO);
    printf("[FFmpeg] avformat : %d.%d.%d\n",
        LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);
    printf("[FFmpeg] avutil   : %d.%d.%d\n\n",
        LIBAVUTIL_VERSION_MAJOR, LIBAVUTIL_VERSION_MINOR, LIBAVUTIL_VERSION_MICRO);

    // --- 媒体源 ---
    std::string url;
    if (argc > 1)
    {
        url = argv[1];
    }
    else
    {
        url = "http://vjs.zencdn.net/v/oceans.mp4";
        printf("[Info] No input specified, using default: %s\n", url.c_str());
        printf("[Info] Usage: YuiStream.exe <file_or_url>\n\n");
    }

    // --- 解封装 + 解码：获取第一帧 ---
    Demuxer demuxer;
    if (!demuxer.open(url))
    {
        printf("[Error] Failed to open: %s\n", url.c_str());
        return -1;
    }

    const auto& info = demuxer.getStreamInfo();
    if (info.videoStreamIndex < 0)
    {
        printf("[Error] No video stream found\n");
        demuxer.close();
        return -1;
    }

    VideoDecoder videoDecoder;
    if (!videoDecoder.init(demuxer.getFormatContext(), info.videoStreamIndex))
    {
        printf("[Error] VideoDecoder init failed\n");
        demuxer.close();
        return -1;
    }

    AVCodecContext* codecCtx = videoDecoder.getCodecContext();
    const char* pixFmtName = av_get_pix_fmt_name(codecCtx->pix_fmt);
    printf("[VideoDecoder] %s %dx%d\n", pixFmtName ? pixFmtName : "unknown",
           codecCtx->width, codecCtx->height);

    // 解码第一帧用于渲染测试
    AVPacket* pkt = av_packet_alloc();
    AVFrame* firstFrame = av_frame_alloc();
    bool gotFrame = false;

    while (!gotFrame)
    {
        int ret = demuxer.readPacket(pkt);
        if (ret < 0)
        {
            break;
        }

        if (pkt->stream_index != info.videoStreamIndex)
        {
            av_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(codecCtx, pkt);
        av_packet_unref(pkt);
        if (ret < 0)
        {
            continue;
        }

        if (avcodec_receive_frame(codecCtx, firstFrame) == 0)
        {
            gotFrame = true;
        }
    }
    av_packet_free(&pkt);

    if (!gotFrame)
    {
        printf("[Error] Failed to decode first frame\n");
        av_frame_free(&firstFrame);
        videoDecoder.close();
        demuxer.close();
        return -1;
    }

    // 检查像素格式是否为 YUV420P
    if (firstFrame->format != AV_PIX_FMT_YUV420P)
    {
        printf("[Warning] Frame format is %s, expected yuv420p. Rendering may be incorrect.\n",
               av_get_pix_fmt_name(static_cast<AVPixelFormat>(firstFrame->format)));
    }

    int videoW = firstFrame->width;
    int videoH = firstFrame->height;
    printf("[Frame] Decoded first frame: %dx%d, linesize=[%d, %d, %d]\n\n",
           videoW, videoH,
           firstFrame->linesize[0], firstFrame->linesize[1], firstFrame->linesize[2]);

    // --- SDL2 窗口 + OpenGL 上下文 ---
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("[Error] SDL_Init failed: %s\n", SDL_GetError());
        av_frame_free(&firstFrame);
        return -1;
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 4);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 5);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);

    int winW = videoW > 1280 ? 1280 : videoW;
    int winH = static_cast<int>(winW * (static_cast<double>(videoH) / videoW));

    SDL_Window* window = SDL_CreateWindow(
        "YuiStream — Day 4 YUV Render Test",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        winW, winH,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE);

    if (!window)
    {
        printf("[Error] SDL_CreateWindow failed: %s\n", SDL_GetError());
        av_frame_free(&firstFrame);
        SDL_Quit();
        return -1;
    }

    SDL_GLContext glCtx = SDL_GL_CreateContext(window);
    if (!glCtx)
    {
        printf("[Error] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        av_frame_free(&firstFrame);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    // GLAD 加载 GL 函数
    if (!gladLoadGLLoader((GLADloadproc)SDL_GL_GetProcAddress))
    {
        printf("[Error] GLAD loader failed\n");
        av_frame_free(&firstFrame);
        SDL_GL_DeleteContext(glCtx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }

    printf("[OpenGL] Vendor  : %s\n", glGetString(GL_VENDOR));
    printf("[OpenGL] Renderer: %s\n", glGetString(GL_RENDERER));
    printf("[OpenGL] Version : %s\n\n", glGetString(GL_VERSION));

    // --- 加载 YUV Shader ---
    GLShader yuvShader;
    if (!yuvShader.loadFromFile("assets/shaders/yuv420p.vert", "assets/shaders/yuv420p.frag"))
    {
        printf("[Error] Failed to load YUV shader\n");
        av_frame_free(&firstFrame);
        SDL_GL_DeleteContext(glCtx);
        SDL_DestroyWindow(window);
        SDL_Quit();
        return -1;
    }
    printf("[GLShader] YUV shader loaded (program=%u)\n", yuvShader.getRendererID());

    // --- 创建 3 个 R8 纹理 (Y/U/V 平面) ---
    GLTexture texY, texU, texV;
    texY.create(videoW, videoH, GL_R8);
    texU.create(videoW / 2, videoH / 2, GL_R8);
    texV.create(videoW / 2, videoH / 2, GL_R8);
    printf("[GLTexture] Y=%ux%u, U=%ux%u, V=%ux%u (R8)\n\n",
           texY.getWidth(), texY.getHeight(),
           texU.getWidth(), texU.getHeight(),
           texV.getWidth(), texV.getHeight());

    // --- 上传第一帧 YUV 数据到纹理 ---
    texY.update(firstFrame->data[0], firstFrame->linesize[0]);
    texU.update(firstFrame->data[1], firstFrame->linesize[1]);
    texV.update(firstFrame->data[2], firstFrame->linesize[2]);
    printf("[Upload] First frame uploaded to GPU\n");

    // --- 创建全屏 quad ---
    uint32_t quadVAO = 0, quadVBO = 0;
    createQuadVAO(quadVAO, quadVBO);

    // --- 解码更多帧用于连续播放 ---
    constexpr int maxBufferedFrames = 300;
    AVFrame* frameBuffer[maxBufferedFrames];
    int frameCount = 0;

    // 第一帧已有
    frameBuffer[0] = firstFrame;
    firstFrame = nullptr;  // 所有权转移
    frameCount = 1;

    // 继续解码一批帧
    pkt = av_packet_alloc();
    AVFrame* tmpFrame = av_frame_alloc();

    while (frameCount < maxBufferedFrames)
    {
        int ret = demuxer.readPacket(pkt);
        if (ret < 0)
        {
            // EOF
            if (ret == AVERROR_EOF)
            {
                avcodec_send_packet(codecCtx, nullptr);
                while (frameCount < maxBufferedFrames &&
                       avcodec_receive_frame(codecCtx, tmpFrame) == 0)
                {
                    frameBuffer[frameCount] = av_frame_clone(tmpFrame);
                    frameCount++;
                    av_frame_unref(tmpFrame);
                }
            }
            break;
        }

        if (pkt->stream_index != info.videoStreamIndex)
        {
            av_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(codecCtx, pkt);
        av_packet_unref(pkt);
        if (ret < 0)
        {
            continue;
        }

        while (frameCount < maxBufferedFrames &&
               avcodec_receive_frame(codecCtx, tmpFrame) == 0)
        {
            frameBuffer[frameCount] = av_frame_clone(tmpFrame);
            frameCount++;
            av_frame_unref(tmpFrame);
        }
    }

    av_frame_free(&tmpFrame);
    av_packet_free(&pkt);
    printf("[Decode] Buffered %d frames for playback\n\n", frameCount);

    // --- 渲染循环 ---
    printf("[Render] Entering render loop (press ESC or close window to exit)...\n");

    bool running = true;
    int currentFrame = 0;
    uint32_t lastTick = SDL_GetTicks();
    int renderedThisSecond = 0;
    double displayFPS = 0.0;

    // 用帧率计算帧间隔
    double fps = info.fps > 0 ? info.fps : 30.0;
    double frameIntervalMs = 1000.0 / fps;
    uint32_t lastFrameTick = SDL_GetTicks();

    SDL_GL_SetSwapInterval(0);  // 关闭 VSync (低延迟模式)

    while (running)
    {
        // 事件处理
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            if (event.type == SDL_QUIT)
            {
                running = false;
            }
            else if (event.type == SDL_KEYDOWN && event.key.keysym.sym == SDLK_ESCAPE)
            {
                running = false;
            }
            else if (event.type == SDL_WINDOWEVENT &&
                     event.window.event == SDL_WINDOWEVENT_RESIZED)
            {
                glViewport(0, 0, event.window.data1, event.window.data2);
            }
        }

        // 帧率控制：按视频原始帧率播放
        uint32_t now = SDL_GetTicks();
        double elapsed = static_cast<double>(now - lastFrameTick);
        if (elapsed < frameIntervalMs)
        {
            SDL_Delay(1);
            continue;
        }
        lastFrameTick = now;

        // 切换到下一帧 (循环播放)
        AVFrame* frame = frameBuffer[currentFrame];
        currentFrame = (currentFrame + 1) % frameCount;

        // 上传 YUV 数据
        texY.update(frame->data[0], frame->linesize[0]);
        texU.update(frame->data[1], frame->linesize[1]);
        texV.update(frame->data[2], frame->linesize[2]);

        // 渲染
        glClear(GL_COLOR_BUFFER_BIT);

        yuvShader.bind();
        texY.bind(0);
        texU.bind(1);
        texV.bind(2);
        yuvShader.setInt("u_TexY", 0);
        yuvShader.setInt("u_TexU", 1);
        yuvShader.setInt("u_TexV", 2);

        glBindVertexArray(quadVAO);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        SDL_GL_SwapWindow(window);

        // FPS 统计
        renderedThisSecond++;
        if (now - lastTick >= 1000)
        {
            displayFPS = renderedThisSecond * 1000.0 / (now - lastTick);
            char title[128];
            snprintf(title, sizeof(title),
                     "YuiStream — Day 4 | %dx%d | Frame %d/%d | %.1f FPS",
                     videoW, videoH, currentFrame, frameCount, displayFPS);
            SDL_SetWindowTitle(window, title);
            renderedThisSecond = 0;
            lastTick = now;
        }
    }

    // --- 清理 ---
    printf("\n[Cleanup] Releasing resources...\n");

    glDeleteVertexArrays(1, &quadVAO);
    glDeleteBuffers(1, &quadVBO);

    for (int i = 0; i < frameCount; i++)
    {
        av_frame_free(&frameBuffer[i]);
    }

    videoDecoder.close();
    demuxer.close();

    SDL_GL_DeleteContext(glCtx);
    SDL_DestroyWindow(window);
    SDL_Quit();

    printf("Day 4 test complete.\n");
    return 0;
}
