// Demuxer.h — 解封装器
// 职责：打开媒体源，读取 AVPacket，分发到视频/音频 PacketQueue
// 支持本地文件和网络流 (RTSP/RTMP/HTTP)，含中断回调和超时控制
#pragma once
#include <string>
#include <thread>
#include <atomic>
#include <chrono>
#include <functional>

struct AVFormatContext;
struct AVPacket;
class PacketQueue;

class Demuxer
{
public:
    // 解封装循环退出原因
    enum class ExitReason
    {
        None,           // 尚未退出
        EndOfFile,      // 文件播放完毕 (正常结束)
        NetworkError,   // 网络断连/超时 (可尝试重连)
        Stopped         // 外部调用 stop() 主动停止
    };

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
        bool isLive = false;  // 直播流标记 (无 duration 或网络流)
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

    // 网络流判断
    bool isNetworkStream() const;

    const StreamInfo& getStreamInfo() const { return m_streamInfo; }
    AVFormatContext* getFormatContext() const { return m_formatCtx; }
    const std::string& getUrl() const { return m_url; }

    // demuxLoop 退出原因 (供 PlayerCore 判断是否需要重连)
    ExitReason getExitReason() const { return m_exitReason; }

private:
    void demuxLoop();  // 线程主循环

    // FFmpeg 中断回调 (静态 → 转发到实例方法)
    static int interruptCallback(void* opaque);
    bool shouldInterrupt() const;
    void resetTimeout();

    AVFormatContext* m_formatCtx = nullptr;
    StreamInfo m_streamInfo;

    PacketQueue* m_videoQueue = nullptr;
    PacketQueue* m_audioQueue = nullptr;

    std::thread m_thread;
    std::atomic<bool> m_running{false};

    // 中断回调超时控制
    std::chrono::steady_clock::time_point m_lastActiveTime;
    static constexpr int kNetworkTimeoutMs = 5000;  // 5 秒无数据视为断连

    std::string m_url;  // 保存当前 URL (重连/日志用)
    ExitReason m_exitReason = ExitReason::None;
};
