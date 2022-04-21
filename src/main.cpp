#include "CameraManager.h"

#include <glib-unix.h>

namespace
{
gboolean on_take_screenshot(CameraManager* manager)
{
    manager->take_screenshot();
    return G_SOURCE_CONTINUE;
}

gboolean on_switch_recording(CameraManager* manager)
{
    if (manager->is_recording())
    {
        manager->stop_recording();
    }
    else
    {
        manager->start_recording();
    }
    return G_SOURCE_CONTINUE;
}

gboolean on_quit(CameraManager* manager)
{
    g_print("\n");
    manager->shut();
    return G_SOURCE_REMOVE;
}
} // namespace

int main(int argc, char* argv[])
{
    gst_init(&argc, &argv);

    CameraManager manager;
    if (!manager.init())
    {
        return -1;
    }

    g_unix_signal_add(SIGUSR1, reinterpret_cast<GSourceFunc>(on_take_screenshot), &manager);
    g_unix_signal_add(SIGUSR2, reinterpret_cast<GSourceFunc>(on_switch_recording), &manager);
    g_unix_signal_add(SIGINT, reinterpret_cast<GSourceFunc>(on_quit), &manager);
    if (!manager.run_and_wait())
    {
        return -2;
    }

    return 0;
}
