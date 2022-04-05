#include "EncodingPipeline.h"

bool EncodingPipeline::create_pipeline() noexcept
{
    (void)m_pipeline;
    // TODO
    return false;
}

bool EncodingPipeline::start() noexcept
{
    if (m_pipeline != nullptr)
    {
        return true;
    }

    if (!create_pipeline())
    {
        return false;
    }

    gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PLAYING);
    return true;
}

void EncodingPipeline::stop() noexcept
{
    if (m_pipeline != nullptr)
    {
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
}
