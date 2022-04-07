#pragma once

#include "IStreamConsumer.h"

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

    bool start(IStreamConsumer* stream_consumer) noexcept;
    void stop() noexcept;

  private:
    bool create_pipeline() noexcept;
    bool register_buffer_probes(IStreamConsumer* stream_consumer) noexcept;

    GstPipeline* m_pipeline = nullptr;
};
