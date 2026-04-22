// Decoder.h — 解码器基类
// 职责：从 PacketQueue 读取 AVPacket，解码为 AVFrame，输出到 FrameQueue
#pragma once
#include <thread>
#include <atomic>

struct AVCodecContext;
struct AVFormatContext;
struct AVFrame;
class PacketQueue;
class FrameQueue;

class Decoder
{
public:
    virtual ~Decoder();

    // 初始化解码器 (从 AVFormatContext 对应流的 codec 参数创建解码器上下文)
    bool init(AVFormatContext* fmtCtx, int streamIndex);

    void close();

    // 启动/停止解码线程
    void start(PacketQueue* input, FrameQueue* output);
    void stop();

    AVCodecContext* getCodecContext() const { return m_codecCtx; }
    int getStreamIndex() const { return m_streamIndex; }

protected:
    // 子类 hook：帧解码后处理 (如硬解帧转换)
    virtual void onFrameDecoded(AVFrame* frame) {}

    AVCodecContext* m_codecCtx = nullptr;
    int m_streamIndex = -1;

private:
    void decodeLoop();

    PacketQueue* m_input = nullptr;
    FrameQueue* m_output = nullptr;
    std::thread m_thread;
    std::atomic<bool> m_running{false};
};
