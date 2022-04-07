#pragma once

#include <gst/gst.h>

class IStreamConsumer
{
  public:
    IStreamConsumer() = default;
    IStreamConsumer(const IStreamConsumer&) = default;
    IStreamConsumer(IStreamConsumer&&) = default;
    IStreamConsumer& operator=(const IStreamConsumer&) = default;
    IStreamConsumer& operator=(IStreamConsumer&&) = default;

    virtual ~IStreamConsumer() = default;

    virtual bool push_buffer(unsigned int stream_idx, GstBuffer* buffer) noexcept = 0;
};
