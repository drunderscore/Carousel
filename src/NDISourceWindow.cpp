/*
 * Copyright (c) 2023, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "NDISourceWindow.h"
#include <imgui/imgui.h>
#include <limits>
#include <optional>
#include <stdexcept>

namespace Carousel
{
NDISourceWindow::NDISourceWindow(const NDIlib_source_t& source) : m_source(source)
{
    create_receiver(m_receiver_bandwidth);
    set_frame_texture_filtering(m_frame_texture_filtering);
}

NDISourceWindow::~NDISourceWindow()
{
    if (m_receiver_instance)
    {
        NDIlib_recv_destroy(m_receiver_instance);
        m_receiver_instance = nullptr;
    }
}

bool NDISourceWindow::update()
{
    receive();

    int width{}, height{};
    m_frame_texture.with_bound([&width, &height]() {
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_WIDTH, &width);
        glGetTexLevelParameteriv(GL_TEXTURE_2D, 0, GL_TEXTURE_HEIGHT, &height);
    });

    std::optional<float> frame_aspect_ratio;
    if (height != 0)
        frame_aspect_ratio = static_cast<float>(width) / static_cast<float>(height);

    // A docked window won't respect its size constraints, so don't even bother.
    if (!ImGui::IsWindowDocked() && frame_aspect_ratio)
    {
        // FIXME: Minimum size should be made to be something reasonable from the aspect ratio.
        ImGui::SetNextWindowSizeConstraints(
            ImVec2(0, 0), ImVec2(std::numeric_limits<float>::max(), std::numeric_limits<float>::max()),
            [](ImGuiSizeCallbackData* data) {
                data->DesiredSize.y = data->DesiredSize.x / *reinterpret_cast<float*>(data->UserData);
            },
            &*frame_aspect_ratio);
    }

    if (ImGui::Begin(m_source.m_name.c_str(), &m_is_window_open) && m_is_window_open)
    {
        auto texture_size = ImGui::GetContentRegionAvail();

        // If we didn't end up setting the windows size constraints, size constrain the texture instead.
        if (ImGui::IsWindowDocked() && frame_aspect_ratio)
        {
            auto content_region_aspect_ratio = texture_size.x / texture_size.y;

            if (*frame_aspect_ratio > content_region_aspect_ratio)
                texture_size.y = texture_size.x / *frame_aspect_ratio;
            else
                texture_size.x = texture_size.y * *frame_aspect_ratio;
        }

        ImGui::Image(reinterpret_cast<ImTextureID>(m_frame_texture.name()), texture_size);

        if (ImGui::IsItemClicked(ImGuiMouseButton_Right))
            ImGui::OpenPopup("NDI Source Settings");
    }

    if (ImGui::BeginPopup("NDI Source Settings"))
    {
        if (ImGui::BeginMenu("Bandwidth"))
        {
            if (ImGui::MenuItem("Highest", nullptr, m_receiver_bandwidth == NDIlib_recv_bandwidth_highest))
            {
                m_receiver_bandwidth = NDIlib_recv_bandwidth_highest;
                create_receiver(m_receiver_bandwidth);
            }

            if (ImGui::MenuItem("Lowest", nullptr, m_receiver_bandwidth == NDIlib_recv_bandwidth_lowest))
            {
                m_receiver_bandwidth = NDIlib_recv_bandwidth_lowest;
                create_receiver(m_receiver_bandwidth);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Filtering"))
        {
            if (ImGui::MenuItem("Linear", nullptr, m_frame_texture_filtering == GL_LINEAR))
            {
                m_frame_texture_filtering = GL_LINEAR;
                set_frame_texture_filtering(m_frame_texture_filtering);
            }

            if (ImGui::MenuItem("Nearest", nullptr, m_frame_texture_filtering == GL_NEAREST))
            {
                m_frame_texture_filtering = GL_NEAREST;
                set_frame_texture_filtering(m_frame_texture_filtering);
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Resize to Source"))
            ImGui::SetWindowSize(m_source.m_name.c_str(),
                                 ImVec2(static_cast<float>(width), static_cast<float>(height)));

        ImGui::EndPopup();
    }

    ImGui::End();

    return !m_is_window_open;
}

void NDISourceWindow::create_receiver(NDIlib_recv_bandwidth_e bandwidth)
{
    if (m_receiver_instance)
    {
        NDIlib_recv_destroy(m_receiver_instance);
        m_receiver_instance = nullptr;
    }

    NDIlib_recv_create_v3_t receiver_create{};
    // RGB(A) is likely the worst format for bandwidth reasons, but was the simplest to get going with. probably
    // should prioritize support for one of the YUV formats that is subsampled, or maybe just all formats :^)
    receiver_create.color_format = NDIlib_recv_color_format_RGBX_RGBA;
    receiver_create.bandwidth = bandwidth;
    receiver_create.source_to_connect_to = NDIlib_source_t(m_source.m_name.c_str(), m_source.m_url_address.c_str());

    if (!(m_receiver_instance = NDIlib_recv_create_v3(&receiver_create)))
        throw std::runtime_error("Failed to create NDI receiver instance");
}

void NDISourceWindow::receive()
{
    NDIlib_video_frame_v2_t video_frame{};

    if (auto frame_type = NDIlib_recv_capture_v3(m_receiver_instance, &video_frame, nullptr, nullptr, 0);
        frame_type == NDIlib_frame_type_video)
    {
        m_frame_texture.with_bound([&video_frame]() {
            JMP::GL::Texture2D::set_data(0, GL_RGBA, video_frame.xres, video_frame.yres, GL_RGBA, GL_UNSIGNED_BYTE,
                                         video_frame.p_data);
        });

        NDIlib_recv_free_video_v2(m_receiver_instance, &video_frame);
    }
    else if (frame_type == NDIlib_frame_type_error)
    {
        throw std::runtime_error("Received error frame from NDI source");
    }
}

void NDISourceWindow::set_frame_texture_filtering(GLint filtering)
{
    m_frame_texture.with_bound([filtering]() {
        JMP::GL::Texture2D::set_parameter(GL_TEXTURE_MIN_FILTER, filtering);
        JMP::GL::Texture2D::set_parameter(GL_TEXTURE_MAG_FILTER, filtering);
    });
}
}