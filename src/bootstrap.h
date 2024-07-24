#include "types.h"
#include "structs.h"
#include "initializers.h"

#include <iostream>
#include <set>
#include <string>

namespace Bootstrap{
    class InstanceBuilder{
    public:
        const char* appName = "Default Name";
        uint32_t apiVersion = VK_API_VERSION_1_3;
        bool enableValidationLayers;

        void setApplicationName(std::string newName) {
            appName = newName.c_str();
        }

        void setApiVersion(uint32_t newVersion){
            apiVersion = newVersion;
        }

        void requestValidationLayers(bool useLayers){
            enableValidationLayers = useLayers;
        }

        BootstrapInstance build(){
            BootstrapInstance bi{};
            bi.instance = buildInstance();
            if(enableValidationLayers){
                bi.debugMessenger = buildDebugMessenger(bi.instance);
            }

            return bi;
        }
    
    private:
        VkInstance buildInstance(){
            VkInstance instance;

            if (enableValidationLayers && !checkValidationLayerSupport()) {
                throw std::runtime_error("validation layers requested, but not available!");
            }

            VkApplicationInfo appInfo{};

            appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            appInfo.pApplicationName = appName;
            appInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.pEngineName = "No Engine";
            appInfo.engineVersion = VK_MAKE_VERSION(1, 0, 0);
            appInfo.apiVersion = apiVersion;

            VkInstanceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            createInfo.pApplicationInfo = &appInfo;

            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;

            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            createInfo.enabledExtensionCount = glfwExtensionCount;
            createInfo.ppEnabledExtensionNames = glfwExtensions;

            VkDebugUtilsMessengerCreateInfoEXT debugCreateInfo{};

            if(enableValidationLayers){
                createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
                createInfo.ppEnabledLayerNames = validationLayers.data();

                populateDebugMessengerCreateInfo(debugCreateInfo);
                createInfo.pNext = (VkDebugUtilsMessengerCreateInfoEXT*) &debugCreateInfo;
            } else {
                createInfo.enabledLayerCount = 0;
            }

            auto extensions = getRequiredExtensions();
            createInfo.enabledExtensionCount = static_cast<uint32_t>(extensions.size());
            createInfo.ppEnabledExtensionNames = extensions.data();

            VkResult result = vkCreateInstance(&createInfo, nullptr, &instance);

            if(result != VK_SUCCESS) {
                throw std::runtime_error("Failed to create Instance");
            }

            return instance;
        }

        std::vector<const char*> getRequiredExtensions() {
            uint32_t glfwExtensionCount = 0;
            const char** glfwExtensions;

            glfwExtensions = glfwGetRequiredInstanceExtensions(&glfwExtensionCount);

            std::vector<const char*> extensions(glfwExtensions, glfwExtensions + glfwExtensionCount);

            if(enableValidationLayers) {
                extensions.push_back(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
            }

            return extensions;
        }

        bool checkValidationLayerSupport() {
            uint32_t layerCount;
            VkResult result = vkEnumerateInstanceLayerProperties(&layerCount, nullptr);

            if (result != VK_SUCCESS) {
                std::cout << "Failed to enumerate instance layer properties: " << result << std::endl;
                return false;
            }

            std::vector<VkLayerProperties> availableLayers(layerCount);

            result = vkEnumerateInstanceLayerProperties(&layerCount, availableLayers.data());
            if (result != VK_SUCCESS) {
                std::cout << "Failed to enumerate instance layer properties with data: " << result << std::endl;
                return false;
            }

            for (const char* layerName: validationLayers) {
                bool layerFound = false;

                for(const auto& layerProp: availableLayers) {
                    if(strcmp(layerName, layerProp.layerName) == 0) {
                        layerFound = true;
                        break;
                    }
                }

                if (!layerFound) {
                    std::cout << "Validation layer not found: " << layerName << std::endl;
                    return false;
                }
            }

            return true;
        }

        VkDebugUtilsMessengerEXT buildDebugMessenger(VkInstance instance) {
            VkDebugUtilsMessengerEXT debugMessenger;

            // if (!enableValidationLayers) return debugMessenger;

            VkDebugUtilsMessengerCreateInfoEXT createInfo{};

            populateDebugMessengerCreateInfo(createInfo);

            if (CreateDebugUtilsMessengerEXT(instance, &createInfo, nullptr, &debugMessenger) != VK_SUCCESS) {
                throw std::runtime_error("Failed to set up debug messenger!");
            }

            return debugMessenger;
        }

        static void populateDebugMessengerCreateInfo(VkDebugUtilsMessengerCreateInfoEXT& createInfo) {
            createInfo = {};
            createInfo.sType = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
            createInfo.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT 
                                        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT 
                                        | VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;
            createInfo.messageType = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT 
                                    | VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT 
                                    | VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;
            createInfo.pfnUserCallback = debugCallback;
            createInfo.pUserData = nullptr;
        }

        // void cleanupDebugMessenger(VkInstance instance) {
        //     if(enableValidationLayers) {
        //         DestroyDebugUtilsMessengerEXT(instance, debugMessenger, nullptr);
        //     }
        // }

        VkResult CreateDebugUtilsMessengerEXT(
            VkInstance instance,
            const VkDebugUtilsMessengerCreateInfoEXT* pCreateInfo,
            const VkAllocationCallbacks* pAllocator,
            VkDebugUtilsMessengerEXT* pDebugMessenger) 
        {
            auto func = (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(
                instance,
                "vkCreateDebugUtilsMessengerEXT"
            );

            if(func != nullptr) {
                return func(instance, pCreateInfo, pAllocator, pDebugMessenger);
            } else {
                return VK_ERROR_EXTENSION_NOT_PRESENT;
            }
        }

        static VKAPI_ATTR VkBool32 VKAPI_CALL debugCallback(
            VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData
        ) {
            std::cout << "validation layer: " << pCallbackData->pMessage << std::endl;

            return VK_FALSE;
        }

        void DestroyDebugUtilsMessengerEXT(VkInstance instance, 
        VkDebugUtilsMessengerEXT debugMessenger,
        const VkAllocationCallbacks* pAllocator) {
                auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

                if(func != nullptr) {
                    func(instance, debugMessenger, pAllocator);
                }
        }
    };

    class DeviceBuilder{
    public:
        VkPhysicalDeviceVulkan11Features features11{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES};
        VkPhysicalDeviceVulkan12Features features12{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES};
        VkPhysicalDeviceVulkan13Features features13{VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES};

        void setPhysicalDeviceVulkan11Features(VkPhysicalDeviceVulkan11Features newFeatures11){
            features11 = newFeatures11;
        }

        void setPhysicalDeviceVulkan12Features(VkPhysicalDeviceVulkan12Features newFeatures12){
            features12 = newFeatures12;
        }

        void setPhysicalDeviceVulkan13Features(VkPhysicalDeviceVulkan13Features newFeatures13){
            features13 = newFeatures13;
        }

        BootstrapDevice build(VkInstance instance, VkSurfaceKHR surface){
            VkPhysicalDevice physicalDevice = pickPhysicalDevice(instance, surface);

            return createLogicalDevice(physicalDevice, surface);
        }

    private:
        QueueFamilyIndices indices;

        VkPhysicalDevice pickPhysicalDevice(VkInstance instance, VkSurfaceKHR surface) {
            VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;

            uint32_t deviceCount = 0;
            vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);

            if (deviceCount == 0) {
                throw std::runtime_error("failed to find GPUs that support Vulkan");
            }

            std::vector<VkPhysicalDevice> devices(deviceCount);
            vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());

            for (const auto& device: devices) {
                if (isDeviceSuitable(device, surface)) {
                    physicalDevice = device;
                    break;
                }
            }

            if (physicalDevice == VK_NULL_HANDLE) {
                throw std::runtime_error("Failed to find a suitable GPU");
            }

            return physicalDevice;
        }

        BootstrapDevice createLogicalDevice(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
            VkDevice device;
            VkQueue graphicsQueue;

            QueueFamilyIndices indices = QueueFamilyIndices::findQueueFamilies(physicalDevice, surface);

            std::vector<VkDeviceQueueCreateInfo> queueCreateInfos;
            std::set<uint32_t> uniqueuQueueFamilies =  {indices.graphicsFamily.value(), indices.presentFamily.value()};

            float queuePriority = 1.0f;
            
            for (uint32_t queueFamily : uniqueuQueueFamilies){
                VkDeviceQueueCreateInfo queueCreateInfo{};
                queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                queueCreateInfo.queueFamilyIndex = indices.graphicsFamily.value();
                queueCreateInfo.queueCount = 1;
                queueCreateInfo.pQueuePriorities = &queuePriority;
                queueCreateInfos.push_back(queueCreateInfo);
            }

            VkPhysicalDeviceFeatures deviceFeatures{};
            deviceFeatures.samplerAnisotropy = VK_TRUE;
            deviceFeatures.fillModeNonSolid = VK_TRUE;

            features11.pNext = &features12;
            features12.pNext = &features13;

            VkPhysicalDeviceFeatures2 physicalDeviceFeatures2{ VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_FEATURES_2 };
            physicalDeviceFeatures2.features = deviceFeatures;
            physicalDeviceFeatures2.pNext = &features11;

            VkDeviceCreateInfo createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            
            createInfo.queueCreateInfoCount = static_cast<uint32_t>(queueCreateInfos.size());
            createInfo.pQueueCreateInfos = queueCreateInfos.data();

            createInfo.pEnabledFeatures = nullptr;
            createInfo.pNext = &physicalDeviceFeatures2;

            createInfo.enabledExtensionCount = static_cast<uint32_t>(deviceExtensions.size());
            createInfo.ppEnabledExtensionNames = deviceExtensions.data();

            if(USE_VALIDATION_LAYERS) {
                createInfo.enabledLayerCount = static_cast<uint32_t>(validationLayers.size());
                createInfo.ppEnabledLayerNames = validationLayers.data();
            } else {
                createInfo.enabledLayerCount = 0;
            }

        
            if (vkCreateDevice(physicalDevice, &createInfo, nullptr, &device) != VK_SUCCESS) {
                throw std::runtime_error("Failed to create logical device");
            }

            vkGetDeviceQueue(device, indices.graphicsFamily.value(), 0, &graphicsQueue);

            BootstrapDevice bd;
            bd.device = device;
            bd.physicalDevice = physicalDevice;
            bd.graphicsQueue = graphicsQueue;
            bd.graphicsQueueFamily = indices.graphicsFamily.value();

            return bd;
        }

        bool isDeviceSuitable(VkPhysicalDevice device, VkSurfaceKHR surface) {
            indices = QueueFamilyIndices::findQueueFamilies(device, surface);
            bool extensionsSupported = checkDeviceExtensionSupport(device);
            bool swapChainAdequate = false;

            if(extensionsSupported) {
                SwapChainSupportDetails swapChainSupport = SwapChainSupportDetails::querySwapChainSupport(device, surface);
                swapChainAdequate = !swapChainSupport.formats.empty() && !swapChainSupport.presentModes.empty();
            }

            VkPhysicalDeviceFeatures supportedFeatures;
            vkGetPhysicalDeviceFeatures(device, &supportedFeatures);

            return indices.isComplete() && extensionsSupported && swapChainAdequate && supportedFeatures.samplerAnisotropy;
        }

        bool checkDeviceExtensionSupport(VkPhysicalDevice device){ 
            uint32_t extensionCount;
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, nullptr);

            std::vector<VkExtensionProperties> availableExtensions(extensionCount);
            vkEnumerateDeviceExtensionProperties(device, nullptr, &extensionCount, availableExtensions.data());

            std::set<std::string> requiredExtensions(deviceExtensions.begin(), deviceExtensions.end());
            // requiredExtensions.insert("")

            for (const auto& extension: availableExtensions) {
                requiredExtensions.erase(extension.extensionName);
            }

            return requiredExtensions.empty();
        }
    };

    class SwapchainBuilder{
        VkSwapchainKHR swapChain;
        std::vector<VkImage> swapChainImages;
        VkFormat swapChainImageFormat;
        VkExtent2D swapChainExtent;
        std::vector<VkImageView> swapChainImageViews;
        std::vector<VkFramebuffer> swapChainFrameBuffers;

        VkImageUsageFlags imageUsage;
    public:

        SwapChainInfomation setupSwapChain(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device, GLFWwindow* window){
            createSwapChain(physicalDevice, surface, device, window);
            createImageViews(device);

            SwapChainInfomation scI;
            scI.swapchain = swapChain;
            scI.swapchainExtent = swapChainExtent;
            scI.swapchainImageFormat = swapChainImageFormat;
            scI.swapchainImages = swapChainImages;
            scI.swapchainImageViews = swapChainImageViews;

            return scI;
        }

        void setupFrameBuffer(VkDevice device, VkImageView depthImageView, VkRenderPass renderPass){
            createFrameBuffer(device, depthImageView, renderPass);
        }

        void setUsageFlags(VkImageUsageFlags newFlags){
            imageUsage = newFlags;
        }

        /*
        // void recreateSwapChain(VkDevice device, VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, GLFWwindow* window, DepthBuffer& depthBuffer, VkRenderPass renderPass){
        //     int width = 0, height = 0;
        //     glfwGetFramebufferSize(window, &width, &height);
        //     while(width == 0 || height == 0){
        //         glfwGetFramebufferSize(window, &width, &height);
        //         glfwWaitEvents();
        //     }

        //     vkDeviceWaitIdle(device);

        //     cleanupSwapChain(device, depthBuffer);

        //     createSwapChain(physicalDevice, surface, device, window);
        //     createImageViews(device);
        //     depthBuffer.setupDepthResources(device, physicalDevice, swapChainExtent);
        //     createFrameBuffer(device, depthBuffer.depthImageView, renderPass);
        // }

        // void cleanupSwapChain(VkDevice device, DepthBuffer& depthBuffer){
        //     depthBuffer.cleanupDepthResources(device);

        //     for(size_t i = 0; i < swapChainFrameBuffers.size(); i++){
        //         vkDestroyFramebuffer(device, swapChainFrameBuffers[i], nullptr);
        //     }

        //     for(size_t i = 0; i < swapChainImageViews.size(); i++){
        //         vkDestroyImageView(device, swapChainImageViews[i], nullptr);
        //     }

        //     vkDestroySwapchainKHR(device, swapChain, nullptr);
        // }
        */
    
    private:
        void createSwapChain(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface, VkDevice device, GLFWwindow* window){
            SwapChainSupportDetails swapChainSupport = SwapChainSupportDetails::querySwapChainSupport(physicalDevice, surface);

            VkSurfaceFormatKHR surfaceFormat = chooseSwapSurfaceFormat(swapChainSupport.formats);
            VkPresentModeKHR presentMode = chooseSwapPresentMode(swapChainSupport.presentModes);
            VkExtent2D extent = chooseSwapExtent(swapChainSupport.capabilities, window);

            uint32_t imageCount = swapChainSupport.capabilities.minImageCount + 1;

            if(swapChainSupport.capabilities.maxImageCount > 0 && imageCount > swapChainSupport.capabilities.maxImageCount) {
                imageCount = swapChainSupport.capabilities.maxImageCount;
            }

            VkSwapchainCreateInfoKHR createInfo{};
            createInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            createInfo.surface = surface;

            createInfo.minImageCount = imageCount;
            createInfo.imageFormat = surfaceFormat.format;
            createInfo.imageColorSpace = surfaceFormat.colorSpace;
            createInfo.imageExtent = extent;
            createInfo.imageArrayLayers = 1;
            createInfo.imageUsage = imageUsage;
            // createInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            QueueFamilyIndices indices = QueueFamilyIndices::findQueueFamilies(physicalDevice, surface);
            uint32_t queueFamilyIndices[] = {indices.graphicsFamily.value(), indices.presentFamily.value()};

            if (indices.graphicsFamily != indices.presentFamily){
                createInfo.imageSharingMode = VK_SHARING_MODE_CONCURRENT;
                createInfo.queueFamilyIndexCount = 2;
                createInfo.pQueueFamilyIndices = queueFamilyIndices;
            } else {
                createInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
                createInfo.queueFamilyIndexCount = 0;
                createInfo.pQueueFamilyIndices = nullptr;
            }

            createInfo.preTransform = swapChainSupport.capabilities.currentTransform;

            createInfo.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;

            createInfo.presentMode = presentMode;
            createInfo.clipped = VK_TRUE;
            createInfo.oldSwapchain = VK_NULL_HANDLE;

            if (vkCreateSwapchainKHR(device, &createInfo, nullptr, &swapChain) != VK_SUCCESS) {
                throw std::runtime_error("failed to create swap chain!");
            }

            vkGetSwapchainImagesKHR(device, swapChain, &imageCount, nullptr);
            swapChainImages.resize(imageCount);
            vkGetSwapchainImagesKHR(device, swapChain, &imageCount, swapChainImages.data());

            swapChainImageFormat = surfaceFormat.format;
            swapChainExtent = extent;
        }

        VkSurfaceFormatKHR chooseSwapSurfaceFormat(const std::vector<VkSurfaceFormatKHR>& availableFormats){
            for (const auto& availableFormat: availableFormats) {
                if (availableFormat.format == VK_FORMAT_B8G8R8A8_SRGB &&
                availableFormat.colorSpace == VK_COLOR_SPACE_SRGB_NONLINEAR_KHR
                ) {
                    return availableFormat;
                }
            }

            return availableFormats[0];
        }

        VkPresentModeKHR chooseSwapPresentMode(const std::vector<VkPresentModeKHR>& availablePresentModes) {
            for (const auto& availablePresentMode: availablePresentModes) {
                if (availablePresentMode == VK_PRESENT_MODE_MAILBOX_KHR) {
                    return availablePresentMode;
                }
            }
            
            return VK_PRESENT_MODE_FIFO_KHR;
        }

        VkExtent2D chooseSwapExtent(const VkSurfaceCapabilitiesKHR& capabilities, GLFWwindow* window){
            if (capabilities.currentExtent.width != std::numeric_limits<uint32_t>::max()) {
                return capabilities.currentExtent;
            } 

            int width, height;
            glfwGetFramebufferSize(window, &width, &height);

            VkExtent2D actualExtent = {
                static_cast<uint32_t>(width),
                static_cast<uint32_t>(height)
            };

            actualExtent.width = std::clamp(actualExtent.width, capabilities.minImageExtent.width, capabilities.maxImageExtent.width);
            actualExtent.height = std::clamp(actualExtent.height, capabilities.minImageExtent.height, capabilities.maxImageExtent.height);

            return actualExtent;
        }

        void createImageViews(VkDevice device){
            swapChainImageViews.resize(swapChainImages.size());

            for (size_t i = 0; i < swapChainImages.size(); i++){
                VkImageViewCreateInfo createInfo = Initializers::imageViewCreateInfo(swapChainImageFormat, swapChainImages[i], VK_IMAGE_ASPECT_COLOR_BIT);

                VK_CHECK(vkCreateImageView(device, &createInfo, nullptr, &swapChainImageViews[i]));
                // swapChainImageViews[i] = Image::createImageView1(swapChainImages[i], swapChainImageFormat, VK_IMAGE_ASPECT_COLOR_BIT, device);
            }
        }

        void createFrameBuffer(VkDevice device, VkImageView depthImageView, VkRenderPass renderPass){
            swapChainFrameBuffers.resize(swapChainImageViews.size());

            for(size_t i = 0; i < swapChainImageViews.size(); i++){
                std::array<VkImageView, 2> attachments = {
                    swapChainImageViews[i],
                    depthImageView
                };

                VkFramebufferCreateInfo frameBufferInfo{};
                frameBufferInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                frameBufferInfo.renderPass = renderPass;
                frameBufferInfo.attachmentCount = static_cast<uint32_t>(attachments.size());;
                frameBufferInfo.pAttachments = attachments.data();
                frameBufferInfo.width = swapChainExtent.width;
                frameBufferInfo.height = swapChainExtent.height;
                frameBufferInfo.layers = 1;

                if (vkCreateFramebuffer(device, &frameBufferInfo, nullptr, &swapChainFrameBuffers[i]) != VK_SUCCESS) {
                    throw std::runtime_error("failed to create framebuffer!");
                }
            }
        }
    };
};