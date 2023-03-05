/*
 * Copyright (c) 2023, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <glad/gl.h>

#include "Application.h"
#include <GLFW/glfw3.h>
#include <algorithm>
#include <cstdio>
#include <imgui/backends/imgui_impl_glfw.h>
#include <imgui/backends/imgui_impl_opengl3.h>
#include <imgui/imgui.h>
#include <stdexcept>

namespace Carousel
{
Application::Application()
{
    m_playback_device.pUserData = nullptr;
    m_audio_context.pUserData = nullptr;

    JMP::ScopeGuard free_if_error_occurs([this]() {
        if (m_window)
        {
            glfwDestroyWindow(m_window);
            m_window = nullptr;
        }

        if (m_ndi_finder_instance)
        {
            NDIlib_find_destroy(m_ndi_finder_instance);
            m_ndi_finder_instance = nullptr;
        }

        if (m_playback_device.pUserData)
        {
            ma_device_uninit(&m_playback_device);
            m_playback_device.pUserData = nullptr;
        }

        if (m_audio_context.pUserData)
        {
            ma_context_uninit(&m_audio_context);
            m_audio_context.pUserData = nullptr;
        }
    });

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // FIXME: Format error into exception message?
    if (!(m_window = glfwCreateWindow(1280, 720, "Carousel", nullptr, nullptr)))
        throw std::runtime_error("Failed to create GLFW window");

    create_finder();

    glfwMakeContextCurrent(m_window);
    glfwSwapInterval(s_use_vsync);

    if (!gladLoadGL(glfwGetProcAddress))
        throw std::runtime_error("Failed to load GLAD");

    auto audio_context_config = ma_context_config_init();
    // Just so we know if this was successfully initialized or not.
    audio_context_config.pUserData = this;
    if (ma_context_init(nullptr, 0, &audio_context_config, &m_audio_context) != MA_SUCCESS)
        throw std::runtime_error("Failed to initialize miniaudio context");

    ma_device_info* playback_device_infos{};
    ma_uint32 number_of_playback_device_infos{};

    // Note: The pointers returned here stay alive until the next call to this function, or context uninit, so they'll
    // live long enough.
    if (ma_context_get_devices(&m_audio_context, &playback_device_infos, &number_of_playback_device_infos, nullptr,
                               nullptr) != MA_SUCCESS)
        throw std::runtime_error("Failed to get all playback devices from miniaudio");

    m_playback_device_infos = {playback_device_infos, number_of_playback_device_infos};

    if (!initialize_playback_device(nullptr))
        fprintf(stderr, "Failed to initialize default playback device, there will be no audio!\n");

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGui_ImplGlfw_InitForOpenGL(m_window, true);
    ImGui_ImplOpenGL3_Init("#version 330 core");

    auto& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;

    glfwSetFramebufferSizeCallback(m_window,
                                   [](GLFWwindow* window, int width, int height) { glViewport(0, 0, width, height); });

    glClearColor(0.25f, 0.25f, 0.25f, 1.0f);

    free_if_error_occurs.disarm();
}

Application::~Application()
{
    glfwDestroyWindow(m_window);

    if (m_ndi_finder_instance)
    {
        NDIlib_find_destroy(m_ndi_finder_instance);
        m_ndi_finder_instance = nullptr;
    }

    if (m_playback_device.pUserData)
    {
        ma_device_uninit(&m_playback_device);
        m_playback_device.pUserData = nullptr;
    }

    if (m_audio_context.pUserData)
    {
        ma_context_uninit(&m_audio_context);
        m_audio_context.pUserData = nullptr;
    }
}

int Application::run()
{
    while (!glfwWindowShouldClose(m_window))
    {
        uint32_t number_of_found_sources{};
        m_found_ndi_sources = {NDIlib_find_get_current_sources(m_ndi_finder_instance, &number_of_found_sources),
                               static_cast<size_t>(number_of_found_sources)};

        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::DockSpaceOverViewport();

        ImGui::ShowDemoWindow();

        if (ImGui::BeginMainMenuBar())
        {
            if (ImGui::BeginMenu("NDI"))
            {
                if (ImGui::BeginMenu("Sources..."))
                {
                    if (m_found_ndi_sources.empty())
                    {
                        ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f),
                                           "No sources found! Ensure the zeroconf service of your platform is running "
                                           "(Bonjour/Avahi)");
                    }
                    else
                    {
                        for (auto i = 0; i < m_found_ndi_sources.size(); i++)
                        {
                            ImGui::PushID(i);

                            auto& source = m_found_ndi_sources[i];

                            // This will prevent you making multiple windows for the same source. It is a bit
                            // unfortunate, but the other option is to make each window title unique (probably based on
                            // pointer), but then that breaks imgui.ini persistence, which is pretty important to me.
                            auto does_source_have_existing_window = std::none_of(
                                m_ndi_source_windows.begin(), m_ndi_source_windows.end(),
                                [&source](const auto& source_window) { return source_window->source() == source; });

                            if (ImGui::MenuItem(source.p_ndi_name, nullptr, false, does_source_have_existing_window))
                            {
                                printf("Connecting to source %d (%s)\n", i, source.p_ndi_name);

                                try
                                {
                                    m_ndi_source_windows.push_back(std::make_unique<NDISourceWindow>(source));
                                }
                                catch (const std::exception& ex)
                                {
                                    fprintf(stderr, "Failed to create NDI source window: %s\n", ex.what());
                                }
                            }

                            ImGui::PopID();
                        }
                    }

                    ImGui::EndMenu();
                }

                if (ImGui::MenuItem("Restart Finder"))
                    create_finder();

                ImGui::EndMenu();
            }

            if (ImGui::BeginMenu("Audio"))
            {
                ImGui::MenuItem("Focused Window Only", nullptr, &m_only_play_audio_from_focused_window);
                if (ImGui::BeginMenu("Playback Device"))
                {
                    std::string_view playback_device_name(m_playback_device.playback.name);

                    for (auto& playback_device_info : m_playback_device_infos)
                    {
                        auto is_current_playback_device = playback_device_name == playback_device_info.name;

                        // FIXME: This is comparing the device name, which is not the ID! How are you meant to compare
                        //        device IDs with miniaudio? Doesn't seem to be an API for this...
                        if (ImGui::MenuItem(playback_device_info.name, nullptr, is_current_playback_device,
                                            !is_current_playback_device))
                            initialize_playback_device(&playback_device_info);
                    }

                    ImGui::EndMenu();
                }

                ImGui::EndMenu();
            }

            ImGui::EndMainMenuBar();
        }

        {
            std::lock_guard ndi_source_windows_lock(m_ndi_source_windows_mutex);

            for (auto ndi_connection_iterator = m_ndi_source_windows.begin();
                 ndi_connection_iterator != m_ndi_source_windows.end();)
            {
                bool should_remove_source_window;

                try
                {
                    should_remove_source_window = (*ndi_connection_iterator)->update();
                }
                catch (const std::exception& ex)
                {
                    fprintf(stderr, "Threw exception whilst updating source window: %s\n", ex.what());
                    should_remove_source_window = true;
                }

                if (should_remove_source_window)
                    ndi_connection_iterator = m_ndi_source_windows.erase(ndi_connection_iterator);
                else
                    ndi_connection_iterator++;
            }
        }

        ImGui::Render();

        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }

    return 0;
}

void Application::create_finder()
{
    if (m_ndi_finder_instance)
    {
        NDIlib_find_destroy(m_ndi_finder_instance);
        m_ndi_finder_instance = nullptr;
    }

    if (!(m_ndi_finder_instance = NDIlib_find_create_v2()))
        throw std::runtime_error("Failed to create NDI finder instance");
}

bool Application::initialize_playback_device(ma_device_info* device_info)
{
    if (m_playback_device.pUserData)
    {
        ma_device_uninit(&m_playback_device);
        m_playback_device.pUserData = nullptr;
    }

    auto playback_device_config = ma_device_config_init(ma_device_type_playback);
    // We don't have to set the number of channels or sample rate here, the device defaults are better anyway.
    playback_device_config.playback.pDeviceID = !device_info ? nullptr : &device_info->id;
    playback_device_config.playback.format = ma_format_f32;
    playback_device_config.dataCallback = miniaudio_playback_data_callback;
    playback_device_config.pUserData = this;

    if (ma_device_init(nullptr, &playback_device_config, &m_playback_device) == MA_SUCCESS)
        return ma_device_start(&m_playback_device) == MA_SUCCESS;

    return false;
}

void Application::miniaudio_playback_data_callback(ma_device* device, void* output, const void*, ma_uint32 frame_count)
{
    auto& application = *reinterpret_cast<Application*>(device->pUserData);
    auto output_floats = reinterpret_cast<float*>(output);

    std::lock_guard ndi_source_windows_lock(application.m_ndi_source_windows_mutex);
    if (application.m_ndi_source_windows.empty())
        return;

    auto total_number_of_frames_for_all_channels = frame_count * device->playback.channels;
    float samples_for_this_source[total_number_of_frames_for_all_channels];

    for (auto& ndi_source_window : application.m_ndi_source_windows)
    {
        NDIlib_audio_frame_v2_t audio_frame;
        // Even if the source window is muted, we need to consume the capture for the sync... I think.
        // Without this, muting and unmuting the audio rapidly for a few seconds quickly made it desync. It doesn't cost
        // us much to do this anyhow.
        NDIlib_framesync_capture_audio(ndi_source_window->framesync_instance(), &audio_frame,
                                       static_cast<int>(device->playback.internalSampleRate),
                                       static_cast<int>(device->playback.channels), static_cast<int>(frame_count));

        JMP::ScopeGuard free_audio_frame = [&ndi_source_window, &audio_frame]() {
            NDIlib_framesync_free_audio(ndi_source_window->framesync_instance(), &audio_frame);
        };

        if (application.m_only_play_audio_from_focused_window && !ndi_source_window->is_window_focused() ||
            ndi_source_window->is_audio_muted())
            continue;

        NDIlib_audio_frame_interleaved_32f_t audio_frame_interleaved_floats;
        audio_frame_interleaved_floats.no_channels = static_cast<int>(device->playback.channels);
        audio_frame_interleaved_floats.sample_rate = static_cast<int>(device->playback.internalSampleRate);
        audio_frame_interleaved_floats.no_samples = static_cast<int>(frame_count);
        audio_frame_interleaved_floats.p_data = samples_for_this_source;

        NDIlib_util_audio_to_interleaved_32f_v2(&audio_frame, &audio_frame_interleaved_floats);

        for (auto i = 0; i < total_number_of_frames_for_all_channels; i++)
        {
            auto mixed = std::clamp(output_floats[i] + (samples_for_this_source[i] * ndi_source_window->audio_volume()),
                                    -1.0f, 1.0f);
            output_floats[i] = mixed;
        }

        // Note: We can't break early from this loop even if m_only_play_audio_from_focused_window is true, because of
        // the observed potential desync issues observed with NDIlib_framesync_capture_audio described above -- we need
        // to be sure to at least consume the audio from all sources.
    }
}
}