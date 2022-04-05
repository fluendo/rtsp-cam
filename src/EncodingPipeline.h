#pragma once

#include <gst/gst.h>

class EncodingPipeline final
{
  public:
    EncodingPipeline() = default;
    EncodingPipeline(EncodingPipeline&&) = default;
    EncodingPipeline& operator=(EncodingPipeline&&) = default;

    EncodingPipeline(const EncodingPipeline&) = delete;
    EncodingPipeline& operator=(const EncodingPipeline&) = delete;

    ~EncodingPipeline()
    {
        stop();
    }

    bool start() noexcept;
    void stop() noexcept;

    GstPipeline* get_pipeline() noexcept
    {
        return m_pipeline;
    }

  private:
    bool create_pipeline() noexcept;
    GstPipeline* m_pipeline = nullptr;
};
