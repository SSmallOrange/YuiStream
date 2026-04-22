// VideoDecoder.h — 视频解码器
#pragma once
#include "Decoder.h"
#include <string>

struct AVBufferRef;

class VideoDecoder : public Decoder
{
public:
    enum class HWAccelStatus
    {
        Disabled,
        Active,
        FallbackToSW
    };

    VideoDecoder();
    ~VideoDecoder();

    // 启用硬件解码，失败则自动回退软解
    bool enableHardwareAccel(const std::string& hwType = "d3d11va");

    HWAccelStatus getHWStatus() const { return m_hwStatus; }

protected:
    void onFrameDecoded(AVFrame* frame) override;

private:
    // 硬解帧从 GPU 拷回 CPU
    AVFrame* transferHWFrame(AVFrame* hwFrame);

    AVBufferRef* m_hwDeviceCtx = nullptr;
    HWAccelStatus m_hwStatus = HWAccelStatus::Disabled;
};
