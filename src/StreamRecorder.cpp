#include "StreamRecorder.h"

#include <cassert>
#include <gst/app/app.h>

namespace
{
constexpr GstClockTime EOS_PROPAGATION_TIMEOUT = 5 * GST_SECOND;
constexpr GstClockTime WAITING_FOR_PLAYING_STATE_TIMEOUT = 3 * GST_SECOND;
} // namespace

bool StreamRecorder::create_pipeline() noexcept
{
    assert(m_pipeline == nullptr);

    GError* error = nullptr;
    GstElement* pipeline =
        gst_parse_launch("appsrc name=entry-point is-live=true do-timestamp=true emit-signals=false format=time "
                         "leaky-type=downstream max-buffers=5 "
                         "! identity name=sink_identity silent=false "
                         "! nvvidconv ! omxh264enc control-rate=1 bitrate=30000000 peak-bitrate=52000000 "
                         "! video/x-h264,stream-format=byte-stream ! h264parse ! qtmux ! "
                         "! queue max-size-bytes=0 max-size-time=0 max-size-buffers=10 "
                         "filesink name=file-output enable-last-sample=false qos=true",
                         &error);

    if (pipeline == nullptr)
    {
        if (error != nullptr)
        {
            g_printerr("ERROR: cannot create stream recorder pipeline (%s)\n", error->message);
            g_error_free(error);
        }
        else
        {
            g_printerr("ERROR: cannot create stream recorder pipeline (unspecified error)\n");
        }

        return false;
    }

    if (error != nullptr)
    {
        g_printerr("WARNING: fixed issue encountered while creating stream recorder pipeline (%s)\n", error->message);
        g_error_free(error);
    }

    m_pipeline = GST_PIPELINE(gst_object_ref_sink(pipeline));
    return true;
}

void StreamRecorder::finish_grabbing() noexcept
{
    assert(m_pipeline != nullptr);
    assert(m_appsrc != nullptr);

    GstState state = GST_STATE_NULL;
    GstStateChangeReturn ret = gst_element_get_state(GST_ELEMENT(m_pipeline), &state, nullptr, 0);
    if ((ret != GST_STATE_CHANGE_SUCCESS) || (state != GST_STATE_PLAYING))
    {
        return;
    }

    g_print("Finishing grabbing...\n");
    if (gst_app_src_end_of_stream(GST_APP_SRC(m_appsrc)) != GST_FLOW_OK)
    {
        g_printerr("WARNING: cannot send EOS event to the pipeline, grabbed video may be corrupted\n");
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
        g_printerr("WARNING: not receiving EOS message, grabbed video may be corrupted\n");
    }
}

bool StreamRecorder::init() noexcept
{
    if (m_pipeline != nullptr)
    {
        assert(m_appsrc != nullptr);
        return true;
    }

    assert(m_appsrc == nullptr);
    if (!create_pipeline())
    {
        return false;
    }

    m_appsrc = gst_bin_get_by_name(GST_BIN(m_pipeline), "entry-point");
    assert(m_appsrc != nullptr);

    g_print("Stream recorder configured\n");
    return true;
}

void StreamRecorder::shut() noexcept
{
    stop_recording();

    if (m_appsrc != nullptr)
    {
        gst_object_unref(m_appsrc);
        m_appsrc = nullptr;
    }

    if (m_pipeline != nullptr)
    {
        gst_object_unref(m_pipeline);
        m_pipeline = nullptr;
    }
}

bool StreamRecorder::start_recording() noexcept
{
    if (m_pipeline == nullptr)
    {
        return false;
    }

    assert(m_appsrc != nullptr);
    if (is_recording())
    {
        return true;
    }

    // Set output filename for the recorded video
    char filename[16]; // until "./video_999.mp4", just in case // NOLINT
    g_snprintf(filename, sizeof(filename), "./video_%03u.mp4", m_video_idx);

    GstElement* sink = gst_bin_get_by_name(GST_BIN(m_pipeline), "file-output");
    assert(sink != nullptr);
    g_object_set(sink, "location", filename, nullptr);
    gst_object_unref(sink);

    // Start recording pipeline
    if (gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_READY) != GST_STATE_CHANGE_SUCCESS)
    {
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
        g_printerr("ERROR: cannot change stream recorder pipeline to READY state\n");
        return false;
    }

    if (gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PAUSED) != GST_STATE_CHANGE_NO_PREROLL)
    {
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
        g_printerr("ERROR: cannot change stream recorder pipeline to PAUSED state\n");
        return false;
    }

    if (gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_PLAYING) != GST_STATE_CHANGE_ASYNC)
    {
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
        g_printerr("ERROR: cannot change stream recorder pipeline to PLAYING state\n");
        return false;
    }

    GstState state = GST_STATE_NULL;
    if ((gst_element_get_state(GST_ELEMENT(m_pipeline), &state, nullptr, WAITING_FOR_PLAYING_STATE_TIMEOUT) !=
         GST_STATE_CHANGE_SUCCESS) ||
        (state != GST_STATE_PLAYING))
    {
        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
        g_printerr("ERROR: cannot change stream recorder pipeline to PLAYING state\n");
        return false;
    }

    ++m_video_idx;
    g_print("Start recording video to %s\n", filename);

    return true;
}

void StreamRecorder::stop_recording() noexcept
{
    if (m_pipeline != nullptr)
    {
        // When stopping the recording pipeline, we must ensure that the pipeline
        // has processed all the buffers, else the resulting file may be corrupted.
        // To do so, we push an EOS event at the beginning of the pipeline and we
        // wait for the event to reach the final file sink.
        finish_grabbing();

        gst_element_set_state(GST_ELEMENT(m_pipeline), GST_STATE_NULL);
        g_print("Stream recorder stopped\n");
    }
}

bool StreamRecorder::is_recording() const noexcept
{
    if (m_pipeline != nullptr)
    {
        GstState state = GST_STATE_NULL;
        if ((gst_element_get_state(GST_ELEMENT(m_pipeline), &state, nullptr, 0) != GST_STATE_CHANGE_FAILURE) &&
            (state > GST_STATE_NULL))
        {
            return true;
        }
    }

    return false;
}

bool StreamRecorder::push_caps(unsigned int /*stream_idx*/, GstCaps* caps) noexcept
{
    // WARNING: same remark about multithreading as the one below
    // in push_buffer() method.
    if ((caps == nullptr) || (m_appsrc == nullptr))
    {
        return false;
    }

    g_object_set(m_appsrc, "caps", caps, nullptr);
    return true;
}

bool StreamRecorder::push_buffer(unsigned int /*stream_idx*/, GstBuffer* buffer) noexcept
{
    // WARNING: this method is normally called from the streaming thread of the
    // encoding pipeline raw branch (from raw_stream_pad_probe() pad probe in
    // EncodingPipeline.cpp). As m_appsrc is initialized and released from the
    // application user thread (init/shut methods), we may potentially encounter
    // multithreading issues here.
    //
    // BUT, as the StreamRecorder MUST be initialized before passing it to
    // the EncodingPipeline (through the EncodingPipeline::start() method) and
    // MUST be shut only AFTER stopping the EncodingPipeline (see CameraManager
    // run_and_wait() and shut() methods), we don't need any kind of
    // synchronization here. The m_appsrc member is always initialized BEFORE
    // starting the streaming threads and released AFTER terminating the
    // streaming threads.
    //
    // Indeed, if previous conditions are changed, correct multithreading
    // synchronization would be required to access m_appsrc member.
    if ((buffer == nullptr) || (m_appsrc == nullptr))
    {
        return false;
    }

    // Invalidate buffer pts and dts to enable appsrc retimestamping
    // on current running-time (do-timestamp=true).
    // We can directly retimestamp all incoming buffers on pipeline
    // running-time because they are pushed synchronously to the recording
    // pipeline (sync=true in EncodingPipeline "frame-producer" fakesink).
    buffer = gst_buffer_copy(buffer);
    GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;

    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(m_appsrc), buffer);
    return (ret == GST_FLOW_OK);
}
