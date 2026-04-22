#include <cstdio>
#include <string>
#include "core/Demuxer.h"
#include "core/VideoDecoder.h"
#include "core/AudioDecoder.h"

extern "C" {
    #include <libavutil/avutil.h>
    #include <libavutil/pixdesc.h>
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

static void printFrameInfo(const AVFrame* frame, int index)
{
    const char* pixFmtName = av_get_pix_fmt_name(static_cast<AVPixelFormat>(frame->format));
    char pictType = av_get_picture_type_char(frame->pict_type);

    printf("[Frame %3d] %dx%d %s | pts: %8lld | type: %c%s\n",
        index,
        frame->width, frame->height,
        pixFmtName ? pixFmtName : "unknown",
        frame->pts,
        pictType,
        (frame->flags & AV_FRAME_FLAG_KEY) ? " | key" : "");
}

int main(int argc, char* argv[])
{
    printf("=== YuiStream — Day 3: Decoder Test ===\n\n");

    printf("[FFmpeg] avcodec  : %d.%d.%d\n",
        LIBAVCODEC_VERSION_MAJOR, LIBAVCODEC_VERSION_MINOR, LIBAVCODEC_VERSION_MICRO);
    printf("[FFmpeg] avformat : %d.%d.%d\n",
        LIBAVFORMAT_VERSION_MAJOR, LIBAVFORMAT_VERSION_MINOR, LIBAVFORMAT_VERSION_MICRO);
    printf("[FFmpeg] avutil   : %d.%d.%d\n\n",
        LIBAVUTIL_VERSION_MAJOR, LIBAVUTIL_VERSION_MINOR, LIBAVUTIL_VERSION_MICRO);

    // 媒体源：优先命令行参数
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

    // 解封装
    Demuxer demuxer;
    if (!demuxer.open(url))
    {
        printf("[Error] Failed to open: %s\n", url.c_str());
        return -1;
    }

    const auto& info = demuxer.getStreamInfo();

    // 初始化视频解码器
    VideoDecoder videoDecoder;
    bool hasVideo = false;

    if (info.videoStreamIndex >= 0)
    {
        hasVideo = videoDecoder.init(demuxer.getFormatContext(), info.videoStreamIndex);
        if (hasVideo)
        {
            AVCodecContext* ctx = videoDecoder.getCodecContext();
            const char* pixFmtName = av_get_pix_fmt_name(ctx->pix_fmt);
            printf("[VideoDecoder] Pixel format: %s\n", pixFmtName ? pixFmtName : "unknown");
            printf("[VideoDecoder] Resolution  : %dx%d\n\n", ctx->width, ctx->height);
        }
        else
        {
            printf("[VideoDecoder] Init failed for stream #%d\n", info.videoStreamIndex);
        }
    }

    if (!hasVideo)
    {
        printf("[Error] No video stream available for decode test\n");
        demuxer.close();
        return -1;
    }

    // 解码测试：读取 packet → 送入解码器 → 输出帧信息
    constexpr int maxFrames = 30;
    int framesDecoded = 0;
    int packetsSent = 0;
    int packetsRead = 0;

    printf("[Decode] Decoding first %d video frames...\n\n", maxFrames);

    AVPacket* pkt = av_packet_alloc();
    AVFrame* frame = av_frame_alloc();
    if (!pkt || !frame)
    {
        printf("[Error] Failed to allocate AVPacket/AVFrame\n");
        return -1;
    }

    AVCodecContext* codecCtx = videoDecoder.getCodecContext();

    while (framesDecoded < maxFrames)
    {
        int ret = demuxer.readPacket(pkt);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
            {
                // EOF: 刷新解码器中缓存的帧
                avcodec_send_packet(codecCtx, nullptr);
                while (avcodec_receive_frame(codecCtx, frame) == 0)
                {
                    printFrameInfo(frame, framesDecoded);
                    framesDecoded++;
                    av_frame_unref(frame);
                    if (framesDecoded >= maxFrames)
                    {
                        break;
                    }
                }
            }
            break;
        }

        packetsRead++;

        // 只处理视频流的 packet
        if (pkt->stream_index != info.videoStreamIndex)
        {
            av_packet_unref(pkt);
            continue;
        }

        ret = avcodec_send_packet(codecCtx, pkt);
        av_packet_unref(pkt);

        if (ret < 0)
        {
            char errBuf[256];
            av_strerror(ret, errBuf, sizeof(errBuf));
            printf("[Decode] avcodec_send_packet error: %s\n", errBuf);
            continue;
        }
        packetsSent++;

        // 一个 packet 可能解码出多个 frame (B 帧重排序)
        while (avcodec_receive_frame(codecCtx, frame) == 0)
        {
            printFrameInfo(frame, framesDecoded);
            framesDecoded++;
            av_frame_unref(frame);
            if (framesDecoded >= maxFrames)
            {
                break;
            }
        }
    }

    av_frame_free(&frame);
    av_packet_free(&pkt);

    // 统计摘要
    printf("\n=== Decode Summary ===\n");
    printf("  Packets read    : %d\n", packetsRead);
    printf("  Packets sent    : %d (video)\n", packetsSent);
    printf("  Frames decoded  : %d\n", framesDecoded);
    printf("  Video codec     : %s (software)\n", info.videoCodecName);
    printf("======================\n\n");

    videoDecoder.close();
    demuxer.close();
    printf("Day 3 test complete.\n");

    return 0;
}
