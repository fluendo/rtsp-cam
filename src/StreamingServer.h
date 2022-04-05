#pragma once

#include <glib.h>

class StreamingServer final
{
  public:
    StreamingServer() = default;
    StreamingServer(StreamingServer&&) = default;
    StreamingServer& operator=(StreamingServer&&) = default;

    StreamingServer(const StreamingServer&) = delete;
    StreamingServer& operator=(const StreamingServer&) = delete;

    ~StreamingServer()
    {
        stop();
    }

    bool configure(const char* port = nullptr) noexcept;
    bool start() noexcept;
    void stop() noexcept;

  private:
    static constexpr char DEFAULT_RTSP_PORT[] = "8554";

    bool create_server(const char* port) noexcept;

    guint m_server_source = 0;
    GMainLoop* m_loop = nullptr;
};
