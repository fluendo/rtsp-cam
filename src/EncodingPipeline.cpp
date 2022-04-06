#include "EncodingPipeline.h"

#include <cassert>

GstPadProbeReturn EncodingPipeline::highq_stream_pad_probe(GstPad* pad, GstPadProbeInfo* info,
                                                           EncodingPipeline* pipeline)
{
    assert(pad != nullptr);
    assert(info != nullptr);
    assert(pipeline != nullptr);

    // TODO

    return GST_PAD_PROBE_OK;
}

GstPadProbeReturn EncodingPipeline::lowq_stream_pad_probe(GstPad* pad, GstPadProbeInfo* info,
                                                          EncodingPipeline* pipeline)
{
    assert(pad != nullptr);
    assert(info != nullptr);
    assert(pipeline != nullptr);

    // TODO

    return GST_PAD_PROBE_OK;
}

bool EncodingPipeline::create_pipeline() noexcept
{
    assert(m_pipeline == nullptr);

    GError* error = nullptr;
    // clang-format off
    GstElement* pipeline = gst_parse_launch(
        "v4l2src ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! tee name=raw-img "
        "raw-img. ! queue ! videoscale ! vaapih264enc bitrate=2048 cabac=true dct8x8=true keyframe-period=0 quality-level=2 rate-control=vbr ! video/x-h264,profile=high,stream-format=byte-stream,width=640,height=480 ! h264parse ! qtmux ! filesink location=./out.mp4 "
        "raw-img. ! queue ! videoscale ! vaapih264enc bitrate=1024 cabac=true keyframe-period=0 quality-level=6 rate-control=vbr ! video/x-h264,profile=main,stream-format=byte-stream,width=640,height=480 ! h264parse ! fakesink name=highq-stream "
        "raw-img. ! queue ! videoscale ! vaapih264enc bitrate=512 cabac=true keyframe-period=0 quality-level=7 rate-control=vbr ! video/x-h264,profile=main,stream-format=byte-stream,width=320,height=240 ! h264parse ! fakesink name=lowq-stream",
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

bool EncodingPipeline::register_buffer_probes() noexcept
{
    assert(m_pipeline != nullptr);

    for (int i = 0; i < 2; ++i)
    {
        bool high = (i > 0);

        GstElement* sink = gst_bin_get_by_name(GST_BIN(m_pipeline), high ? "highq-stream" : "lowq-stream");
        assert(sink != nullptr);

        GstPad* sink_pad = gst_element_get_static_pad(sink, "sink");
        assert(sink_pad != nullptr);

        gulong probe_id =
            gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_DATA_DOWNSTREAM,
                              reinterpret_cast<GstPadProbeCallback>(high ? EncodingPipeline::highq_stream_pad_probe
                                                                         : EncodingPipeline::lowq_stream_pad_probe),
                              this, nullptr);

        gst_object_unref(sink_pad);
        gst_object_unref(sink);

        if (probe_id == 0)
        {
            g_printerr("ERROR: cannot register buffer probe for %s\n", high ? "highq-stream" : "lowq-stream");
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
            return false;
        }
    }

    return true;
}

bool EncodingPipeline::start() noexcept
{
    if (m_pipeline != nullptr)
    {
        return true;
    }

    if (!create_pipeline() || !register_buffer_probes())
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
