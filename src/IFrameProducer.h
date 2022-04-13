#pragma once

#include <gst/gst.h>

class IFrameProducer
{
  public:
    IFrameProducer() = default;
    IFrameProducer(const IFrameProducer&) = default;
    IFrameProducer(IFrameProducer&&) = default;
    IFrameProducer& operator=(const IFrameProducer&) = default;
    IFrameProducer& operator=(IFrameProducer&&) = default;

    virtual ~IFrameProducer() = default;

    virtual GstSample* get_last_sample() const noexcept = 0;
};
