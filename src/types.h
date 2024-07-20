#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <fmt/format.h>

#include <chrono>

#include <VkBootstrap.h>
#include <optional>

#include <vector>

#include <deque>
#include <functional>

// #define VMA_IMPLEMENTATION
// #include "vk_mem_alloc.h"

const uint32_t WIDTH = 800, HEIGHT = 600;

const bool USE_VALIDATION_LAYERS = 2;

const uint32_t FRAME_OVERLAP = 2;

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)
