/*
 * Copyright (c) 2023, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "NDI.h"
#include "NDISourceWindow.h"
#include <memory>
#include <miniaudio.h>
#include <mutex>
#include <span>
#include <vector>

struct GLFWwindow;

namespace Carousel
{
class Application
{
public:
    Application();
    ~Application();
    Application(const Application&) = delete;

    int run();

private:
    static constexpr bool s_use_vsync = true;

    GLFWwindow* m_window{};
    NDIlib_find_instance_t m_ndi_finder_instance{};
    std::span<const NDIlib_source_t> m_found_ndi_sources{};
    std::vector<std::unique_ptr<NDISourceWindow>> m_ndi_source_windows;
    std::mutex m_ndi_source_windows_mutex;
    ma_context m_audio_context{};
    std::span<ma_device_info> m_playback_device_infos;
    ma_device m_playback_device{};
    bool m_only_play_audio_from_focused_window{};

    void create_finder();
    bool initialize_playback_device(ma_device_info*);
    static void miniaudio_playback_data_callback(ma_device* device, void* output, const void*, ma_uint32 frame_count);
};
}