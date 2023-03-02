/*
 * Copyright (c) 2023, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include "NDI.h"
#include "NDISourceWindow.h"
#include <memory>
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

    void create_finder();
};
}