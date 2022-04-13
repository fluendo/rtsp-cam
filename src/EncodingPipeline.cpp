#include "EncodingPipeline.h"

#include <cassert>

namespace
{
constexpr unsigned int NB_STREAMS = 2;
constexpr char STREAM_IDX_KEY[] = "stream-idx";
constexpr GstClockTime EOS_PROPAGATION_TIMEOUT = 5 * GST_SECOND;

GstPadProbeReturn stream_pad_probe(GstPad* pad, GstPadProbeInfo* info, IStreamConsumer* stream_consumer)
{
    assert(pad != nullptr);
    assert(info != nullptr);
    assert(stream_consumer != nullptr);

    if (info->data != nullptr)
    {
        assert((info->type & GST_PAD_PROBE_TYPE_BUFFER) == GST_PAD_PROBE_TYPE_BUFFER);

        auto stream_idx =
            static_cast<unsigned int>(reinterpret_cast<guintptr>(g_object_get_data(G_OBJECT(pad), STREAM_IDX_KEY)));
        assert(stream_idx < NB_STREAMS);

        stream_consumer->push_buffer(stream_idx, GST_BUFFER(info->data));
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
        "v4l2src ! video/x-raw,width=640,height=480,framerate=30/1 ! videoconvert ! tee name=raw-img "
        "raw-img. ! queue silent=true ! fakesink name=frame-producer enable-last-sample=true "
        "raw-img. ! queue silent=true ! videoscale ! vaapih264enc bitrate=2048 cabac=true dct8x8=true keyframe-period=0 quality-level=2 rate-control=vbr ! video/x-h264,profile=high,stream-format=byte-stream ! h264parse ! qtmux ! filesink enable-last-sample=false qos=true location=./out.mp4 "
        "raw-img. ! queue silent=true ! videoscale ! vaapih264enc bitrate=1024 cabac=true keyframe-period=0 quality-level=6 rate-control=vbr ! video/x-h264,profile=main,stream-format=byte-stream ! fakesink name=stream0 enable-last-sample=false qos=true sync=true "
        "raw-img. ! queue silent=true ! videoscale ! vaapih264enc bitrate=512 cabac=true keyframe-period=0 quality-level=7 rate-control=vbr ! video/x-h264,profile=main,stream-format=byte-stream,width=320,height=240 ! fakesink name=stream1 enable-last-sample=false qos=true sync=true",
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

bool EncodingPipeline::register_buffer_probes(IStreamConsumer* stream_consumer) noexcept
{
    assert(m_pipeline != nullptr);
    assert(stream_consumer != nullptr);

    char buff[9]; // until "stream99", just in case // NOLINT
    for (unsigned int i = 0; i < NB_STREAMS; ++i)
    {
        g_snprintf(buff, sizeof(buff), "stream%u", i);

        GstElement* sink = gst_bin_get_by_name(GST_BIN(m_pipeline), buff);
        assert(sink != nullptr);

        GstPad* sink_pad = gst_element_get_static_pad(sink, "sink");
        assert(sink_pad != nullptr);
        g_object_set_data(G_OBJECT(sink_pad), STREAM_IDX_KEY, reinterpret_cast<gpointer>(static_cast<guintptr>(i)));

        gulong probe_id =
            gst_pad_add_probe(sink_pad, GST_PAD_PROBE_TYPE_BUFFER,
                              reinterpret_cast<GstPadProbeCallback>(stream_pad_probe), stream_consumer, nullptr);

        gst_object_unref(sink_pad);
        gst_object_unref(sink);

        if (probe_id == 0)
        {
            g_printerr("ERROR: cannot register buffer probe for stream #%u\n", i);
            gst_object_unref(m_pipeline);
            m_pipeline = nullptr;
            return false;
        }
    }

    return true;
}

void EncodingPipeline::finish_grabbing() noexcept
{
    assert(m_pipeline != nullptr);

    GstState state = GST_STATE_NULL;
    GstStateChangeReturn ret = gst_element_get_state(GST_ELEMENT(m_pipeline), &state, nullptr, 0);
    if ((ret != GST_STATE_CHANGE_SUCCESS) || (state != GST_STATE_PLAYING))
    {
        return;
    }

    g_print("Finishing grabbing...\n");

    GstElement* tee = gst_bin_get_by_name(GST_BIN(m_pipeline), "raw-img");
    assert(tee != nullptr);
    GstPad* sink_pad = gst_element_get_static_pad(tee, "sink");
    assert(sink_pad != nullptr);
    gst_object_unref(tee);
    gboolean event_sent = gst_pad_send_event(sink_pad, gst_event_new_eos());
    gst_object_unref(sink_pad);

    if (event_sent != TRUE)
    {
        g_printerr("WARNING: cannot send EOS event to the pipeline, grabbed video may be corrupted.\n");
        return;
    }

    GstBus* bus = gst_pipeline_get_bus(m_pipeline);
    assert(bus != nullptr);
    GstMessage* eos_message = gst_bus_timed_pop_filtered(bus, EOS_PROPAGATION_TIMEOUT, GST_MESSAGE_EOS);
    gst_object_unref(bus);

    if (eos_message != nullptr)
    {
        gst_message_unref(eos_message);
    }
    else
    {
        g_printerr("WARNING: not receiving EOS message, grabbed video may be corrupted.\n");
    }
}

bool EncodingPipeline::start(IStreamConsumer* stream_consumer) noexcept
{
    if (m_pipeline != nullptr)
    {
        return true;
    }

    if ((stream_consumer == nullptr) || !create_pipeline() || !register_buffer_probes(stream_consumer))
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
        // When stopping the encoding pipeline, we must ensure that the grabbing branch
        // has processed all the buffers, else the resulting file may be corrupted.
        // To do so, we push an EOS event at the beginning of the branch and we wait for
        // the event to reach the final filesink.
        finish_grabbing();

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
