#pragma once

#define GLFW_INCLUDE_VULKAN
#include <GLFW/glfw3.h>
#include <vulkan/vk_enum_string_helper.h>

#define GLM_FORCE_RADIANS
#define GLM_FORCE_DEPTH_ZERO_TO_ONE
#define GLM_ENABLE_EXPERIMENTAL
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtx/string_cast.hpp> // TO PRINT

#include <fmt/format.h>

#include <chrono>

#include <VkBootstrap.h>
#include <optional>

#include <vector>

#include <deque>
#include <functional>
#include <span>

#define VMA_IMPLEMENTATION
#include "vk_mem_alloc.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

const uint32_t WIDTH = 800, HEIGHT = 600;

const bool USE_VALIDATION_LAYERS = true;

const uint32_t FRAME_OVERLAP = 2;

const std::vector<const char*> validationLayers = {
    "VK_LAYER_KHRONOS_validation"
};

const std::vector<const char*> deviceExtensions = {
    VK_KHR_SWAPCHAIN_EXTENSION_NAME
    // VK_EXT_DESCRIPTOR_INDEXING_EXTENSION_NAME
    // VK_EXT_BUFFER_DEVICE_ADDRESS_EXTENSION_NAME
};

#define VK_CHECK(x)                                                     \
    do {                                                                \
        VkResult err = x;                                               \
        if (err) {                                                      \
            fmt::println("Detected Vulkan error: {}", string_VkResult(err)); \
            abort();                                                    \
        }                                                               \
    } while (0)
