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
    JMP::ScopeGuard free_if_error_occurs = [this]() {
        // NDI says: You should always destroy the receiver after the frame-sync has been destroyed.
        if (m_framesync_instance)
        {
            NDIlib_framesync_destroy(m_framesync_instance);
            m_framesync_instance = nullptr;
        }

        if (m_receiver_instance)
        {
            NDIlib_recv_destroy(m_receiver_instance);
            m_receiver_instance = nullptr;
        }
    };

    create_receiver_and_framesync(m_receiver_bandwidth);
    set_frame_texture_filtering(m_frame_texture_filtering);

    free_if_error_occurs.disarm();
}

NDISourceWindow::~NDISourceWindow()
{
    // NDI says: You should always destroy the receiver after the frame-sync has been destroyed.
    if (m_framesync_instance)
    {
        NDIlib_framesync_destroy(m_framesync_instance);
        m_framesync_instance = nullptr;
    }

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
        m_is_window_focused = ImGui::IsWindowFocused();
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
            auto is_highest_bandwidth_enabled = m_receiver_bandwidth == NDIlib_recv_bandwidth_highest;
            auto is_lowest_bandwidth_enabled = m_receiver_bandwidth == NDIlib_recv_bandwidth_lowest;

            if (ImGui::MenuItem("Highest", nullptr, is_highest_bandwidth_enabled, !is_highest_bandwidth_enabled))
            {
                m_receiver_bandwidth = NDIlib_recv_bandwidth_highest;
                create_receiver_and_framesync(m_receiver_bandwidth);
            }

            if (ImGui::MenuItem("Lowest", nullptr, is_lowest_bandwidth_enabled, !is_lowest_bandwidth_enabled))
            {
                m_receiver_bandwidth = NDIlib_recv_bandwidth_lowest;
                create_receiver_and_framesync(m_receiver_bandwidth);
            }

            ImGui::EndMenu();
        }

        if (ImGui::BeginMenu("Filtering"))
        {
            auto is_using_linear_filtering = m_frame_texture_filtering == GL_LINEAR;
            auto is_using_nearest_filtering = m_frame_texture_filtering == GL_NEAREST;

            if (ImGui::MenuItem("Linear", nullptr, is_using_linear_filtering, !is_using_linear_filtering))
            {
                m_frame_texture_filtering = GL_LINEAR;
                set_frame_texture_filtering(m_frame_texture_filtering);
            }

            if (ImGui::MenuItem("Nearest", nullptr, is_using_nearest_filtering, !is_using_nearest_filtering))
            {
                m_frame_texture_filtering = GL_NEAREST;
                set_frame_texture_filtering(m_frame_texture_filtering);
            }

            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Resize to Source"))
            ImGui::SetWindowSize(m_source.m_name.c_str(),
                                 ImVec2(static_cast<float>(width), static_cast<float>(height)));

        if (ImGui::BeginMenu("Audio"))
        {
            ImGui::SliderFloat("Volume", &m_audio_volume, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp);
            ImGui::SameLine();
            ImGui::Checkbox("Mute", &m_audio_muted);
            ImGui::EndMenu();
        }

        if (ImGui::MenuItem("Reconnect"))
            create_receiver_and_framesync(m_receiver_bandwidth);

        ImGui::EndPopup();
    }

    ImGui::End();

    return !m_is_window_open;
}

void NDISourceWindow::create_receiver_and_framesync(NDIlib_recv_bandwidth_e bandwidth)
{
    // NDI says: You should always destroy the receiver after the frame-sync has been destroyed.
    if (m_framesync_instance)
    {
        NDIlib_framesync_destroy(m_framesync_instance);
        m_framesync_instance = nullptr;
    }

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

    if (!(m_framesync_instance = NDIlib_framesync_create(m_receiver_instance)))
        throw std::runtime_error("Failed to create NDI framesync instance");
}

void NDISourceWindow::receive()
{
    NDIlib_video_frame_v2_t video_frame{};
    NDIlib_framesync_capture_video(m_framesync_instance, &video_frame);

    // With framesync, it's possible (and likely) we'll get the same frame multiple times. Don't update the texture if
    // the frame hasn't changed.
    //
    // If we have not received even a single frame yet, NDI says:
    // "this will return NDIlib_video_frame_v2_t as an empty (all zero) structure"
    // So, check for p_data to be something first before checking the timecode.
    if (video_frame.p_data && video_frame.timecode != m_frame_timecode)
    {
        m_frame_texture.with_bound([&video_frame]() {
            JMP::GL::Texture2D::set_data(0, GL_RGBA, video_frame.xres, video_frame.yres, GL_RGBA, GL_UNSIGNED_BYTE,
                                         video_frame.p_data);
        });

        m_frame_timecode = video_frame.timecode;
    }

    NDIlib_framesync_free_video(m_framesync_instance, &video_frame);
}

void NDISourceWindow::set_frame_texture_filtering(GLint filtering)
{
    m_frame_texture.with_bound([filtering]() {
        JMP::GL::Texture2D::set_parameter(GL_TEXTURE_MIN_FILTER, filtering);
        JMP::GL::Texture2D::set_parameter(GL_TEXTURE_MAG_FILTER, filtering);
    });
}
}