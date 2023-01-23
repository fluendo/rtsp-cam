#include "EncodingPipeline.h"

#include <cassert>

namespace
{
constexpr unsigned int NB_STREAMS = 2;
constexpr char STREAM_IDX_KEY[] = "stream-idx";

GstPadProbeReturn encoded_stream_pad_probe(GstPad* pad, GstPadProbeInfo* info, IStreamConsumer* encoded_stream_consumer)
{
    assert(pad != nullptr);
    assert(info != nullptr);
    assert(encoded_stream_consumer != nullptr);

    if (info->data != nullptr)
    {
        auto stream_idx =
            static_cast<unsigned int>(reinterpret_cast<guintptr>(g_object_get_data(G_OBJECT(pad), STREAM_IDX_KEY)));
        assert(stream_idx < NB_STREAMS);

        if ((info->type & GST_PAD_PROBE_TYPE_BUFFER) == GST_PAD_PROBE_TYPE_BUFFER)
        {
            encoded_stream_consumer->push_buffer(stream_idx, GST_BUFFER(info->data));
        }
        else if ((info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) == GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM)
        {
            GstEvent* event = GST_EVENT(info->data);
            if (event->type == GST_EVENT_CAPS)
            {
                GstCaps* caps = nullptr;
                gst_event_parse_caps(event, &caps);
                if (caps != nullptr)
                {
                    encoded_stream_consumer->push_caps(stream_idx, caps);
                }
            }
        }
    }

    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn raw_stream_pad_probe(GstPad* pad, GstPadProbeInfo* info, IStreamConsumer* raw_stream_consumer)
{
    assert(pad != nullptr);
    assert(info != nullptr);
    assert(raw_stream_consumer != nullptr);

    if (info->data != nullptr)
    {
        if ((info->type & GST_PAD_PROBE_TYPE_BUFFER) == GST_PAD_PROBE_TYPE_BUFFER)
        {
            raw_stream_consumer->push_buffer(0, GST_BUFFER(info->data));
        }
        else if ((info->type & GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM) == GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM)
        {
            GstEvent* event = GST_EVENT(info->data);
            if (event->type == GST_EVENT_CAPS)
            {
                GstCaps* caps = nullptr;
                gst_event_parse_caps(event, &caps);
                if (caps != nullptr)
                {
                    raw_stream_consumer->push_caps(0, caps);
                }
            }
        }
    }

    return GST_PAD_PROBE_OK;
}
} // namespace

bool EncodingPipeline::create_pipeline() noexcept
{
    assert(m_pipeline == nullptr);

    GError* error = nullptr;
    // clang-format off
    GstElement* pipeline = gst_parse_launch(
        "videotestsrc is-live=true pattern=ball ! video/x-raw,width=1920,height=1080 ! timeoverlay ! tee name=raw-img "
        "raw-img. ! queue silent=true ! fakesink name=frame-producer enable-last-sample=true sync=true "
        "raw-img. ! queue silent=true  ! fakesink name=stream0 enable-last-sample=false qos=true sync=true "
        "raw-img. ! queue silent=true  ! fakesink name=stream1 enable-last-sample=false qos=true sync=true",
        &error);
    // clang-format on

    if (pipeline == nullptr)
    {
        if (error != nullptr)
        {
            g_printerr("ERROR: cannot create encoding pipeline (%s)\n", error->message);
            g_error_free(error);
        }
        else
        {
            g_printerr("ERROR: cannot create encoding pipeline (unspecified error)\n");
        }

        return false;
    }

    if (error != nullptr)
    {
        g_printerr("WARNING: fixed issue encountered while creating encoding pipeline (%s)\n", error->message);
        g_error_free(error);
    }

    m_pipeline = GST_PIPELINE(gst_object_ref_sink(pipeline));
    return true;
}

bool EncodingPipeline::register_buffer_probes(IStreamConsumer& encoded_stream_consumer,
                                              IStreamConsumer& raw_stream_consumer) noexcept
{
    assert(m_pipeline != nullptr);

    // Register encoded streams pads probes
    char buff[9]; // until "stream99", just in case // NOLINT
    for (unsigned int i = 0; i < NB_STREAMS; ++i)
    {
        g_snprintf(buff, sizeof(buff), "stream%u", i);

        GstElement* sink = gst_bin_get_by_name(GST_BIN(m_pipeline), buff);
        assert(sink != nullptr);

        GstPad* sink_pad = gst_element_get_static_pad(sink, "sink");
        assert(sink_pad != nullptr);
        g_object_set_data(G_OBJECT(sink_pad), STREAM_IDX_KEY, reinterpret_cast<gpointer>(static_cast<guintptr>(i)));

        gulong probe_id = gst_pad_add_probe(
            sink_pad, static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM),
            reinterpret_cast<GstPadProbeCallback>(encoded_stream_pad_probe), &encoded_stream_consumer, nullptr);

        gst_object_unref(sink_pad);
        gst_object_unref(sink);

        if (probe_id == 0)
        {
            g_printerr("ERROR: cannot register buffer probe for encoded stream #%u\n", i);
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
            return false;
        }
    }

    // Register raw stream pad probe
    GstElement* sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "frame-producer");
    assert(sink != nullptr);

    GstPad* sink_pad = gst_element_get_static_pad(sink, "sink");
    assert(sink_pad != nullptr);
    gulong probe_id = gst_pad_add_probe(
        sink_pad, static_cast<GstPadProbeType>(GST_PAD_PROBE_TYPE_BUFFER | GST_PAD_PROBE_TYPE_EVENT_DOWNSTREAM),
        reinterpret_cast<GstPadProbeCallback>(raw_stream_pad_probe), &raw_stream_consumer, nullptr);

    gst_object_unref(sink_pad);
    gst_object_unref(sink);

    if (probe_id == 0)
    {
        g_printerr("ERROR: cannot register buffer probe for raw stream\n");
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        return false;
    }

    return true;
}

bool EncodingPipeline::start(IStreamConsumer& encoded_stream_consumer, IStreamConsumer& raw_stream_consumer) noexcept
{
    if (m_pipeline != nullptr)
    {
        return true;
    }

    if (!create_pipeline() || !register_buffer_probes(encoded_stream_consumer, raw_stream_consumer))
    {
        return false;
    }

    gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PLAYING);
    g_print("Encoding pipeline started\n");
    return true;
}

void EncodingPipeline::stop() noexcept
{
    if (m_pipeline != nullptr)
    {
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
        g_print("Encoding pipeline stopped\n");
    }
}

GstSample* EncodingPipeline::get_last_sample() const noexcept
{
    if (m_pipeline == nullptr)
    {
        return nullptr;
    }

    GstElement* frame_producer = gst_bin_get_by_name(GST_BIN(m_pipeline), "frame-producer");
    assert(frame_producer != nullptr);

    GstSample* last_sample = nullptr;
    g_object_get(frame_producer, "last-sample", &last_sample, nullptr);
    gst_object_unref(frame_producer);

    return last_sample;
}
