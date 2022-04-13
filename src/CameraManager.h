#pragma once

#include "EncodingPipeline.h"
#include "ImageWriter.h"
#include "StreamingServer.h"

class CameraManager final
{
  public:
    bool configure(const char* port = nullptr) noexcept;
    bool start() noexcept;
    void stop() noexcept;

    bool take_screenshot() noexcept;

  private:
    StreamingServer m_streaming_server;
    EncodingPipeline m_encoding_pipeline;
    ImageWriter m_img_writer;
};
