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
    JMP::ScopeGuard free_if_error_occurs([this]() {
        if (m_window)
            glfwDestroyWindow(m_window);

        if (m_ndi_finder_instance)
            NDIlib_find_destroy(m_ndi_finder_instance);
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
    NDIlib_find_destroy(m_ndi_finder_instance);
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
            ImGui::EndMainMenuBar();
        }

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
        ImGui::Render();

        glClear(GL_COLOR_BUFFER_BIT);

        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(m_window);
    }

    return 0;
}

void Application::create_finder()
{
    if (!(m_ndi_finder_instance = NDIlib_find_create_v2()))
        throw std::runtime_error("Failed to create NDI finder instance");
}
}