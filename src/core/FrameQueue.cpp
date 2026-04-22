// FrameQueue.cpp — 线程安全有界帧队列实现
#include "FrameQueue.h"
#include <cstdio>

extern "C" {
    #include <libavutil/frame.h>
}

FrameQueue::FrameQueue(size_t maxSize)
    : m_maxSize(maxSize)
{
}

FrameQueue::~FrameQueue()
{
    flush();
}

bool FrameQueue::push(AVFrame* frame, int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);

    // 等待队列不满
    if (timeoutMs < 0)
    {
        m_notFull.wait(lock, [this]()
        {
            return m_closed || m_queue.size() < m_maxSize;
        });
    }
    else if (timeoutMs > 0)
    {
        bool ok = m_notFull.wait_for(lock, std::chrono::milliseconds(timeoutMs), [this]()
        {
            return m_closed || m_queue.size() < m_maxSize;
        });
        if (!ok)
        {
            return false;
        }
    }
    else
    {
        if (m_closed || m_queue.size() >= m_maxSize)
        {
            return false;
        }
    }

    if (m_closed)
    {
        return false;
    }

    // clone frame 入队 (队列持有独立引用)
    AVFrame* clone = av_frame_clone(frame);
    if (!clone)
    {
        printf("[FrameQueue] av_frame_clone failed\n");
        return false;
    }

    m_queue.push(clone);
    m_notEmpty.notify_one();
    return true;
}

AVFrame* FrameQueue::pop(int timeoutMs)
{
    std::unique_lock<std::mutex> lock(m_mutex);

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
            return nullptr;
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
        return nullptr;
    }

    AVFrame* frame = m_queue.front();
    m_queue.pop();
    m_notFull.notify_one();
    return frame;
}

AVFrame* FrameQueue::peek()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    if (m_queue.empty())
    {
        return nullptr;
    }
    return m_queue.front();  // 不取出，调用者不要释放
}

void FrameQueue::flush()
{
    std::lock_guard<std::mutex> lock(m_mutex);

    while (!m_queue.empty())
    {
        AVFrame* frame = m_queue.front();
        m_queue.pop();
        av_frame_free(&frame);
    }

    m_notFull.notify_all();
}

void FrameQueue::close()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_closed = true;
    m_notFull.notify_all();
    m_notEmpty.notify_all();
}

void FrameQueue::reopen()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_closed = false;
}

size_t FrameQueue::size() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_queue.size();
}

bool FrameQueue::isClosed() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_closed;
}
