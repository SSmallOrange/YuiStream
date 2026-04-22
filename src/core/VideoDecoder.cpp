// VideoDecoder.cpp — 视频解码器实现
#include "VideoDecoder.h"
#include <cstdio>

extern "C" {
    #include <libavcodec/avcodec.h>
    #include <libavutil/hwcontext.h>
}

VideoDecoder::VideoDecoder() = default;

VideoDecoder::~VideoDecoder()
{
    if (m_hwDeviceCtx)
    {
        av_buffer_unref(&m_hwDeviceCtx);
        m_hwDeviceCtx = nullptr;
    }
}

bool VideoDecoder::enableHardwareAccel(const std::string& hwType)
{
    // TODO: Week 3 — D3D11VA 硬件解码
    printf("[VideoDecoder] Hardware acceleration not yet implemented\n");
    return false;
}

void VideoDecoder::onFrameDecoded(AVFrame* frame)
{
    // TODO: Week 3 — 硬解帧需要从 GPU 拷回 CPU
}

AVFrame* VideoDecoder::transferHWFrame(AVFrame* hwFrame)
{
    // TODO: Week 3 — av_hwframe_transfer_data
    return nullptr;
}
