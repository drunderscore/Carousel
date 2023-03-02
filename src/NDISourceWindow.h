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
        Source(const NDIlib_source_t& source) : m_name(source.p_ndi_name), m_url_address(source.p_url_address) {}

        std::string_view name() const { return m_name; }
        std::string_view url_address() const { return m_url_address; }

        bool operator==(const Source& rhs) const { return name() == rhs.name() && url_address() == rhs.url_address(); }
        bool operator!=(const Source& rhs) const { return !(rhs == *this); }

    private:
        std::string m_name;
        std::string m_url_address;
    };

    explicit NDISourceWindow(const NDIlib_source_t&);
    ~NDISourceWindow();

    NDISourceWindow(const NDISourceWindow&) = delete;

    const Source& source() const { return m_source; }

    bool update();

private:
    bool m_is_window_open = true;
    Source m_source;
    NDIlib_recv_instance_t m_receiver_instance{};
    JMP::GL::Texture2D m_frame_texture;
    NDIlib_recv_bandwidth_e m_receiver_bandwidth = NDIlib_recv_bandwidth_highest;
    GLint m_frame_texture_filtering = GL_LINEAR;

    void create_receiver(NDIlib_recv_bandwidth_e);
    void receive();
    void set_frame_texture_filtering(GLint);
};
}
