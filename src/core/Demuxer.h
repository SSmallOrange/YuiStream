// Demuxer.h — 解封装器
// 职责：打开媒体源，读取 AVPacket，分发到视频/音频 PacketQueue
#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <functional>

struct AVFormatContext;
struct AVPacket;
class PacketQueue;

class Demuxer
{
public:
    struct StreamInfo
    {
        int videoStreamIndex = -1;
        int audioStreamIndex = -1;
        int width = 0, height = 0;
        double fps = 0;
        int sampleRate = 0;
        int channels = 0;
        const char* videoCodecName = "";
        const char* audioCodecName = "";
    };

    Demuxer();
    ~Demuxer();

    // 打开媒体源 (本地文件或网络 URL)
    // 网络流设置：低延迟参数、超时、传输协议
    bool open(const std::string& url);

    // 关闭媒体源 (释放 AVFormatContext)
    void close();

    // 开始解封装线程
    void start(PacketQueue* videoQueue, PacketQueue* audioQueue);

    // 停止
    void stop();

    // 读取一个 AVPacket (手动模式)
    // 返回 0 成功, <0 EOF 或错误
    int readPacket(AVPacket* packet);

    const StreamInfo& getStreamInfo() const { return m_streamInfo; }
    AVFormatContext* getFormatContext() const { return m_formatCtx; }

private:
    void demuxLoop();  // 线程主循环

    AVFormatContext* m_formatCtx = nullptr;
    StreamInfo m_streamInfo;

    PacketQueue* m_videoQueue = nullptr;
    PacketQueue* m_audioQueue = nullptr;

    std::thread m_thread;
    std::atomic<bool> m_running{false};
};
