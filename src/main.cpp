#include <cstdio>
#include <string>
#include <SDL.h>
#include <glad/glad.h>
#include "core/Demuxer.h"

// FFmpeg headers
extern "C" {
    #include <libavutil/avutil.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

// 打印 AVPacket 的简要信息
static void printPacketInfo(const AVPacket* pkt, const Demuxer::StreamInfo& info)
{
    const char* type = "???";
    if (pkt->stream_index == info.videoStreamIndex)
    {
        type = "VIDEO";
    }
    else if (pkt->stream_index == info.audioStreamIndex)
    {
        type = "AUDIO";
    }
    else
    {
        type = "OTHER";
    }

    bool isKeyFrame = (pkt->flags & AV_PKT_FLAG_KEY) != 0;

    printf("[Packet] stream #%d %-5s | size: %6d bytes | pts: %8lld | dts: %8lld | duration: %5lld%s\n",
        pkt->stream_index,
        type,
        pkt->size,
        pkt->pts,
        pkt->dts,
        pkt->duration,
        isKeyFrame ? " [KEY]" : "");
}

int main(int argc, char* argv[])
{
    printf("=== YuiStream — Day 2: Demuxer Test ===\n\n");

    // --- 环境信息 ---
    printf("[FFmpeg] avcodec  : %d.%d.%d\n",
        LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO);
    printf("[FFmpeg] avformat : %d.%d.%d\n",
        LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);
    printf("[FFmpeg] avutil   : %d.%d.%d\n\n",
        LIBAVUTIL_VERSION_MAJOR, LIBAVUTIL_VERSION_MINOR, LIBAVUTIL_VERSION_MICRO);

    // --- 确定媒体源 ---
    // 优先使用命令行参数，否则使用默认测试文件
    std::string url;
    if (argc > 1)
    {
        url = argv[1];
    }
    else
    {
        // 默认测试文件
        url = "http://vjs.zencdn.net/v/oceans.mp4";
        printf("[Info] No input specified, using default: %s\n", url.c_str());
        printf("[Info] Usage: YuiStream.exe <file_or_url>\n\n");
    }

    // --- Demuxer 测试 ---
    Demuxer demuxer;

    if (!demuxer.open(url))
    {
        printf("[Error] Failed to open: %s\n", url.c_str());
        return -1;
    }

    const auto& info = demuxer.getStreamInfo();

    // --- 读取并打印前 N 个 packet ---
    constexpr int maxPackets = 50;
    int videoPackets = 0;
    int audioPackets = 0;
    int otherPackets = 0;
    int totalPackets = 0;
    int64_t totalBytes = 0;

    printf("\n[Demuxer] Reading first %d packets...\n\n", maxPackets);

    AVPacket* pkt = av_packet_alloc();
    if (!pkt)
    {
        printf("[Error] Failed to allocate AVPacket\n");
        return -1;
    }

    while (totalPackets < maxPackets)
    {
        int ret = demuxer.readPacket(pkt);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
            {
                printf("\n[Demuxer] End of file reached\n");
            }
            else
            {
                char errBuf[256];
                av_strerror(ret, errBuf, sizeof(errBuf));
                printf("\n[Demuxer] Read error: %s\n", errBuf);
            }
            break;
        }

        printPacketInfo(pkt, info);

        // 统计
        totalBytes += pkt->size;
        if (pkt->stream_index == info.videoStreamIndex)
        {
            videoPackets++;
        }
        else if (pkt->stream_index == info.audioStreamIndex)
        {
            audioPackets++;
        }
        else
        {
            otherPackets++;
        }
        totalPackets++;

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);

    // --- 统计摘要 ---
    printf("\n=== Packet Summary ===\n");
    printf("  Total  : %d packets, %lld bytes (%.2f KB)\n", totalPackets, totalBytes, (double)totalBytes / 1024.0);
    printf("  Video  : %d packets\n", videoPackets);
    printf("  Audio  : %d packets\n", audioPackets);
    if (otherPackets > 0)
    {
        printf("  Other  : %d packets\n", otherPackets);
    }
    printf("======================\n\n");

    // --- 清理 ---
    demuxer.close();
    printf("[Demuxer] Closed. Day 2 test complete.\n");

    return 0;
}
