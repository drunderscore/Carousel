/*
 * Copyright (c) 2023, James Puleo <james@jame.xyz>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include "Application.h"
#include "NDI.h"
#include <GLFW/glfw3.h>
#include <stdio.h>
#include <stdlib.h>

int main()
{
    if (!glfwInit())
    {
        const char* error;
        glfwGetError(&error);
        fprintf(stderr, "Failed to initialize GLFW: %s\n", error);
        return EXIT_FAILURE;
    }

    atexit(glfwTerminate);

    if (!NDIlib_initialize())
    {
        fprintf(stderr, "Failed to initialize NDI\n");
        return EXIT_FAILURE;
    }

    atexit(NDIlib_destroy);

    try
    {
        Carousel::Application application;
        return application.run();
    }
    catch (const std::exception& ex)
    {
        fprintf(stderr, "Error occurred during initialization/execution: %s\n", ex.what());
        return EXIT_FAILURE;
    }
}
