#pragma once
#include "types.h"

#include "bootstrap.h"
#include "structs.h"
#include "initializers.h"

class Renderer{
public:
    GLFWwindow* _window;
    VkSurfaceKHR _surface;

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;

    VkPhysicalDevice _physicalDevice;
    VkDevice _device;

    VmaAllocator _allocator;

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    FrameData _frames[FRAME_OVERLAP];
    uint32_t _frameNumber;

    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    VkExtent2D _swapchainExtent;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    bool frameBufferResized;

    AllocatedImage _drawImage;
    AllocatedImage _depthImage;

    void init(){
        setupWindow();
        setupVulkan();
        setupSwapchain();
        setupCommandResources();
        setupSyncStructures();
    }

    void run(){
        double totalFrameTime = 0.0f;
        int frameCount = 0;
        auto startTime = std::chrono::high_resolution_clock::now();

        while(!glfwWindowShouldClose(_window)){
            auto frameStartTime = std::chrono::high_resolution_clock::now();

            glfwPollEvents();

            draw();

            auto frameEndTime = std::chrono::high_resolution_clock::now();
            std::chrono::duration<double, std::milli> frameDuration = frameEndTime - frameStartTime;

            totalFrameTime += frameDuration.count();
            frameCount++;
        }

        auto endTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<double> elapsedSeconds = endTime - startTime;
        double avgFrameTime = elapsedSeconds.count() / frameCount;
        double fps = 1.0 / avgFrameTime;

        fmt::println("Total elapsed time: {}s", elapsedSeconds.count());
        fmt::println("Total frames: {}", frameCount);
        fmt::println("Average frame time: {}ms", avgFrameTime*1000.0f);
        fmt::println("Average FPS: {}", fps);
    }

    void cleanup(){
        vkDeviceWaitIdle(_device);

        for (size_t i = 0; i < FRAME_OVERLAP; i++)
        {
            vkDestroyCommandPool(_device, _frames[i].commandPool, nullptr);

            vkDestroyFence(_device, _frames[i].renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i].renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i].swapchainSemaphore, nullptr);
        }

        cleanupSwapchain();
        _mainDeletionQueue.flush();
        

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);
        destroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
        vkDestroyInstance(_instance, nullptr);
        
        cleanupWindow();
    }

private:
    DeletionQueue _mainDeletionQueue;
    DeletionQueue _swapchainDeletionQueue;

    void draw(){
        if(frameBufferResized) {
            frameBufferResized = false;
            recreateSwapChain();
        }
    }

    void setupCommandResources(){
        VkCommandPoolCreateInfo createInfo = Initializers::commandPoolCreateInfo(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        for (size_t i = 0; i < FRAME_OVERLAP; i++)
        {
            VK_CHECK(vkCreateCommandPool(_device, &createInfo, nullptr, &_frames[i].commandPool));

            VkCommandBufferAllocateInfo allocInfo = Initializers::commandBufferAllocateInfo(_frames[i].commandPool, 1);

            VK_CHECK(vkAllocateCommandBuffers(_device, &allocInfo, &_frames[i].mainCommandBuffer));

            // _mainDeletionQueue.pushFunction([&](){
            //     vkDestroyCommandPool(_device, _frames[i].commandPool, nullptr);
            // });
        }
    }

    void setupSyncStructures(){
        VkFenceCreateInfo fenceCreateInfo = Initializers::fenceCreateInfo(VK_FENCE_CREATE_SIGNALED_BIT);

        VkSemaphoreCreateInfo semaphoreCreateInfo = Initializers::semaphoreCreateInfo();

        for (size_t i = 0; i < FRAME_OVERLAP; i++)
        {
            VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_frames[i].renderFence));

            VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].swapchainSemaphore));
            VK_CHECK(vkCreateSemaphore(_device, &semaphoreCreateInfo, nullptr, &_frames[i].renderSemaphore));

            // _mainDeletionQueue.pushFunction([&](){
            //     vkDestroyFence(_device, _frames[i].renderFence, nullptr);

            //     vkDestroySemaphore(_device, _frames[i].swapchainSemaphore, nullptr);
            //     vkDestroySemaphore(_device, _frames[i].renderSemaphore, nullptr);
            // });
        }
    }

    void setupSwapchain(){
        createSwapchain();
    // Create Draw Image
        VkExtent3D drawImageExent = {
            _swapchainExtent.width,
            _swapchainExtent.height,
            1
        };

        _drawImage.imageFormat = VK_FORMAT_R16G16B16A16_SFLOAT;
        _drawImage.imageExtent = drawImageExent;

        VkImageUsageFlags drawImageUsage{};
        drawImageUsage |= VK_IMAGE_USAGE_TRANSFER_SRC_BIT;
        drawImageUsage |= VK_IMAGE_USAGE_TRANSFER_DST_BIT;
        drawImageUsage |= VK_IMAGE_USAGE_STORAGE_BIT;
        drawImageUsage |= VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

        VkImageCreateInfo rimageInfo = Initializers::imageCreateInfo(_drawImage.imageFormat, drawImageUsage, drawImageExent);

        VmaAllocationCreateInfo rimageAllocInfo{};
        rimageAllocInfo.usage = VMA_MEMORY_USAGE_GPU_ONLY;
        rimageAllocInfo.requiredFlags = VkMemoryPropertyFlags(VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

        VK_CHECK(vmaCreateImage(_allocator, &rimageInfo, &rimageAllocInfo, &_drawImage.image, &_drawImage.allocation, nullptr));

        VkImageViewCreateInfo rviewInfo = Initializers::imageViewCreateInfo(_drawImage.imageFormat, _drawImage.image, VK_IMAGE_ASPECT_COLOR_BIT);

        VK_CHECK(vkCreateImageView(_device, &rviewInfo, nullptr, &_drawImage.imageView));
    
    // Create Depth Image
        _depthImage.imageFormat = VK_FORMAT_D32_SFLOAT;
        _depthImage.imageExtent = drawImageExent;

        VkImageUsageFlags depthImageUsages{};
        depthImageUsages |= VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;

        VkImageCreateInfo dImageInfo = Initializers::imageCreateInfo(_depthImage.imageFormat, depthImageUsages, drawImageExent);

        VK_CHECK(vmaCreateImage(_allocator, &dImageInfo, &rimageAllocInfo, &_depthImage.image, &_depthImage.allocation, nullptr));

        VkImageViewCreateInfo dviewInfo = Initializers::imageViewCreateInfo(_depthImage.imageFormat, _depthImage.image, VK_IMAGE_ASPECT_DEPTH_BIT);

        VK_CHECK(vkCreateImageView(_device, &dviewInfo, nullptr, &_depthImage.imageView));

        _swapchainDeletionQueue.pushFunction([&](){
            vkDestroyImageView(_device, _drawImage.imageView, nullptr);
            vmaDestroyImage(_allocator, _drawImage.image, _drawImage.allocation);

            vkDestroyImageView(_device, _depthImage.imageView, nullptr);
            vmaDestroyImage(_allocator, _depthImage.image, _depthImage.allocation);
        });

    }

    void createSwapchain(){
        Bootstrap::SwapchainBuilder builder;

        builder.setUsageFlags(VkImageUsageFlags(VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT));

        SwapChainInfomation scI = builder.setupSwapChain(_physicalDevice, _surface, _device, _window);

        _swapchain = scI.swapchain;
        _swapchainExtent = scI.swapchainExtent;
        _swapchainImageFormat = scI.swapchainImageFormat;
        _swapchainImages = scI.swapchainImages;
        _swapchainImageViews = scI.swapchainImageViews;
    }

    void recreateSwapChain(){
        int width = 0, height = 0;
        glfwGetFramebufferSize(_window, &width, &height);
        while(width == 0 || height == 0){
            glfwGetFramebufferSize(_window, &width, &height);
            glfwWaitEvents();
        }

        vkDeviceWaitIdle(_device);

        cleanupSwapchain();

        // createSwapChain(_physicalDevice, surface, device, window);
        setupSwapchain();
        // depthBuffer.setupDepthResources(device, physicalDevice, swapChainExtent);
        // createFrameBuffer(device, depthBuffer.depthImageView, renderPass);
    }

    void cleanupSwapchain(){
        _swapchainDeletionQueue.flush();
        vkDestroySwapchainKHR(_device, _swapchain, nullptr);

        for (size_t i = 0; i < _swapchainImageViews.size(); i++)
        {
            vkDestroyImageView(_device, _swapchainImageViews[i], nullptr);
        }
    }

    void setupVulkan(){
        // vkb::Instance vkbInstance = setupInsatnceAndDebugMessenger();
        setupInstanceAndDebug();
        setupSurface();
        // setupPhysicalDevice(vkbInstance);
        setupPhysicalDevice();

        VmaAllocatorCreateInfo allocInfo{};
        allocInfo.physicalDevice = _physicalDevice;
        allocInfo.device = _device;
        allocInfo.instance = _instance;
        allocInfo.flags = VMA_ALLOCATOR_CREATE_BUFFER_DEVICE_ADDRESS_BIT;
        vmaCreateAllocator(&allocInfo, &_allocator);

        _mainDeletionQueue.pushFunction([&](){
            vmaDestroyAllocator(_allocator);
        });
    }

    void setupInstanceAndDebug(){
        Bootstrap::InstanceBuilder builder;
        builder.setApplicationName("Renderer");
        builder.setApiVersion(VK_API_VERSION_1_3);
        builder.requestValidationLayers(USE_VALIDATION_LAYERS);

        BootstrapInstance bi = builder.build();
        _instance = bi.instance;
        _debugMessenger = bi.debugMessenger;

        return;
    }

    void setupPhysicalDevice(){
        VkPhysicalDeviceVulkan13Features features13{};
        features13.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_3_FEATURES;
        features13.dynamicRendering = VK_TRUE;
        features13.synchronization2 = VK_TRUE;

        VkPhysicalDeviceVulkan12Features features12{};
        features12.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_2_FEATURES;
        features12.bufferDeviceAddress = VK_TRUE;
        features12.descriptorIndexing = VK_TRUE;

        VkPhysicalDeviceVulkan11Features features11{};
        features11.sType = VK_STRUCTURE_TYPE_PHYSICAL_DEVICE_VULKAN_1_1_FEATURES;

        Bootstrap::DeviceBuilder selector;
        selector.setPhysicalDeviceVulkan12Features(features12);
        selector.setPhysicalDeviceVulkan13Features(features13);

        BootstrapDevice bd = selector.build(_instance, _surface);

        _device = bd.device;
        _physicalDevice = bd.physicalDevice;

        _graphicsQueue = bd.graphicsQueue;
        _graphicsQueueFamily = bd.graphicsQueueFamily;
    }

    void setupWindow(){
        glfwInit();

        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        glfwWindowHint(GLFW_RESIZABLE, GLFW_TRUE);

        _window = glfwCreateWindow(WIDTH, HEIGHT, "Vulkan Renderer", nullptr, nullptr);

        // Set Resize Callback
        glfwSetWindowUserPointer(_window, this);
        glfwSetFramebufferSizeCallback(_window, frameBufferResizeCallback);
    }

    static void frameBufferResizeCallback(GLFWwindow* window, int width, int heigh){
            auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
            app->frameBufferResized = true;
        }

    void setupSurface(){
        if(glfwCreateWindowSurface(_instance, _window, nullptr, &_surface) != VK_SUCCESS){
            fmt::println("Failed to create window surface!");
        }
    }

    void destroyDebugUtilsMessengerEXT(VkInstance instance, VkDebugUtilsMessengerEXT debugMessenger, const VkAllocationCallbacks* pAllocator) {
        auto func = (PFN_vkDestroyDebugUtilsMessengerEXT) vkGetInstanceProcAddr(instance, "vkDestroyDebugUtilsMessengerEXT");

        if(func != nullptr) {
            func(instance, debugMessenger, pAllocator);
        }
    }

    void cleanupWindow(){
        glfwDestroyWindow(_window);
        glfwTerminate();
    }
};