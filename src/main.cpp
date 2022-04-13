#include "CameraManager.h"

#include <glib-unix.h>

namespace
{
gboolean on_take_screenshot(CameraManager* manager)
{
    manager->take_screenshot();
    return G_SOURCE_CONTINUE;
}

gboolean on_quit(CameraManager* manager)
{
    g_print("\n");
    manager->stop();
    return G_SOURCE_REMOVE;
}
} // namespace

int main(int argc, char* argv[])
{
    gst_init(&argc, &argv);

    CameraManager manager;
    if (!manager.configure())
    {
        return -1;
    }

    g_unix_signal_add(SIGUSR1, reinterpret_cast<GSourceFunc>(on_take_screenshot), &manager);
    g_unix_signal_add(SIGINT, reinterpret_cast<GSourceFunc>(on_quit), &manager);
    if (!manager.start())
    {
        return -2;
    }

    return 0;
}
