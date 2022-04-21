#pragma once

#include "IStreamConsumer.h"

#include <gst/rtsp-server/rtsp-server.h>
#include <mutex>

class StreamingServer final : public IStreamConsumer
{
  public:
    StreamingServer() = default;

    StreamingServer(StreamingServer&&) = delete;
    StreamingServer& operator=(StreamingServer&&) = delete;
    StreamingServer(const StreamingServer&) = delete;
    StreamingServer& operator=(const StreamingServer&) = delete;

    ~StreamingServer() override
    {
        stop();
    }

    bool configure(const char* port = nullptr) noexcept;
    bool start() noexcept;
    void stop() noexcept;

    bool push_caps(unsigned int stream_idx, GstCaps* caps) noexcept override;
    bool push_buffer(unsigned int stream_idx, GstBuffer* buffer) noexcept override;

  private:
    static constexpr unsigned int NB_MEDIA = 2;
    static gboolean on_sessions_cleanup(StreamingServer* streaming_server) noexcept;
    static void on_media_configure(GstRTSPMediaFactory* factory, GstRTSPMedia* media,
                                   StreamingServer* streaming_server) noexcept;

    bool create_server(const char* port) noexcept;

    GstRTSPServer* m_server = nullptr;
    guint m_server_source = 0;
    GMainLoop* m_loop = nullptr;
    guint m_loop_timeout = 0;

    std::mutex m_media_mutex[NB_MEDIA];
    GstElement* m_media_appsrc[NB_MEDIA] = {nullptr};
};
