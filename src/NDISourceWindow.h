/*
 * Copyright (c) 2023, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "NDI.h"
#include <JMP/GL/Texture.h>
#include <string>

namespace Carousel
{
class Application;

class NDISourceWindow
{
public:
    class Source
    {
        friend class NDISourceWindow;

    public:
        explicit Source(const NDIlib_source_t& source) : m_name(source.p_ndi_name), m_url_address(source.p_url_address)
        {
        }

        std::string_view name() const { return m_name; }
        std::string_view url_address() const { return m_url_address; }

        bool operator==(const NDIlib_source_t& rhs) const
        {
            return name() == rhs.p_ndi_name && url_address() == rhs.p_url_address;
        }

    private:
        std::string m_name;
        std::string m_url_address;
    };

    explicit NDISourceWindow(const NDIlib_source_t&);
    ~NDISourceWindow();

    NDISourceWindow(const NDISourceWindow&) = delete;

    const Source& source() const { return m_source; }
    NDIlib_framesync_instance_t framesync_instance() const { return m_framesync_instance; }
    float audio_volume() const { return m_audio_volume; }
    bool is_audio_muted() const { return m_audio_muted; }
    bool is_window_focused() const { return m_is_window_focused; }

    bool update();

private:
    bool m_is_window_open = true;
    bool m_is_window_focused{};
    Source m_source;
    NDIlib_recv_instance_t m_receiver_instance{};
    NDIlib_framesync_instance_t m_framesync_instance{};
    JMP::GL::Texture2D m_frame_texture;
    NDIlib_recv_bandwidth_e m_receiver_bandwidth = NDIlib_recv_bandwidth_highest;
    GLint m_frame_texture_filtering = GL_LINEAR;
    float m_audio_volume = 1.0f;
    bool m_audio_muted = true;
    // Initialized at -1, so that if we receive a timecode of 0, we properly take that first frame.
    // This timecode is seen always and constantly by the Test Patterns NDI Tool
    int64_t m_frame_timecode = -1;

    void create_receiver_and_framesync(NDIlib_recv_bandwidth_e);
    void receive();
    void set_frame_texture_filtering(GLint);
};
}
