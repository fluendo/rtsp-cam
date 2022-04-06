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

  private:
    static GstPadProbeReturn highq_stream_pad_probe(GstPad* pad, GstPadProbeInfo* info, EncodingPipeline* pipeline);
    static GstPadProbeReturn lowq_stream_pad_probe(GstPad* pad, GstPadProbeInfo* info, EncodingPipeline* pipeline);

    bool create_pipeline() noexcept;
    bool register_buffer_probes() noexcept;
    GstPipeline* m_pipeline = nullptr;
};
