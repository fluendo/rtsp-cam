#pragma once

#include "IStreamConsumer.h"

class StreamRecorder final : public IStreamConsumer
{
  public:
    StreamRecorder() = default;
    StreamRecorder(StreamRecorder&&) = default;
    StreamRecorder& operator=(StreamRecorder&&) = default;

    StreamRecorder(const StreamRecorder&) = delete;
    StreamRecorder& operator=(const StreamRecorder&) = delete;

    ~StreamRecorder() override
    {
        shut();
    }

    bool init() noexcept;
    void shut() noexcept;

    bool start_recording() noexcept;
    void stop_recording() noexcept;
    bool is_recording() const noexcept;

    bool push_caps(unsigned int stream_idx, GstCaps* caps) noexcept override;
    bool push_buffer(unsigned int stream_idx, GstBuffer* buffer) noexcept override;

  private:
    bool create_pipeline() noexcept;
    void finish_grabbing() noexcept;

    GstPipeline* m_pipeline = nullptr;
    GstElement* m_appsrc = nullptr;
    unsigned int m_video_idx = 0;
};
