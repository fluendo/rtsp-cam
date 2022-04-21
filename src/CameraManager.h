#pragma once

#include "EncodingPipeline.h"
#include "ImageWriter.h"
#include "StreamRecorder.h"
#include "StreamingServer.h"

class CameraManager final
{
  public:
    CameraManager() = default;

    CameraManager(CameraManager&&) = delete;
    CameraManager& operator=(CameraManager&&) = delete;
    CameraManager(const CameraManager&) = delete;
    CameraManager& operator=(const CameraManager&) = delete;

    ~CameraManager()
    {
        shut();
    }

    bool init(const char* port = nullptr) noexcept;
    bool run_and_wait() noexcept;
    void shut() noexcept;

    bool start_recording() noexcept;
    void stop_recording() noexcept;
    bool is_recording() const noexcept;

    bool take_screenshot() noexcept;

  private:
    StreamingServer m_streaming_server;
    EncodingPipeline m_encoding_pipeline;
    StreamRecorder m_stream_recorder;
    ImageWriter m_img_writer;
};
