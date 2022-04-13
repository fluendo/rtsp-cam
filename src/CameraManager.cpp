#include "CameraManager.h"

bool CameraManager::configure(const char* port) noexcept
{
    if (!m_streaming_server.configure(port))
    {
        g_printerr("Cannot configure streaming server.\n");
        return false;
    }

    return true;
}

bool CameraManager::start() noexcept
{
    if (!m_encoding_pipeline.start(&m_streaming_server))
    {
        g_printerr("Cannot start encoding pipeline.\n");
        return false;
    }

    if (!m_img_writer.start())
    {
        stop();
        g_printerr("Cannot start image writer pipeline.\n");
        return false;
    }

    if (!m_streaming_server.start())
    {
        stop();
        g_printerr("Cannot start streaming server.\n");
        return false;
    }

    return true;
}

void CameraManager::stop() noexcept
{
    m_streaming_server.stop();
    m_img_writer.stop();
    m_encoding_pipeline.stop();
}

bool CameraManager::take_screenshot() noexcept
{
    return m_img_writer.take_screenshot(m_encoding_pipeline);
}
