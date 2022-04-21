#pragma once

#include "IFrameProducer.h"
#include "IStreamConsumer.h"

class EncodingPipeline final : public IFrameProducer
{
  public:
    EncodingPipeline() = default;
    EncodingPipeline(EncodingPipeline&&) = default;
    EncodingPipeline& operator=(EncodingPipeline&&) = default;

    EncodingPipeline(const EncodingPipeline&) = delete;
    EncodingPipeline& operator=(const EncodingPipeline&) = delete;

    ~EncodingPipeline() override
    {
        stop();
    }

    bool start(IStreamConsumer& encoded_stream_consumer, IStreamConsumer& raw_stream_consumer) noexcept;
    void stop() noexcept;

    GstSample* get_last_sample() const noexcept override;

  private:
    bool create_pipeline() noexcept;
    bool register_buffer_probes(IStreamConsumer& encoded_stream_consumer,
                                IStreamConsumer& raw_stream_consumer) noexcept;

    GstPipeline* m_pipeline = nullptr;
};
