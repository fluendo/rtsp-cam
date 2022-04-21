#include "CameraManager.h"

bool CameraManager::init(const char* port) noexcept
{
    if (!m_streaming_server.configure(port))
    {
        g_printerr("Cannot configure streaming server\n");
        return false;
    }

    return true;
}

bool CameraManager::run_and_wait() noexcept
{
    if (!m_stream_recorder.init())
    {
        shut();
        g_printerr("Cannot initialize stream recorder\n");
        return false;
    }

    if (!m_encoding_pipeline.start(m_streaming_server, m_stream_recorder))
    {
        shut();
        g_printerr("Cannot start encoding pipeline\n");
        return false;
    }

    if (!m_img_writer.start())
    {
        shut();
        g_printerr("Cannot start image writer pipeline\n");
        return false;
    }

    if (!m_streaming_server.start())
    {
        shut();
        g_printerr("Cannot start streaming server\n");
        return false;
    }

    return true;
}

void CameraManager::shut() noexcept
{
    m_streaming_server.stop();
    m_img_writer.stop();
    m_encoding_pipeline.stop();
    m_stream_recorder.shut();
}

bool CameraManager::start_recording() noexcept
{
    return m_stream_recorder.start_recording();
}

void CameraManager::stop_recording() noexcept
{
    m_stream_recorder.stop_recording();
}

bool CameraManager::is_recording() const noexcept
{
    return m_stream_recorder.is_recording();
}

bool CameraManager::take_screenshot() noexcept
{
    return m_img_writer.take_screenshot(m_encoding_pipeline);
}
