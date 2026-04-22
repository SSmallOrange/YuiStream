// FrameQueue.h — 线程安全有界帧队列
// 职责：Decoder 线程 → Renderer/AudioOutput 之间的 AVFrame 传递缓冲
// 容量设为 4~8 帧 (1080p YUV420 一帧 ≈ 3MB，小容量控制延迟和内存)
#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cstddef>

struct AVFrame;

class FrameQueue
{
public:
    explicit FrameQueue(size_t maxSize = 8);
    ~FrameQueue();

    // 禁止拷贝
    FrameQueue(const FrameQueue&) = delete;
    FrameQueue& operator=(const FrameQueue&) = delete;

    // 生产者调用 (Decoder 线程)
    // frame 会被 clone，调用者可安全 unref 原始帧
    bool push(AVFrame* frame, int timeoutMs = -1);

    // 消费者调用 (Renderer/AudioOutput)
    // 返回 nullptr 表示队列已关闭或超时
    // 调用者负责 av_frame_free 返回的帧
    AVFrame* pop(int timeoutMs = -1);

    // 查看队首但不取出 (视频同步用：先看 PTS 再决定取不取)
    // 返回 nullptr 表示队列为空
    // 注意：返回的指针仍归队列所有，不要释放
    AVFrame* peek();

    // 清空队列 (重连/seek 时调用)
    void flush();

    // 关闭/重开
    void close();
    void reopen();

    size_t size() const;
    bool isClosed() const;

private:
    std::queue<AVFrame*> m_queue;
    size_t m_maxSize;
    bool m_closed = false;

    mutable std::mutex m_mutex;
    std::condition_variable m_notFull;
    std::condition_variable m_notEmpty;
};
