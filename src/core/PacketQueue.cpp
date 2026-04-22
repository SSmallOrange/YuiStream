// PacketQueue.cpp — 线程安全有界数据包队列实现
#include "PacketQueue.h"
#include <cstdio>

extern "C" {
    #include <libavcodec/avcodec.h>
}

PacketQueue::PacketQueue(size_t maxSize)
    : m_maxSize(maxSize)
{
}

PacketQueue::~PacketQueue()
{
    flush();
}

bool PacketQueue::push(AVPacket* packet, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // 等待队列不满
    if (timeoutMs < 0)
    {
        // 无限等待
        m_notFull.wait(lock, [this]()
        {
            return m_closed || m_queue.size() < m_maxSize;
        });
    }
    else if (timeoutMs > 0)
    {
        // 超时等待
        bool ok = m_notFull.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]()
        {
            return m_closed || m_queue.size() < m_maxSize;
        });
        if (!ok)
        {
            return false;  // 超时
        }
    }
    else
    {
        // 不等待: 队列满或已关闭直接返回
        if (m_closed || m_queue.size() >= m_maxSize)
        {
            return false;
        }
    }

    if (m_closed)
    {
        return false;
    }

    // clone packet 入队 (队列持有独立副本)
    AVPacket* clone = av_packet_clone(packet);
    if (!clone)
    {
        printf("[PacketQueue] av_packet_clone failed\n");
        return false;
    }

    m_queue.push(clone);
    m_notEmpty.notify_one();
    return true;
}

AVPacket* PacketQueue::pop(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // 等待队列不空
    if (timeoutMs < 0)
    {
        m_notEmpty.wait(lock, [this]()
        {
            return m_closed || !m_queue.empty();
        });
    }
    else if (timeoutMs > 0)
    {
        bool ok = m_notEmpty.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]()
        {
            return m_closed || !m_queue.empty();
        });
        if (!ok)
        {
            return nullptr;  // 超时
        }
    }
    else
    {
        if (m_queue.empty())
        {
            return nullptr;
        }
    }

    if (m_queue.empty())
    {
        return nullptr;  // 队列已关闭且为空
    }

    AVPacket* pkt = m_queue.front();
    m_queue.pop();
    m_notFull.notify_one();
    return pkt;
}

void PacketQueue::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    while (!m_queue.empty())
    {
        AVPacket* pkt = m_queue.front();
        m_queue.pop();
        av_packet_free(&pkt);
    }

    // 唤醒所有等待 push 的线程
    m_notFull.notify_all();
}

void PacketQueue::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_closed = true;
    // 唤醒所有等待者，让它们检测到 closed 状态
    m_notFull.notify_all();
    m_notEmpty.notify_all();
}

void PacketQueue::reopen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_closed = false;
}

size_t PacketQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

bool PacketQueue::isClosed() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_closed;
}
