// AudioOutput.cpp — SDL2 音频输出实现
#include "AudioOutput.h"
#include "FrameQueue.h"
#include <cstdio>
#include <cstring>
#include <algorithm>

#include <SDL2/SDL.h>

extern "C" {
    #include <libavutil/frame.h>
    #include <libavutil/opt.h>
    #include <libavutil/channel_layout.h>
    #include <libswresample/swresample.h>
}

// --- 构造 / 析构 ---

AudioOutput::AudioOutput()
{
    m_audioBuf = new uint8_t[kMaxAudioBufSize];
}

AudioOutput::~AudioOutput()
{
    stop();
    close();
    delete[] m_audioBuf;
    m_audioBuf = nullptr;
}

// --- 初始化 SDL2 音频设备 ---

bool AudioOutput::init(int sampleRate, int channels, Clock* clock)
{
    close();

    m_clock = clock;

    // SDL2 音频设备参数
    SDL_AudioSpec wanted = {};
    wanted.freq = sampleRate;
    wanted.format = AUDIO_S16SYS;   // 16-bit signed，系统字节序
    wanted.channels = static_cast<uint8_t>(channels);
    wanted.samples = 1024;           // 缓冲区采样数 (低延迟)
    wanted.callback = AudioOutput::audioCallback;
    wanted.userdata = this;

    SDL_AudioSpec obtained = {};
    m_deviceId = SDL_OpenAudioDevice(nullptr, 0, &wanted, &obtained, 0);
    if (m_deviceId == 0)
    {
        printf("[AudioOutput] SDL_OpenAudioDevice failed: %s\n", SDL_GetError());
        return false;
    }

    m_sampleRate = obtained.freq;
    m_channels = obtained.channels;

    // 清空内部缓冲
    m_audioBufSize = 0;
    m_audioBufIndex = 0;

    printf("[AudioOutput] Initialized: %d Hz, %d channels, buffer=%d samples\n",
           m_sampleRate, m_channels, obtained.samples);
    return true;
}

void AudioOutput::close()
{
    stop();

    if (m_deviceId > 0)
    {
        SDL_CloseAudioDevice(m_deviceId);
        m_deviceId = 0;
    }

    if (m_swrCtx)
    {
        swr_free(&m_swrCtx);
        m_swrCtx = nullptr;
    }

    m_audioBufSize = 0;
    m_audioBufIndex = 0;
}

// --- 开始/停止 ---

void AudioOutput::start(FrameQueue* frameQueue)
{
    if (m_started || m_deviceId == 0)
    {
        return;
    }

    m_frameQueue = frameQueue;
    m_started = true;

    // 取消暂停，开始播放 (SDL2 设备默认是暂停状态)
    SDL_PauseAudioDevice(m_deviceId, 0);
    printf("[AudioOutput] Playback started\n");
}

void AudioOutput::stop()
{
    if (!m_started)
    {
        return;
    }

    // 暂停音频设备 (停止回调触发)
    if (m_deviceId > 0)
    {
        SDL_PauseAudioDevice(m_deviceId, 1);
    }

    m_started = false;
    m_frameQueue = nullptr;
    printf("[AudioOutput] Playback stopped\n");
}

void AudioOutput::setVolume(float volume)
{
    m_volume = std::clamp(volume, 0.0f, 1.0f);
}

// --- SDL2 音频回调 ---

// 由 SDL2 内部音频线程调用 (声卡硬件中断驱动)
// stream: 输出缓冲区指针
// len: 需要填充的字节数
void AudioOutput::audioCallback(void* userdata, uint8_t* stream, int len)
{
    AudioOutput* self = static_cast<AudioOutput*>(userdata);
    self->fillAudioBuffer(stream, len);
}

void AudioOutput::fillAudioBuffer(uint8_t* stream, int len)
{
    // 先清零 (静音兜底，防止噪音)
    memset(stream, 0, len);

    if (!m_frameQueue)
    {
        return;
    }

    int written = 0;
    while (written < len)
    {
        // 如果内部缓冲有剩余数据，优先消费
        if (m_audioBufIndex < m_audioBufSize)
        {
            int remaining = m_audioBufSize - m_audioBufIndex;
            int toCopy = std::min(remaining, len - written);

            // 混合音量
            SDL_MixAudioFormat(stream + written, m_audioBuf + m_audioBufIndex,
                               AUDIO_S16SYS, toCopy,
                               static_cast<int>(m_volume * SDL_MIX_MAXVOLUME));

            m_audioBufIndex += toCopy;
            written += toCopy;
            continue;
        }

        // 内部缓冲耗尽，从 FrameQueue 取新帧进行重采样
        int resampled = decodeAndResample();
        if (resampled <= 0)
        {
            break;  // 无数据可用，剩余部分保持静音
        }
    }
}

// --- 重采样 ---

int AudioOutput::decodeAndResample()
{
    // 非阻塞取帧 (回调线程中不能阻塞)
    AVFrame* frame = m_frameQueue->pop(0);
    if (!frame)
    {
        return 0;
    }

    // 延迟初始化重采样上下文 (首帧到达时)
    if (!m_swrCtx)
    {
        if (!initResampler(frame))
        {
            av_frame_free(&frame);
            return 0;
        }
    }

    // 计算重采样输出采样数
    int outSamples = swr_get_out_samples(m_swrCtx, frame->nb_samples);
    if (outSamples <= 0)
    {
        av_frame_free(&frame);
        return 0;
    }

    // 计算输出缓冲需要的字节数：采样数 × 通道数 × 2 (S16 = 2 bytes/sample)
    int bytesPerSample = 2 * m_channels;
    int maxOutBytes = outSamples * bytesPerSample;

    // 防止溢出
    if (maxOutBytes > kMaxAudioBufSize)
    {
        maxOutBytes = kMaxAudioBufSize;
        outSamples = maxOutBytes / bytesPerSample;
    }

    // 执行重采样
    uint8_t* outBuf = m_audioBuf;
    int converted = swr_convert(m_swrCtx,
                                &outBuf, outSamples,
                                (const uint8_t**)frame->data, frame->nb_samples);

    // TODO: Day 4 — 更新 Clock PTS
    // if (m_clock && frame->pts != AV_NOPTS_VALUE)
    // {
    //     double pts = frame->pts * av_q2d(m_timeBase);
    //     m_clock->set(pts);
    // }

    av_frame_free(&frame);

    if (converted <= 0)
    {
        return 0;
    }

    m_audioBufSize = converted * bytesPerSample;
    m_audioBufIndex = 0;
    return m_audioBufSize;
}

bool AudioOutput::initResampler(const AVFrame* frame)
{
    // 使用新版 channel layout API
    AVChannelLayout outLayout = {};
    av_channel_layout_default(&outLayout, m_channels);

    int ret = swr_alloc_set_opts2(&m_swrCtx,
        &outLayout,             AV_SAMPLE_FMT_S16, m_sampleRate,   // 输出: SDL 需要的格式
        &frame->ch_layout,      static_cast<AVSampleFormat>(frame->format), frame->sample_rate,  // 输入: 解码帧的实际格式
        0, nullptr);

    av_channel_layout_uninit(&outLayout);

    if (ret < 0 || !m_swrCtx)
    {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        printf("[AudioOutput] swr_alloc_set_opts2 failed: %s\n", errBuf);
        return false;
    }

    ret = swr_init(m_swrCtx);
    if (ret < 0)
    {
        char errBuf[256];
        av_strerror(ret, errBuf, sizeof(errBuf));
        printf("[AudioOutput] swr_init failed: %s\n", errBuf);
        swr_free(&m_swrCtx);
        return false;
    }

    printf("[AudioOutput] Resampler initialized: fmt=%d rate=%d ch=%d → S16 %dHz %dch\n",
           frame->format, frame->sample_rate, frame->ch_layout.nb_channels,
           m_sampleRate, m_channels);
    return true;
}
