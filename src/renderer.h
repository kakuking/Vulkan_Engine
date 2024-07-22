#pragma once
#include "types.h"

#include "bootstrap.h"
#include "structs.h"
#include "utility.h"
#include "initializers.h"

#include "imgui.h"
#include "imgui_impl_glfw.h"
#include "imgui_impl_vulkan.h"

class Renderer{
public:
    GLFWwindow* _window;
    VkSurfaceKHR _surface;

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;

    VkPhysicalDevice _physicalDevice;
    VkDevice _device;

    VmaAllocator _allocator;

    DescriptorAllocator _globalDescriptorAllocator;

    VkQueue _graphicsQueue;
    uint32_t _graphicsQueueFamily;

    FrameData _frames[FRAME_OVERLAP];
    uint32_t _frameNumber;

    VkSwapchainKHR _swapchain;
    VkFormat _swapchainImageFormat;
    VkExtent2D _swapchainExtent;
    std::vector<VkImage> _swapchainImages;
    std::vector<VkImageView> _swapchainImageViews;

    AllocatedImage _drawImage, _depthImage;
    VkExtent2D _drawExtent;

    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    VkPipeline _backgroundShaderPipeline;
    VkPipelineLayout _backgroundShaderPipelineLayout;

    std::vector<ComputeEffect> _backgroundEffects;
    int _currentBackground{0};

    VkFence _immediateFence;
    VkCommandBuffer _immediateCommandBuffer;
    VkCommandPool _immediateCommandPool;
    
    bool frameBufferResized;

    void init(){
        setupWindow();
        setupVulkan();
        setupSwapchain();
        setupCommandResources();
        setupSyncStructures();
        setupDescriptors();
        setupPipeline();
        setupImgui();
    }

    void run(){
        double totalFrameTime = 0.0f;
        int frameCount = 0;
        auto startTime = std::chrono::high_resolution_clock::now();

        while(!glfwWindowShouldClose(_window)){
            auto frameStartTime = std::chrono::high_resolution_clock::now();
            glfwPollEvents();
            if(frameBufferResized) {
                frameBufferResized = false;
                recreateSwapChain();
            }

            ImGui_ImplGlfw_NewFrame();
            ImGui_ImplVulkan_NewFrame();
            ImGui::NewFrame();
            renderImgui();

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
        _descriptorDeletionQueue.flush();
        

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);
        destroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
        vkDestroyInstance(_instance, nullptr);
        
        cleanupWindow();
    }

private:
    DeletionQueue _mainDeletionQueue;
    DeletionQueue _swapchainDeletionQueue;
    DeletionQueue _descriptorDeletionQueue;

    void draw(){
        VK_CHECK(vkWaitForFences(_device, 1, &getCurrentFrame().renderFence, VK_TRUE, 1000000000));

        getCurrentFrame().deletionQueue.flush();

        VK_CHECK(vkResetFences(_device, 1, &getCurrentFrame().renderFence));

        uint32_t swapchainImageIndex;
        if(vkAcquireNextImageKHR(_device, _swapchain, 1000000000, getCurrentFrame().swapchainSemaphore, nullptr, &swapchainImageIndex) == VK_ERROR_OUT_OF_DATE_KHR){
            frameBufferResized = true;
            return;
        }

        VkCommandBuffer command = getCurrentFrame().mainCommandBuffer;

        VK_CHECK(vkResetCommandBuffer(command, 0));

        _drawExtent.width = _drawImage.imageExtent.width;
        _drawExtent.height = _drawImage.imageExtent.height;

        VkCommandBufferBeginInfo beginInfo = Initializers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(command, &beginInfo));

        Utility::transitionImage(command, _drawImage.image, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_GENERAL);

        drawBackground(command);

        Utility::transitionImage(command, _drawImage.image, VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
        Utility::transitionImage(command, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);

        Utility::copyImageToImage(command, _drawImage.image, _swapchainImages[swapchainImageIndex], _drawExtent, _swapchainExtent);

        Utility::transitionImage(command, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

        drawImgui(command, _swapchainImageViews[swapchainImageIndex]);

        Utility::transitionImage(command, _swapchainImages[swapchainImageIndex], VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_PRESENT_SRC_KHR);

        VK_CHECK(vkEndCommandBuffer(command));

        VkCommandBufferSubmitInfo commandInfo = Initializers::commandBufferSubmitInfo(command);

        VkSemaphoreSubmitInfo waitInfo = Initializers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_COLOR_ATTACHMENT_OUTPUT_BIT_KHR, getCurrentFrame().swapchainSemaphore);
        VkSemaphoreSubmitInfo signalInfo = Initializers::semaphoreSubmitInfo(VK_PIPELINE_STAGE_2_ALL_GRAPHICS_BIT, getCurrentFrame().renderSemaphore);

        VkSubmitInfo2 submitInfo = Initializers::submitInfo(&commandInfo, &signalInfo, &waitInfo);

        VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submitInfo, getCurrentFrame().renderFence));

        VkPresentInfoKHR presentInfo{};
        presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        presentInfo.pNext = nullptr;
        presentInfo.pSwapchains = &_swapchain;
        presentInfo.swapchainCount = 1;

        presentInfo.waitSemaphoreCount = 1;
        presentInfo.pWaitSemaphores = &getCurrentFrame().renderSemaphore;

        presentInfo.pImageIndices = &swapchainImageIndex;
        if(vkQueuePresentKHR(_graphicsQueue, &presentInfo) == VK_ERROR_OUT_OF_DATE_KHR){
            frameBufferResized = true;
        }

        _frameNumber++;
    }

    void drawImgui(VkCommandBuffer command, VkImageView targetImageView){
        VkRenderingAttachmentInfo colorAttachment = Initializers::attachmentInfo(targetImageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingInfo renderInfo = Initializers::renderingInfo(_swapchainExtent, &colorAttachment, nullptr);

        vkCmdBeginRendering(command, &renderInfo);

        ImGui_ImplVulkan_RenderDrawData(ImGui::GetDrawData(), command);

        vkCmdEndRendering(command);
    }

    void drawBackground(VkCommandBuffer command){
        ComputeEffect& effect = _backgroundEffects[_currentBackground];

        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_COMPUTE, effect.pipeline);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_COMPUTE, _backgroundShaderPipelineLayout, 0, 1, &_drawImageDescriptors, 0, nullptr);

        vkCmdPushConstants(command, _backgroundShaderPipelineLayout, VK_SHADER_STAGE_COMPUTE_BIT, 0, sizeof(ComputeShaderPushConstants), &effect.data);
        vkCmdDispatch(command, std::ceil(_drawExtent.width/16.0), std::ceil(_drawExtent.height/16.0), 1);
    }

    void setupPipeline(){
        setupBackgroundPipeline();
    }

    void setupBackgroundPipeline(){
        VkPipelineLayoutCreateInfo computeLayout{};
        computeLayout.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        computeLayout.pNext = nullptr;
        computeLayout.setLayoutCount = 1;
        computeLayout.pSetLayouts = &_drawImageDescriptorLayout;

        VkPushConstantRange pushConstant{};
        pushConstant.offset = 0;
        pushConstant.size = sizeof(ComputeShaderPushConstants);
        pushConstant.stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;

        computeLayout.pushConstantRangeCount = 1;
        computeLayout.pPushConstantRanges = &pushConstant;

        VK_CHECK(vkCreatePipelineLayout(_device, &computeLayout, nullptr, &_backgroundShaderPipelineLayout));
        
        VkShaderModule gradientShader;
        if(!Utility::loadShaderModule("shaders\\gradient.comp.spv", _device, &gradientShader)){
            throw std::runtime_error("Failed to load gradient Shader!");
        }

        VkShaderModule skyShader;
        if(!Utility::loadShaderModule("shaders\\sky.comp.spv", _device, &skyShader)){
            fmt::print("Failed to load sky Shader!");
        }

        VkPipelineShaderStageCreateInfo stageInfo{};
        stageInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
        stageInfo.pNext = nullptr;

        stageInfo.stage = VK_SHADER_STAGE_COMPUTE_BIT;
        stageInfo.module = gradientShader;
        stageInfo.pName = "main";

        VkComputePipelineCreateInfo computePipelineCreateInfo{};
        computePipelineCreateInfo.sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO;
        computePipelineCreateInfo.pNext = nullptr;
        computePipelineCreateInfo.layout = _backgroundShaderPipelineLayout;
        computePipelineCreateInfo.stage = stageInfo;

        ComputeEffect gradient;
        gradient.layout = _backgroundShaderPipelineLayout;
        gradient.name = "gradient";
        gradient.data = {};
        gradient.data.color1 = glm::vec4(1, 1, 0, 1);
        gradient.data.color2 = glm::vec4(0, 0, 1, 1);

        VK_CHECK(vkCreateComputePipelines(_device,VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &gradient.pipeline));

        computePipelineCreateInfo.stage.module = skyShader;

        ComputeEffect sky;
        sky.layout = _backgroundShaderPipelineLayout;
        sky.name = "sky";
        sky.data = {};
        sky.data.color1 = glm::vec4(0.1, 0.2, 0.4 ,0.97);

        VK_CHECK(vkCreateComputePipelines(_device,VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &sky.pipeline));

        _backgroundEffects.push_back(gradient);
        _backgroundEffects.push_back(sky);

        vkDestroyShaderModule(_device, gradientShader, nullptr);
        vkDestroyShaderModule(_device, skyShader, nullptr);
        _mainDeletionQueue.pushFunction([&]() {
            vkDestroyPipelineLayout(_device, _backgroundShaderPipelineLayout, nullptr);
            for (size_t i = 0; i < _backgroundEffects.size(); i++)
            {
                vkDestroyPipeline(_device, _backgroundEffects[i].pipeline, nullptr);
            }            
        });
    }

    void renderImgui(){
        if(ImGui::Begin("Background")) {
            ComputeEffect& selected = _backgroundEffects[_currentBackground];

            ImGui::Text("Selected Effect", selected.name);

            ImGui::SliderInt("Effect Index: ", &_currentBackground, 0, _backgroundEffects.size() - 1);

            _currentBackground = std::clamp(_currentBackground, 0, (int)(_backgroundEffects.size() - 1));

            ImGui::InputFloat4("Color 1", (float*)& selected.data.color1);
            ImGui::InputFloat4("Color 2", (float*)& selected.data.color2);

            if(strcmp(selected.name, "sky") == 0){
                ImGui::InputFloat4("Color 3", (float*)& selected.data.color3);
                ImGui::InputFloat4("Color 4", (float*)& selected.data.color4);
            }
        }

        ImGui::End();
        ImGui::Render();
    }

    void setupDescriptors(){
        std::vector<DescriptorAllocator::PoolSizeRatio> sizeRatios = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
        } ;

        _globalDescriptorAllocator.setupPool(_device, 10, sizeRatios);

        {
            DescriptorLayoutBuilder builder;
            builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
        }

        _drawImageDescriptors = _globalDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

        VkDescriptorImageInfo imageInfo{};
        imageInfo.imageLayout = VK_IMAGE_LAYOUT_GENERAL;
        imageInfo.imageView = _drawImage.imageView;

        VkWriteDescriptorSet drawImageInfo{};
        drawImageInfo.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        drawImageInfo.pNext = nullptr;
        
        drawImageInfo.dstBinding = 0;
        drawImageInfo.dstSet = _drawImageDescriptors;
        drawImageInfo.descriptorCount = 1;
        drawImageInfo.descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_IMAGE;
        drawImageInfo.pImageInfo = &imageInfo;

        vkUpdateDescriptorSets(_device, 1, &drawImageInfo, 0, nullptr);

        _descriptorDeletionQueue.pushFunction([&](){
            _globalDescriptorAllocator.destroyPool(_device);
            vkDestroyDescriptorSetLayout(_device, _drawImageDescriptorLayout, nullptr);
        });
    }

    FrameData& getCurrentFrame() {
        return _frames[_frameNumber % FRAME_OVERLAP];
    }

    void setupCommandResources(){
        VkCommandPoolCreateInfo createInfo = Initializers::commandPoolCreateInfo(_graphicsQueueFamily, VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT);

        for (size_t i = 0; i < FRAME_OVERLAP; i++)
        {
            VK_CHECK(vkCreateCommandPool(_device, &createInfo, nullptr, &_frames[i].commandPool));

            VkCommandBufferAllocateInfo allocInfo = Initializers::commandBufferAllocateInfo(_frames[i].commandPool, 1);

            VK_CHECK(vkAllocateCommandBuffers(_device, &allocInfo, &_frames[i].mainCommandBuffer));
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
        }
    }

    void setupImgui(){
        VkDescriptorPoolSize pool_sizes[] = { { VK_DESCRIPTOR_TYPE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1000 },
		{ VK_DESCRIPTOR_TYPE_SAMPLED_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_TEXEL_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 1000 },
		{ VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_STORAGE_BUFFER_DYNAMIC, 1000 },
		{ VK_DESCRIPTOR_TYPE_INPUT_ATTACHMENT, 1000 } };

        VkDescriptorPoolCreateInfo pool_info = {};
        pool_info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        pool_info.flags = VK_DESCRIPTOR_POOL_CREATE_FREE_DESCRIPTOR_SET_BIT;
        pool_info.maxSets = 1000;
        pool_info.poolSizeCount = (uint32_t)std::size(pool_sizes);
        pool_info.pPoolSizes = pool_sizes;

        VkDescriptorPool imguiPool;
        VK_CHECK(vkCreateDescriptorPool(_device, &pool_info, nullptr, &imguiPool));

        ImGui::CreateContext();
        ImGui_ImplGlfw_InitForVulkan(_window, true);

        ImGui_ImplVulkan_InitInfo initInfo{};
        initInfo.Instance = _instance;
        initInfo.PhysicalDevice = _physicalDevice;
        initInfo.Device = _device;
        initInfo.Queue = _graphicsQueue;
        initInfo.DescriptorPool = imguiPool;
        initInfo.MinImageCount = 3;
        initInfo.ImageCount = 3;
        initInfo.UseDynamicRendering = true;

        initInfo.PipelineRenderingCreateInfo = {.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO};
        initInfo.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        initInfo.PipelineRenderingCreateInfo.pColorAttachmentFormats = &_swapchainImageFormat;

        initInfo.MSAASamples = VK_SAMPLE_COUNT_1_BIT;

        ImGui_ImplVulkan_Init(&initInfo);
        ImGui_ImplVulkan_CreateFontsTexture();

        _mainDeletionQueue.pushFunction([this, imguiPool](){
            ImGui_ImplVulkan_Shutdown();
            vkDestroyDescriptorPool(_device, imguiPool, nullptr);
        });
    }

    void immediateSubmit(std::function<void(VkCommandBuffer command)>&& function){
        VK_CHECK(vkResetFences(_device, 1, &_immediateFence));
        VK_CHECK(vkResetCommandBuffer(_immediateCommandBuffer, 0));

        VkCommandBuffer command = _immediateCommandBuffer;
        VkCommandBufferBeginInfo commandBeginInfo = Initializers::commandBufferBeginInfo(VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT);
        VK_CHECK(vkBeginCommandBuffer(command, &commandBeginInfo));

        function(command);

        VK_CHECK(vkEndCommandBuffer(command));

        VkCommandBufferSubmitInfo submitInfo = Initializers::commandBufferSubmitInfo(command);
        VkSubmitInfo2 submit = Initializers::submitInfo(&submitInfo, nullptr, nullptr);

        VK_CHECK(vkQueueSubmit2(_graphicsQueue, 1, &submit, _immediateFence));
        VK_CHECK(vkWaitForFences(_device, 1, &_immediateFence, true, 9999999999));
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
        _descriptorDeletionQueue.flush();

        // createSwapchain();
        setupSwapchain();
        setupDescriptors();

        frameBufferResized = false;
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