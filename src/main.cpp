#include <cstdio>
#include <cstring>
#include <string>

#include <SDL2/SDL.h>

#include "core/Demuxer.h"
#include "core/VideoDecoder.h"
#include "core/PacketQueue.h"
#include "core/FrameQueue.h"
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
    printf("=== YuiStream — Week 2 Day 2: Network Stream Hardening ===\n\n");

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
    // 1. 解封装器
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

    // ========================
    // 2. 视频解码器
    // ========================
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
    // 3. 线程安全队列
    // ========================
    PacketQueue videoPacketQueue(128);   // Demuxer → VideoDecoder
    FrameQueue videoFrameQueue(8);       // VideoDecoder → VideoRenderer

    printf("[Pipeline] PacketQueue capacity: 128, FrameQueue capacity: 8\n\n");

    // ========================
    // 4. SDL2 窗口 + VideoRenderer
    // ========================
    if (SDL_Init(SDL_INIT_VIDEO) < 0)
    {
        printf("[Error] SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    int videoW = codecCtx->width;
    int videoH = codecCtx->height;
    int winW = videoW > 1280 ? 1280 : videoW;
    int winH = static_cast<int>(winW * (static_cast<double>(videoH) / videoW));

    SDLVideoSurface surface;
    if (!surface.create(winW, winH, "YuiStream — Week 2 Day 2"))
    {
        printf("[Error] Failed to create SDLVideoSurface\n");
        SDL_Quit();
        return -1;
    }

    VideoRenderer renderer;
    if (!renderer.init(&surface))
    {
        printf("[Error] Failed to init VideoRenderer\n");
        surface.destroy();
        SDL_Quit();
        return -1;
    }

    // ========================
    // 5. 启动多线程管线
    // ========================
    // 线程启动顺序：渲染器 → 解码器 → 解封装器 (下游先启动，避免队列堆积)

    // 获取视频流 time_base (渲染线程将来做同步用)
    AVStream* videoStream = demuxer.getFormatContext()->streams[info.videoStreamIndex];
    int tbNum = videoStream->time_base.num;
    int tbDen = videoStream->time_base.den;

    // 5a. 启动渲染线程 (GL 上下文从主线程转移到渲染线程)
    renderer.start(&videoFrameQueue, nullptr, tbNum, tbDen);
    printf("[Pipeline] Render thread started\n");

    // 5b. 启动解码线程
    videoDecoder.start(&videoPacketQueue, &videoFrameQueue);
    printf("[Pipeline] Decode thread started\n");

    // 5c. 启动解封装线程
    demuxer.start(&videoPacketQueue, nullptr);
    printf("[Pipeline] Demux thread started\n");

    printf("\n[Pipeline] === All threads running ===\n");
    printf("[Pipeline] Demuxer → PacketQueue → VideoDecoder → FrameQueue → VideoRenderer\n");
    printf("[Pipeline] Press ESC or close window to stop.\n\n");

    // ========================
    // 6. 主线程：事件循环 + 状态监控
    // ========================
    bool running = true;
    uint32_t lastStatsTick = SDL_GetTicks();

    while (running)
    {
        // 事件处理
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
                    // 窗口 resize 事件 — renderer 在自己的线程中处理
                    // 这里只更新 surface 记录的尺寸
                    surface.resize(event.window.data1, event.window.data2);
                }
                break;
            }
        }

        // 检测管线是否自然结束 (EOF)
        if (!renderer.isRunning() && videoFrameQueue.isClosed())
        {
            printf("[Pipeline] Playback finished (EOF)\n");
            running = false;
        }

        // 状态监控 (每秒更新窗口标题)
        uint32_t now = SDL_GetTicks();
        if (now - lastStatsTick >= 1000)
        {
            const auto& stats = renderer.getStats();
            size_t pktQueueSize = videoPacketQueue.size();
            size_t frameQueueSize = videoFrameQueue.size();

            char title[256];
            snprintf(title, sizeof(title),
                     "YuiStream — %dx%d | %.1f FPS | Rendered: %d | PktQ: %zu | FrmQ: %zu",
                     videoW, videoH,
                     stats.currentFPS,
                     stats.renderedFrames,
                     pktQueueSize,
                     frameQueueSize);
            surface.setTitle(title);

            lastStatsTick = now;
        }

        SDL_Delay(16);  // 主线程 ~60Hz 轮询即可
    }

    // ========================
    // 7. 优雅停止管线
    // ========================
    // 停止顺序：上游先停，通过 close 队列传递停止信号到下游
    printf("\n[Pipeline] Stopping...\n");

    // 停止解封装 (会关闭 videoPacketQueue)
    demuxer.stop();
    printf("[Pipeline] Demuxer stopped\n");

    // 关闭 packet 队列 (唤醒阻塞的解码线程)
    videoPacketQueue.close();

    // 停止解码 (会关闭 videoFrameQueue)
    videoDecoder.stop();
    printf("[Pipeline] VideoDecoder stopped\n");

    // 关闭 frame 队列 (唤醒阻塞的渲染线程)
    videoFrameQueue.close();

    // 停止渲染
    renderer.stop();
    printf("[Pipeline] VideoRenderer stopped\n");

    // 清理队列残余
    videoPacketQueue.flush();
    videoFrameQueue.flush();

    // ========================
    // 8. 清理资源
    // ========================
    const auto& finalStats = renderer.getStats();
    printf("\n=== Pipeline Summary ===\n");
    printf("  Rendered frames: %d\n", finalStats.renderedFrames);
    printf("  Dropped frames : %d\n", finalStats.droppedFrames);
    printf("  Final FPS      : %.1f\n", finalStats.currentFPS);

    // 打印 Demuxer 退出原因
    const char* exitReasonStr = "Unknown";
    switch (demuxer.getExitReason())
    {
    case Demuxer::ExitReason::None:         exitReasonStr = "None"; break;
    case Demuxer::ExitReason::EndOfFile:    exitReasonStr = "EOF"; break;
    case Demuxer::ExitReason::NetworkError: exitReasonStr = "NetworkError"; break;
    case Demuxer::ExitReason::Stopped:      exitReasonStr = "UserStopped"; break;
    }
    printf("  Demuxer exit   : %s\n", exitReasonStr);
    printf("========================\n\n");

    videoDecoder.close();
    demuxer.close();
    surface.destroy();
    SDL_Quit();

    printf("Week 2 Day 2 test complete.\n");
    return 0;
}
