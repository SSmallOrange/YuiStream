// AudioOutput.h — SDL2 音频输出
// 职责：从 FrameQueue 消费音频 AVFrame，重采样为 S16 交错格式，通过 SDL2 回调输出
// SDL2 音频回调由声卡硬件中断驱动 (被动拉取模型)
#pragma once
#include <cstdint>

struct AVFrame;
struct SwrContext;
class FrameQueue;
class Clock;

// SDL2 前向声明 (避免在 header 中 include SDL.h)
using SDL_AudioDeviceID = uint32_t;

class AudioOutput
{
public:
    AudioOutput();
    ~AudioOutput();

    // 禁止拷贝
    AudioOutput(const AudioOutput&) = delete;
    AudioOutput& operator=(const AudioOutput&) = delete;

    // 初始化 SDL2 音频设备
    // clock 可为 nullptr (Day 3 暂不接入，Day 4 实现 Clock 后传入)
    bool init(int sampleRate, int channels, Clock* clock = nullptr);

    // 释放 SDL2 音频设备和重采样上下文
    void close();

    // 开始/停止音频播放
    void start(FrameQueue* frameQueue);
    void stop();

    // 音量控制 (0.0 ~ 1.0)
    void setVolume(float volume);
    float getVolume() const { return m_volume; }

    int getSampleRate() const { return m_sampleRate; }
    int getChannels() const { return m_channels; }

private:
    // SDL2 音频回调 (静态，通过 userdata 转发到实例方法)
    static void audioCallback(void* userdata, uint8_t* stream, int len);

    // 填充 SDL 音频缓冲区 (回调内调用)
    void fillAudioBuffer(uint8_t* stream, int len);

    // 从 FrameQueue 取帧并重采样，追加到内部缓冲
    // 返回重采样后的字节数，0 表示无数据
    int decodeAndResample();

    // 延迟初始化 SwrContext (基于第一帧的实际格式)
    bool initResampler(const AVFrame* frame);

    SDL_AudioDeviceID m_deviceId = 0;
    SwrContext* m_swrCtx = nullptr;
    FrameQueue* m_frameQueue = nullptr;
    Clock* m_clock = nullptr;

    // 内部 PCM 缓冲 (重采样输出暂存)
    // 解码一帧的 PCM 数据量通常大于一次回调请求量，需要缓存剩余部分
    uint8_t* m_audioBuf = nullptr;
    int m_audioBufSize = 0;     // 缓冲中有效数据大小 (字节)
    int m_audioBufIndex = 0;    // 当前已消费位置

    float m_volume = 1.0f;
    int m_sampleRate = 44100;
    int m_channels = 2;
    bool m_started = false;

    // 内部缓冲容量 (足够容纳一帧重采样后的数据)
    static constexpr int kMaxAudioBufSize = 192000;  // ~1s of 48kHz stereo S16
};
