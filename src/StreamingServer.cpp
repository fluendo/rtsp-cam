#include "StreamingServer.h"

#include <cassert>
#include <gst/app/app.h>

namespace
{
constexpr char DEFAULT_RTSP_PORT[] = "8554";
constexpr char MEDIA_FACTORY_BIN_DESC[] =
    "( appsrc name=entry-point is-live=true do-timestamp=true caps=\"video/x-h264,framerate=30/1\" emit-signals=false "
    "format=time ! h264parse ! rtph264pay name=pay0 pt=96 )";
constexpr char MEDIA_IDX_KEY[] = "media-idx";
constexpr guint SESSIONS_CLEANUP_TIMEOUT_IN_SECONDS = 5;
} // namespace

gboolean StreamingServer::on_sessions_cleanup(StreamingServer* streaming_server) noexcept
{
    assert(streaming_server != nullptr);
    assert(streaming_server->m_server != nullptr);

    GstRTSPSessionPool* pool = gst_rtsp_server_get_session_pool(streaming_server->m_server);
    gst_rtsp_session_pool_cleanup(pool);
    g_object_unref(pool);

    return G_SOURCE_CONTINUE;
}

void StreamingServer::on_media_configure(GstRTSPMediaFactory* factory, GstRTSPMedia* media,
                                         StreamingServer* streaming_server) noexcept
{
    assert(factory != nullptr);
    assert(media != nullptr);
    assert(streaming_server != nullptr);

    auto media_idx =
        static_cast<unsigned int>(reinterpret_cast<guintptr>(g_object_get_data(G_OBJECT(factory), MEDIA_IDX_KEY)));
    assert(media_idx < NB_MEDIA);

    GstElement* media_bin = gst_rtsp_media_get_element(media);
    assert(media_bin != nullptr);
    GstElement* entry_point = gst_bin_get_by_name(GST_BIN(media_bin), "entry-point");
    assert(entry_point != nullptr);
    gst_object_unref(media_bin);

    std::lock_guard<std::mutex> guard(streaming_server->m_media_mutex[media_idx]);
    if (streaming_server->m_media_appsrc[media_idx] != nullptr)
    {
        gst_object_unref(streaming_server->m_media_appsrc[media_idx]);
    }
    streaming_server->m_media_appsrc[media_idx] = entry_point;
}

bool StreamingServer::create_server(const char* port) noexcept
{
    assert(m_server == nullptr);
    assert(m_server_source == 0);
    assert(m_loop == nullptr);
    assert(m_loop_timeout == 0);
    assert(port != nullptr);
    assert(*port != 0);

    GstRTSPServer* server = gst_rtsp_server_new();
    gst_rtsp_server_set_service(server, port);

    GstRTSPMountPoints* mounts = gst_rtsp_server_get_mount_points(server);
    if (mounts == nullptr)
    {
        g_printerr("ERROR: cannot get RTSP server mount points\n");
        g_object_unref(server);
        return false;
    }

    char buff[9]; // until "/video99", just in case // NOLINT
    for (unsigned int i = 0; i < NB_MEDIA; ++i)
    {
        GstRTSPMediaFactory* media_factory = gst_rtsp_media_factory_new();
        g_object_set_data(G_OBJECT(media_factory), MEDIA_IDX_KEY, reinterpret_cast<gpointer>(static_cast<guintptr>(i)));

        gst_rtsp_media_factory_set_launch(media_factory, MEDIA_FACTORY_BIN_DESC);
        gst_rtsp_media_factory_set_shared(media_factory, TRUE);

        if (g_signal_connect(media_factory, "media-configure",
                             reinterpret_cast<GCallback>(StreamingServer::on_media_configure), this) == 0)
        {
            g_printerr("ERROR: cannot connect signal to media factory #%u\n", i);
            g_object_unref(media_factory);
            g_object_unref(mounts);
            g_object_unref(server);
            return false;
        }

        g_snprintf(buff, sizeof(buff), "/video%u", i);
        gst_rtsp_mount_points_add_factory(mounts, buff, media_factory);
    }
    g_object_unref(mounts);

    m_server_source = gst_rtsp_server_attach(server, nullptr);
    if (m_server_source == 0)
    {
        g_printerr("ERROR: cannot attach RTSP server to main context\n");
        g_object_unref(server);
        return false;
    }

    m_server = server;
    return true;
}

bool StreamingServer::configure(const char* port) noexcept
{
    if (m_loop != nullptr)
    {
        return false;
    }

    if ((port == nullptr) || (*port == 0))
    {
        port = DEFAULT_RTSP_PORT;
    }

    if (!create_server(port))
    {
        return false;
    }

    m_loop = g_main_loop_new(nullptr, FALSE);
    m_loop_timeout = g_timeout_add_seconds(SESSIONS_CLEANUP_TIMEOUT_IN_SECONDS,
                                           reinterpret_cast<GSourceFunc>(on_sessions_cleanup), this);
    g_print("Server configured at rtsp://127.0.0.1:%s\n", port);
    return true;
}

bool StreamingServer::start() noexcept
{
    if (m_loop != nullptr)
    {
        g_print("Server started\n");
        g_main_loop_run(m_loop);
        return true;
    }

    return false;
}

void StreamingServer::stop() noexcept
{
    for (unsigned int i = 0; i < NB_MEDIA; ++i)
    {
        std::lock_guard<std::mutex> guard(m_media_mutex[i]);
        if (m_media_appsrc[i] != nullptr)
        {
            gst_object_unref(m_media_appsrc[i]);
            m_media_appsrc[i] = nullptr;
        }
    }

    if (m_loop_timeout > 0)
    {
        g_source_remove(m_loop_timeout);
        m_loop_timeout = 0;
    }

    if (m_loop != nullptr)
    {
        g_main_loop_quit(m_loop);
        g_main_loop_unref(m_loop);
        m_loop = nullptr;
    }

    if (m_server_source > 0)
    {
        g_source_remove(m_server_source);
        m_server_source = 0;
        g_print("Server stopped\n");
    }

    if (m_server != nullptr)
    {
        g_object_unref(m_server);
        m_server = nullptr;
    }
}

bool StreamingServer::push_caps(unsigned int /*stream_idx*/, GstCaps* /*caps*/) noexcept
{
    // Nothing to do here as we already know the caps
    return true;
}

bool StreamingServer::push_buffer(unsigned int stream_idx, GstBuffer* buffer) noexcept
{
    if (buffer == nullptr)
    {
        return false;
    }

    GstElement* appsrc = nullptr;
    if (stream_idx < NB_MEDIA)
    {
        std::lock_guard<std::mutex> guard(m_media_mutex[stream_idx]);
        if (m_media_appsrc[stream_idx] != nullptr)
        {
            appsrc = GST_ELEMENT(gst_object_ref(m_media_appsrc[stream_idx]));
        }
    }

    if (appsrc == nullptr)
    {
        return false;
    }

    // Invalidate buffer pts and dts to enable appsrc retimestamping
    // on current running-time (do-timestamp=true).
    // We can directly retimestamp all incoming buffers on pipeline
    // running-time because they are pushed synchronously to the RTSP
    // media pipeline (sync=true in EncodingPipeline fakesink configurations).
    buffer = gst_buffer_copy(buffer);
    GST_BUFFER_PTS(buffer) = GST_CLOCK_TIME_NONE;
    GST_BUFFER_DTS(buffer) = GST_CLOCK_TIME_NONE;

    GstFlowReturn ret = gst_app_src_push_buffer(GST_APP_SRC(appsrc), buffer);
    if (ret == GST_FLOW_EOS)
    {
        std::lock_guard<std::mutex> guard(m_media_mutex[stream_idx]);
        if (m_media_appsrc[stream_idx] == appsrc)
        {
            gst_object_unref(m_media_appsrc[stream_idx]);
            m_media_appsrc[stream_idx] = nullptr;
        }
    }

    gst_object_unref(appsrc);
    return (ret == GST_FLOW_OK);
}
