#include <cstdio>
#include <cstring>
#include <string>

#include <SDL2/SDL.h>

#include "core/Demuxer.h"
#include "core/VideoDecoder.h"
#include "render/SDLVideoSurface.h"
#include "render/VideoRenderer.h"

extern "C" {
    #include <libavutil/avutil.h>
    #include <libavutil/pixdesc.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

int main(int argc, char* argv[])
{
    printf("=== YuiStream — Day 5: SDLVideoSurface + VideoRenderer ===\n\n");

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

    // ========================
    // 1. 解封装 + 视频解码器
    // ========================
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
    printf("[VideoDecoder] %s %dx%d\n\n", pixFmtName ? pixFmtName : "unknown",
           codecCtx->width, codecCtx->height);

    // ========================
    // 2. 预解码一批帧
    // ========================
    constexpr int maxBufferedFrames = 300;
    AVFrame* frameBuffer[maxBufferedFrames];
    int frameCount = 0;

    AVPacket* pkt = av_packet_alloc();
    AVFrame* tmpFrame = av_frame_alloc();

    while (frameCount < maxBufferedFrames)
    {
        int ret = demuxer.readPacket(pkt);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
            {
                // 刷新解码器中缓存的帧
                avcodec_send_packet(codecCtx, nullptr);
                while (frameCount < maxBufferedFrames &&
                       avcodec_receive_frame(codecCtx, tmpFrame) == 0)
                {
                    frameBuffer[frameCount++] = av_frame_clone(tmpFrame);
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
            frameBuffer[frameCount++] = av_frame_clone(tmpFrame);
            av_frame_unref(tmpFrame);
        }
    }

    av_frame_free(&tmpFrame);
    av_packet_free(&pkt);

    if (frameCount == 0)
    {
        printf("[Error] Failed to decode any frames\n");
        videoDecoder.close();
        demuxer.close();
        return -1;
    }

    int videoW = frameBuffer[0]->width;
    int videoH = frameBuffer[0]->height;
    printf("[Decode] Buffered %d frames (%dx%d)\n\n", frameCount, videoW, videoH);

    // ========================
    // 3. 创建 SDLVideoSurface
    // ========================
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("[Error] SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    int winW = videoW > 1280 ? 1280 : videoW;
    int winH = static_cast<int>(winW * (static_cast<double>(videoH) / videoW));

    SDLVideoSurface surface;
    if (!surface.create(winW, winH, "YuiStream — Day 5"))
    {
        printf("[Error] Failed to create SDLVideoSurface\n");
        SDL_Quit();
        return -1;
    }

    // ========================
    // 4. 初始化 VideoRenderer
    // ========================
    VideoRenderer renderer;
    if (!renderer.init(&surface))
    {
        printf("[Error] Failed to init VideoRenderer\n");
        surface.destroy();
        SDL_Quit();
        return -1;
    }

    // ========================
    // 5. 渲染循环
    // ========================
    printf("[Render] Entering render loop (ESC or close window to exit)...\n\n");

    bool running = true;
    int currentFrame = 0;

    // 帧率控制
    double fps = info.fps > 0 ? info.fps : 30.0;
    double frameIntervalMs = 1000.0 / fps;
    uint32_t lastFrameTick = SDL_GetTicks();

    // FPS 统计
    uint32_t lastStatsTick = SDL_GetTicks();
    int framesThisSecond = 0;

    while (running)
    {
        // --- 事件处理 ---
        SDL_Event event;
        while (SDL_PollEvent(&event))
        {
            switch (event.type)
            {
            case SDL_QUIT:
                running = false;
                break;

            case SDL_KEYDOWN:
                if (event.key.keysym.sym == SDLK_ESCAPE)
                {
                    running = false;
                }
                break;

            case SDL_WINDOWEVENT:
                if (event.window.event == SDL_WINDOWEVENT_RESIZED)
                {
                    int w = event.window.data1;
                    int h = event.window.data2;
                    surface.resize(w, h);
                    renderer.setViewport(w, h);
                }
                break;
            }
        }

        // --- 帧率控制 ---
        uint32_t now = SDL_GetTicks();
        double elapsed = static_cast<double>(now - lastFrameTick);
        if (elapsed < frameIntervalMs)
        {
            SDL_Delay(1);
            continue;
        }
        lastFrameTick = now;

        // --- 渲染当前帧 ---
        AVFrame* frame = frameBuffer[currentFrame];
        currentFrame = (currentFrame + 1) % frameCount;

        renderer.renderFrame(frame);
        framesThisSecond++;

        // --- FPS 统计 + 窗口标题 ---
        if (now - lastStatsTick >= 1000)
        {
            double displayFPS = framesThisSecond * 1000.0 / (now - lastStatsTick);
            const auto& stats = renderer.getStats();

            char title[128];
            snprintf(title, sizeof(title),
                     "YuiStream — %dx%d | Frame %d/%d | %.1f FPS | Total: %d",
                     videoW, videoH, currentFrame, frameCount,
                     displayFPS, stats.renderedFrames);
            surface.setTitle(title);

            framesThisSecond = 0;
            lastStatsTick = now;
        }
    }

    // ========================
    // 6. 清理
    // ========================
    printf("\n[Cleanup] Releasing resources...\n");

    for (int i = 0; i < frameCount; i++)
    {
        av_frame_free(&frameBuffer[i]);
    }

    videoDecoder.close();
    demuxer.close();
    surface.destroy();
    SDL_Quit();

    printf("Day 5 test complete.\n");
    return 0;
}
