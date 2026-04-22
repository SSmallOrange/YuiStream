// Decoder.cpp — 解码器基类实现
#include "Decoder.h"
#include <cstdio>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavformat/avformat.h>
}

Decoder::~Decoder()
{
    stop();
    close();
}

bool Decoder::init(AVFormatContext* fmtCtx, int streamIndex)
{
    if (!fmtCtx || streamIndex < 0 || static_cast<unsigned>(streamIndex) >= fmtCtx->nb_streams)
    {
        return false;
    }

    close();

    AVStream* stream = fmtCtx->streams[streamIndex];
    AVCodecParameters* codecpar = stream->codecpar;

    // 查找解码器
    const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
    if (!codec)
    {
        printf("[Decoder] Cannot find decoder for codec_id=%d\n", codecpar->codec_id);
        return false;
    }

    // 创建解码器上下文
    m_codecCtx = avcodec_alloc_context3(codec);
    if (!m_codecCtx)
    {
        printf("[Decoder] Failed to allocate AVCodecContext\n");
        return false;
    }

    // 复制编解码参数到解码器上下文
    int ret = avcodec_parameters_to_context(m_codecCtx, codecpar);
    if (ret < 0)
    {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        printf("[Decoder] avcodec_parameters_to_context failed: %s\n", errBuf);
        close();
        return false;
    }

    // 打开解码器
    ret = avcodec_open2(m_codecCtx, codec, nullptr);
    if (ret < 0)
    {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        printf("[Decoder] avcodec_open2 failed: %s\n", errBuf);
        close();
        return false;
    }

    m_streamIndex = streamIndex;
    printf("[Decoder] Initialized: %s (stream #%d)\n", codec->name, streamIndex);
    return true;
}

void Decoder::close()
{
    if (m_codecCtx)
    {
        avcodec_free_context(&m_codecCtx);
        m_codecCtx = nullptr;
    }
    m_streamIndex = -1;
}

void Decoder::start(PacketQueue* input, FrameQueue* output)
{
    if (m_running.load())
    {
        return;
    }

    m_input = input;
    m_output = output;
    m_running.store(true);
    m_thread = std::thread(&Decoder::decodeLoop, this);
}

void Decoder::stop()
{
    m_running.store(false);
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    m_input = nullptr;
    m_output = nullptr;
}

void Decoder::decodeLoop()
{
    // TODO: Day 6 — 多线程串联时实现
    // AVPacket* pkt = av_packet_alloc();
    // AVFrame* frame = av_frame_alloc();
    // while (m_running) {
    //     AVPacket* pkt = m_input->pop();
    //     if (!pkt) break;
    //     avcodec_send_packet(m_codecCtx, pkt);
    //     av_packet_unref(pkt);
    //     while (avcodec_receive_frame(m_codecCtx, frame) == 0) {
    //         onFrameDecoded(frame);
    //         m_output->push(frame);  // 需要 clone
    //         av_frame_unref(frame);
    //     }
    // }
    // av_packet_free(&pkt);
    // av_frame_free(&frame);
}
