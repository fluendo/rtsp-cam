#include "EncodingPipeline.h"
#include "StreamingServer.h"

#include <glib-unix.h>

static gboolean on_quit(StreamingServer* server)
{
    g_print("\n");
    server->stop();
    return G_SOURCE_REMOVE;
}

int main(int argc, char* argv[])
{
    gst_init(&argc, &argv);

    StreamingServer server;
    if (!server.configure())
    {
        g_printerr("Cannot configure streaming server\n");
        return -1;
    }

    g_unix_signal_add(SIGINT, reinterpret_cast<GSourceFunc>(on_quit), &server);
    if (!server.start())
    {
        g_printerr("Cannot start streaming server\n");
        return -2;
    }

    return 0;
}
