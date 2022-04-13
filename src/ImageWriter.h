#pragma once

#include "IFrameProducer.h"

class ImageWriter final
{
  public:
    ImageWriter() = default;
    ImageWriter(ImageWriter&&) = default;
    ImageWriter& operator=(ImageWriter&&) = default;

    ImageWriter(const ImageWriter&) = delete;
    ImageWriter& operator=(const ImageWriter&) = delete;

    ~ImageWriter()
    {
        stop();
    }

    bool start() noexcept;
    void stop() noexcept;

    bool take_screenshot(const IFrameProducer& producer) noexcept;

  private:
    bool create_pipeline() noexcept;

    GstPipeline* m_pipeline = nullptr;
};
