#include "ImageWriter.h"

#include <cassert>
#include <gst/app/app.h>

static constexpr GstClockTime MESSAGE_TIMEOUT = 1 * GST_SECOND;

bool ImageWriter::create_pipeline() noexcept
{
    assert(m_pipeline == nullptr);

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(
        "appsrc name=entry-point is-live=true emit-signals=false format=time ! nvvidconv ! nvjpegenc quality=100 ! "
        "multifilesink enable-last-sample=false post-messages=true location=./screenshot_%03d.jpg",
        &error);

    if (pipeline == nullptr)
    {
        if (error != nullptr)
        {
            g_printerr("ERROR: cannot create image writer pipeline (%s)\n", error->message);
            g_error_free(error);
        }
        else
        {
            g_printerr("ERROR: cannot create image writer pipeline (unspecified error)\n");
        }

        return false;
    }

    if (error != nullptr)
    {
        g_printerr("WARNING: fixed issue encountered while creating image writer pipeline (%s)\n", error->message);
        g_error_free(error);
    }

    m_pipeline = GST_PIPELINE(gst_object_ref_sink(pipeline));
    return true;
}

bool ImageWriter::start() noexcept
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
    g_print("Image writer pipeline started\n");
    return true;
}

void ImageWriter::stop() noexcept
{
    if (m_pipeline != nullptr)
    {
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        g_print("Image writer pipeline stopped\n");
    }
}

bool ImageWriter::take_screenshot(const IFrameProducer& producer) noexcept
{
    if (m_pipeline == nullptr)
    {
        return false;
    }

    GstSample* sample = producer.get_last_sample();
    if (sample == nullptr)
    {
        return false;
    }

    GstCaps* sample_caps = gst_sample_get_caps(sample);
    GstBuffer* sample_buffer = gst_sample_get_buffer(sample);
    if ((sample_caps == nullptr) || (sample_buffer == nullptr))
    {
        gst_sample_unref(sample);
        return false;
    }

    GstElement* appsrc = gst_bin_get_by_name(GST_BIN(m_pipeline), "entry-point");
    assert(appsrc != nullptr);

    g_object_set(appsrc, "caps", sample_caps, nullptr);
    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), gst_buffer_ref(sample_buffer));
    gst_sample_unref(sample);
    gst_object_unref(appsrc);

    if (ret != GST_FLOW_OK)
    {
        g_printerr("WARNING: cannot push the raw frame to the image encoder\n");
        return false;
    }

    GstBus* bus = gst_pipeline_get_bus(m_pipeline);
    assert(bus != nullptr);

    for (;;)
    {
        GstMessage* msg = gst_bus_timed_pop_filtered(bus, MESSAGE_TIMEOUT, GST_MESSAGE_ELEMENT);
        if (msg == nullptr)
        {
            gst_object_unref(bus);
            g_printerr("WARNING: cannot write image file (timeout occurred)\n");
            return false;
        }

        const GstStructure* msg_struct = gst_message_get_structure(msg);
        if (g_strcmp0(gst_structure_get_name(msg_struct), "GstMultiFileSink") == 0)
        {
            const gchar* filename = gst_structure_get_string(msg_struct, "filename");
            g_print("Screenshot written to %s\n", filename);

            gst_message_unref(msg);
            gst_object_unref(bus);
            return true;
        }

        gst_message_unref(msg);
    }
}
