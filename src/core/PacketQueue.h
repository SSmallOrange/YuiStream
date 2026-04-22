// PacketQueue.h — 线程安全有界数据包队列
// 职责：Demuxer 线程 → Decoder 线程之间的 AVPacket 传递缓冲
// 有界队列提供背压机制，防止内存无限增长
#pragma once
#include <mutex>
#include <condition_variable>
#include <queue>
#include <cstddef>

struct AVPacket;

class PacketQueue
{
public:
    explicit PacketQueue(size_t maxSize = 128);
    ~PacketQueue();

    // 禁止拷贝
    PacketQueue(const PacketQueue&) = delete;
    PacketQueue& operator=(const PacketQueue&) = delete;

    // 生产者调用 (Demuxer 线程)
    // 队列满时阻塞等待，直到有空间或队列关闭
    // timeoutMs: -1=无限等待, 0=不等待, >0=毫秒超时
    // 返回 false 表示队列已关闭或超时
    bool push(AVPacket* packet, int timeoutMs = -1);

    // 消费者调用 (Decoder 线程)
    // 队列空时阻塞等待，直到有数据或队列关闭
    // 返回 nullptr 表示队列已关闭或超时
    AVPacket* pop(int timeoutMs = -1);

    // 清空队列，释放所有 packet (重连/seek 时调用)
    void flush();

    // 关闭队列，唤醒所有等待线程
    void close();

    // 重新打开 (close 后如需复用)
    void reopen();

    size_t size() const;
    bool isClosed() const;

private:
    std::queue<AVPacket*> m_queue;
    size_t m_maxSize;
    bool m_closed = false;

    mutable std::mutex m_mutex;
    std::condition_variable m_notFull;    // push 等待：队列不满
    std::condition_variable m_notEmpty;   // pop 等待：队列不空
};
