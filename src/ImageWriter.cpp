#include "ImageWriter.h"

#include <cassert>
#include <gst/app/app.h>

static constexpr GstClockTime MESSAGE_TIMEOUT = 1 * GST_SECOND;

bool ImageWriter::create_pipeline() noexcept
{
    assert(m_pipeline == nullptr);

    GError* error = nullptr;
    GstElement* pipeline = gst_parse_launch(
        "appsrc"
            " name=entry-point"
            " is-live=true"
            " emit-signals=false"
            " leaky-type=downstream"
            " max-buffers=1"
            " format=time"
        " ! videoconvert"
        " ! jpegenc"
        " ! queue"
            " max-size-buffers=10"
        //" ! identity"
        //    " sleep-time=5000000"
        " ! multifilesink"
            " enable-last-sample=false"
            " post-messages=true"
            " location=./screenshot_%03d.jpg",
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

gboolean ImageWriter::bus_cb(GstBus *bus, GstMessage *message, gpointer thiz)
{
  const GstStructure* msg_struct = gst_message_get_structure(message);
  if (!msg_struct || (g_strcmp0(gst_structure_get_name(msg_struct), "GstMultiFileSink") != 0))
  {
    return true;
  }

  const gchar* filename = gst_structure_get_string(msg_struct, "filename");
  gchar* absolute_path = g_canonicalize_filename(filename, nullptr);
  g_print("Screenshot written to %s\n", absolute_path);
  g_free(absolute_path);

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

    GstBus* bus = gst_pipeline_get_bus(m_pipeline);
    assert(bus != nullptr);
    gst_bus_add_watch(bus, bus_cb, this);
    gst_object_unref(bus);

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
    g_print("taking screenshot...\n");
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

    return true;
}
