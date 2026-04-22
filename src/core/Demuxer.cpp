// Demuxer.cpp — 解封装器实现
#include "Demuxer.h"
#include "PacketQueue.h"
#include <cstdio>

extern "C" {
    #include <libavformat/avformat.h>
    #include <libavcodec/avcodec.h>
    #include <libavutil/avutil.h>
}

// --- 构造 / 析构 ---

Demuxer::Demuxer() = default;

Demuxer::~Demuxer()
{
    stop();
    close();
}

// --- 打开媒体源 ---

bool Demuxer::open(const std::string& url)
{
    // 若已打开，先关闭
    close();

    m_formatCtx = avformat_alloc_context();
    if (!m_formatCtx)
    {
        printf("[Demuxer] Failed to allocate AVFormatContext\n");
        return false;
    }

    AVDictionary* opts = nullptr;

    // 低延迟核心参数
    if (url.find("rtsp://") != std::string::npos)
    {
        av_dict_set(&opts, "rtsp_transport", "tcp", 0);  // TCP 更稳定
        av_dict_set(&opts, "stimeout", "3000000", 0);    // 超时 3 秒 (微秒)
    }
    else if (url.find("rtmp://") != std::string::npos)
    {
        av_dict_set(&opts, "live", "1", 0);              // 标记为直播流
        av_dict_set(&opts, "timeout", "3000000", 0);     // 超时 3 秒 (微秒)
    }
    // 通用低延迟参数
    av_dict_set(&opts, "fflags", "nobuffer", 0);           // 不缓冲
    av_dict_set(&opts, "analyzeduration", "500000", 0);    // 缩短分析时间
    av_dict_set(&opts, "probesize", "32768", 0);           // 缩小探测大小

    printf("[Demuxer] Opening: %s\n", url.c_str());

    int ret = avformat_open_input(&m_formatCtx, url.c_str(), nullptr, &opts);
    av_dict_free(&opts);

    if (ret < 0)
    {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        printf("[Demuxer] avformat_open_input failed: %s\n", errBuf);
        m_formatCtx = nullptr;  // avformat_open_input 失败时会自动释放
        return false;
    }

    // 查找流信息
    ret = avformat_find_stream_info(m_formatCtx, nullptr);
    if (ret < 0)
    {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        printf("[Demuxer] avformat_find_stream_info failed: %s\n", errBuf);
        close();
        return false;
    }

    // 填充 StreamInfo
    m_streamInfo = {};

    for (unsigned int i = 0; i < m_formatCtx->nb_streams; ++i)
    {
        AVStream* stream = m_formatCtx->streams[i];
        AVCodecParameters* codecpar = stream->codecpar;

        if (codecpar->codec_type == AVMEDIA_TYPE_VIDEO && m_streamInfo.videoStreamIndex < 0)
        {
            m_streamInfo.videoStreamIndex = static_cast<int>(i);
            m_streamInfo.width  = codecpar->width;
            m_streamInfo.height = codecpar->height;

            // 计算帧率
            if (stream->avg_frame_rate.den > 0 && stream->avg_frame_rate.num > 0)
            {
                m_streamInfo.fps = av_q2d(stream->avg_frame_rate);
            }
            else if (stream->r_frame_rate.den > 0 && stream->r_frame_rate.num > 0)
            {
                m_streamInfo.fps = av_q2d(stream->r_frame_rate);
            }

            const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
            m_streamInfo.videoCodecName = codec ? codec->name : "unknown";
        }
        else if (codecpar->codec_type == AVMEDIA_TYPE_AUDIO && m_streamInfo.audioStreamIndex < 0)
        {
            m_streamInfo.audioStreamIndex = static_cast<int>(i);
            m_streamInfo.sampleRate = codecpar->sample_rate;
            m_streamInfo.channels  = codecpar->ch_layout.nb_channels;

            const AVCodec* codec = avcodec_find_decoder(codecpar->codec_id);
            m_streamInfo.audioCodecName = codec ? codec->name : "unknown";
        }
    }

    // 打印流信息摘要
    printf("[Demuxer] Opened successfully\n");
    printf("[Demuxer] Format   : %s (%s)\n", m_formatCtx->iformat->name, m_formatCtx->iformat->long_name);
    printf("[Demuxer] Duration : %.2f seconds\n",
        m_formatCtx->duration > 0 ? (double)m_formatCtx->duration / AV_TIME_BASE : 0.0);
    printf("[Demuxer] Streams  : %u\n", m_formatCtx->nb_streams);

    if (m_streamInfo.videoStreamIndex >= 0)
    {
        printf("[Demuxer] Video    : stream #%d, %s, %dx%d, %.2f fps\n",
            m_streamInfo.videoStreamIndex,
            m_streamInfo.videoCodecName,
            m_streamInfo.width, m_streamInfo.height,
            m_streamInfo.fps);
    }
    else
    {
        printf("[Demuxer] Video    : not found\n");
    }

    if (m_streamInfo.audioStreamIndex >= 0)
    {
        printf("[Demuxer] Audio    : stream #%d, %s, %d Hz, %d channels\n",
            m_streamInfo.audioStreamIndex,
            m_streamInfo.audioCodecName,
            m_streamInfo.sampleRate,
            m_streamInfo.channels);
    }
    else
    {
        printf("[Demuxer] Audio    : not found\n");
    }

    // 打印完整流信息 (类似 ffprobe 的效果)
    printf("\n[Demuxer] --- Full Stream Dump ---\n");
    av_dump_format(m_formatCtx, 0, url.c_str(), 0);
    printf("[Demuxer] --- End of Dump ---\n\n");

    return true;
}

// --- 关闭 ---

void Demuxer::close()
{
    if (m_formatCtx)
    {
        avformat_close_input(&m_formatCtx);
        m_formatCtx = nullptr;
    }
    m_streamInfo = {};
}

// --- 读取一个 AVPacket (手动调用，Day 2 测试用) ---

int Demuxer::readPacket(AVPacket* packet)
{
    if (!m_formatCtx)
    {
        return -1;
    }
    return av_read_frame(m_formatCtx, packet);
}

// --- 线程相关 (Day 6 串联时完善) ---

void Demuxer::start(PacketQueue* videoQueue, PacketQueue* audioQueue)
{
    if (m_running.load())
    {
        return;
    }

    m_videoQueue = videoQueue;
    m_audioQueue = audioQueue;
    m_running.store(true);
    m_thread = std::thread(&Demuxer::demuxLoop, this);
}

void Demuxer::stop()
{
    m_running.store(false);
    if (m_thread.joinable())
    {
        m_thread.join();
    }
    m_videoQueue = nullptr;
    m_audioQueue = nullptr;
}

void Demuxer::demuxLoop()
{
    printf("[Demuxer] Demux thread started\n");

    AVPacket* pkt = av_packet_alloc();
    if (!pkt)
    {
        printf("[Demuxer] Failed to allocate AVPacket\n");
        return;
    }

    while (m_running.load())
    {
        int ret = av_read_frame(m_formatCtx, pkt);
        if (ret < 0)
        {
            if (ret == AVERROR_EOF)
            {
                printf("[Demuxer] EOF reached\n");
            }
            else
            {
                char errBuf[256];
                av_strerror(ret, errBuf, sizeof(errBuf));
                printf("[Demuxer] av_read_frame error: %s\n", errBuf);
            }
            break;
        }

        // 分发到对应的队列
        if (pkt->stream_index == m_streamInfo.videoStreamIndex && m_videoQueue)
        {
            if (!m_videoQueue->push(pkt))
            {
                // 队列已关闭
                av_packet_unref(pkt);
                break;
            }
        }
        else if (pkt->stream_index == m_streamInfo.audioStreamIndex && m_audioQueue)
        {
            if (!m_audioQueue->push(pkt))
            {
                av_packet_unref(pkt);
                break;
            }
        }

        av_packet_unref(pkt);
    }

    av_packet_free(&pkt);

    // 通知下游队列：没有更多数据了
    if (m_videoQueue)
    {
        m_videoQueue->close();
    }
    if (m_audioQueue)
    {
        m_audioQueue->close();
    }

    printf("[Demuxer] Demux thread stopped\n");
}
