#include "StreamingServer.h"

#include <cassert>
#include <gst/rtsp-server/rtsp-server.h>

bool StreamingServer::create_server(const char* port) noexcept
{
    assert(m_loop == nullptr);
    assert(m_server_source == 0);
    assert(port != nullptr);
    assert(*port != 0);

    GstRTSPMediaFactory* tablet_factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(
        tablet_factory, "( queue name=tablet_pad ! clocksync sync-to-first=true ! rtph264pay name=pay0 pt=96 )");
    gst_rtsp_media_factory_set_shared(tablet_factory, TRUE);

    GstRTSPMediaFactory* cloud_factory = gst_rtsp_media_factory_new();
    gst_rtsp_media_factory_set_launch(
        cloud_factory, "( queue name=cloud_pad ! clocksync sync-to-first=true ! rtph264pay name=pay0 pt=96 )");
    gst_rtsp_media_factory_set_shared(cloud_factory, TRUE);

    GstRTSPServer* server = gst_rtsp_server_new();
    gst_rtsp_server_set_service(server, port);

    GstRTSPMountPoints* mounts = gst_rtsp_server_get_mount_points(server);
    if (mounts == nullptr)
    {
        g_object_unref(server);
        g_object_unref(cloud_factory);
        g_object_unref(tablet_factory);
        return false;
    }
    gst_rtsp_mount_points_add_factory(mounts, "/tablet", tablet_factory);
    gst_rtsp_mount_points_add_factory(mounts, "/cloud", cloud_factory);
    g_object_unref(mounts);

    m_server_source = gst_rtsp_server_attach(server, nullptr);
    g_object_unref(server);

    return (m_server_source > 0);
}

bool StreamingServer::configure(const char* port) noexcept
{
    if (m_loop != nullptr)
    {
        return false;
    }

    if ((port == nullptr) || (*port == 0))
    {
        port = static_cast<const char*>(DEFAULT_RTSP_PORT);
    }

    if (!create_server(port))
    {
        return false;
    }

    m_loop = g_main_loop_new(nullptr, FALSE);
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
}
