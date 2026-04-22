#include <cstdio>
#include <cstring>
#include <string>

#include <SDL2/SDL.h>

#include "core/Demuxer.h"
#include "core/VideoDecoder.h"
#include "core/AudioDecoder.h"
#include "core/AudioOutput.h"
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
    printf("=== YuiStream — Week 2 Day 3: Audio Pipeline ===\n\n");

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
    // 3. 音频解码器 (有音频流时启用)
    // ========================
    AudioDecoder audioDecoder;
    bool hasAudio = false;

    if (info.audioStreamIndex >= 0)
    {
        if (audioDecoder.init(demuxer.getFormatContext(), info.audioStreamIndex))
        {
            hasAudio = true;
            printf("[AudioDecoder] Initialized (stream #%d)\n\n", info.audioStreamIndex);
        }
        else
        {
            printf("[Warning] AudioDecoder init failed, playing video only\n\n");
        }
    }
    else
    {
        printf("[Info] No audio stream found, playing video only\n\n");
    }

    // ========================
    // 4. 线程安全队列
    // ========================
    PacketQueue videoPacketQueue(128);   // Demuxer → VideoDecoder
    FrameQueue videoFrameQueue(8);       // VideoDecoder → VideoRenderer

    PacketQueue audioPacketQueue(64);    // Demuxer → AudioDecoder
    FrameQueue audioFrameQueue(32);      // AudioDecoder → AudioOutput (音频帧小，队列大些)

    printf("[Pipeline] VideoPacketQueue: 128, VideoFrameQueue: 8\n");
    if (hasAudio)
    {
        printf("[Pipeline] AudioPacketQueue: 64, AudioFrameQueue: 32\n");
    }
    printf("\n");

    // ========================
    // 5. SDL2 初始化 + 窗口 + 渲染器
    // ========================
    uint32_t sdlFlags = SDL_INIT_VIDEO;
    if (hasAudio)
    {
        sdlFlags |= SDL_INIT_AUDIO;
    }

    if (SDL_Init(sdlFlags) < 0)
    {
        printf("[Error] SDL_Init failed: %s\n", SDL_GetError());
        return -1;
    }

    int videoW = codecCtx->width;
    int videoH = codecCtx->height;
    int winW = videoW > 1280 ? 1280 : videoW;
    int winH = static_cast<int>(winW * (static_cast<double>(videoH) / videoW));

    SDLVideoSurface surface;
    if (!surface.create(winW, winH, "YuiStream — Week 2 Day 3"))
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
    // 6. 音频输出 (有音频流时创建)
    // ========================
    AudioOutput audioOutput;
    if (hasAudio)
    {
        if (!audioOutput.init(info.sampleRate, info.channels, nullptr))
        {
            printf("[Warning] AudioOutput init failed, playing video only\n");
            hasAudio = false;
        }
    }

    // ========================
    // 7. 启动多线程管线
    // ========================
    // 启动顺序：下游先启动 → 上游后启动 (避免队列堆积)

    // 获取视频流 time_base
    AVStream* videoStream = demuxer.getFormatContext()->streams[info.videoStreamIndex];
    int tbNum = videoStream->time_base.num;
    int tbDen = videoStream->time_base.den;

    // 7a. 启动音频输出 (被动拉取，声卡驱动回调)
    if (hasAudio)
    {
        audioOutput.start(&audioFrameQueue);
        printf("[Pipeline] Audio output started\n");
    }

    // 7b. 启动渲染线程 (GL 上下文从主线程转移到渲染线程)
    renderer.start(&videoFrameQueue, nullptr, tbNum, tbDen);
    printf("[Pipeline] Render thread started\n");

    // 7c. 启动解码线程
    if (hasAudio)
    {
        audioDecoder.start(&audioPacketQueue, &audioFrameQueue);
        printf("[Pipeline] Audio decode thread started\n");
    }
    videoDecoder.start(&videoPacketQueue, &videoFrameQueue);
    printf("[Pipeline] Video decode thread started\n");

    // 7d. 启动解封装线程
    demuxer.start(&videoPacketQueue, hasAudio ? &audioPacketQueue : nullptr);
    printf("[Pipeline] Demux thread started\n");

    printf("\n[Pipeline] === All threads running ===\n");
    if (hasAudio)
    {
        printf("[Pipeline] Demuxer → PacketQueues → Decoders → FrameQueues → Renderer/AudioOutput\n");
    }
    else
    {
        printf("[Pipeline] Demuxer → PacketQueue → VideoDecoder → FrameQueue → VideoRenderer\n");
    }
    printf("[Pipeline] Press ESC or close window to stop.\n\n");

    // ========================
    // 8. 主线程：事件循环 + 状态监控
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
            if (hasAudio)
            {
                snprintf(title, sizeof(title),
                         "YuiStream — %dx%d | %.1f FPS | Rendered: %d | VPktQ: %zu | VFrmQ: %zu | APktQ: %zu | AFrmQ: %zu",
                         videoW, videoH,
                         stats.currentFPS,
                         stats.renderedFrames,
                         pktQueueSize,
                         frameQueueSize,
                         audioPacketQueue.size(),
                         audioFrameQueue.size());
            }
            else
            {
                snprintf(title, sizeof(title),
                         "YuiStream — %dx%d | %.1f FPS | Rendered: %d | PktQ: %zu | FrmQ: %zu",
                         videoW, videoH,
                         stats.currentFPS,
                         stats.renderedFrames,
                         pktQueueSize,
                         frameQueueSize);
            }
            surface.setTitle(title);

            lastStatsTick = now;
        }

        SDL_Delay(16);  // 主线程 ~60Hz 轮询即可
    }

    // ========================
    // 9. 优雅停止管线
    // ========================
    // 停止顺序：上游先停，通过 close 队列传递停止信号到下游
    printf("\n[Pipeline] Stopping...\n");

    // 停止解封装
    demuxer.stop();
    printf("[Pipeline] Demuxer stopped\n");

    // 关闭 packet 队列
    videoPacketQueue.close();
    audioPacketQueue.close();

    // 停止解码
    videoDecoder.stop();
    printf("[Pipeline] VideoDecoder stopped\n");
    if (hasAudio)
    {
        audioDecoder.stop();
        printf("[Pipeline] AudioDecoder stopped\n");
    }

    // 关闭 frame 队列
    videoFrameQueue.close();
    audioFrameQueue.close();

    // 停止渲染和音频输出
    renderer.stop();
    printf("[Pipeline] VideoRenderer stopped\n");
    if (hasAudio)
    {
        audioOutput.stop();
        printf("[Pipeline] AudioOutput stopped\n");
    }

    // 清理队列残余
    videoPacketQueue.flush();
    videoFrameQueue.flush();
    audioPacketQueue.flush();
    audioFrameQueue.flush();

    // ========================
    // 10. 清理资源
    // ========================
    const auto& finalStats = renderer.getStats();
    printf("\n=== Pipeline Summary ===\n");
    printf("  Rendered frames: %d\n", finalStats.renderedFrames);
    printf("  Dropped frames : %d\n", finalStats.droppedFrames);
    printf("  Final FPS      : %.1f\n", finalStats.currentFPS);
    printf("  Audio          : %s\n", hasAudio ? "enabled" : "disabled");

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
    if (hasAudio)
    {
        audioDecoder.close();
        audioOutput.close();
    }
    demuxer.close();
    surface.destroy();
    SDL_Quit();

    printf("Week 2 Day 3 test complete.\n");
    return 0;
}
