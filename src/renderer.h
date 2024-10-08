#pragma once
#include "types.h"

#include "bootstrap.h"
#include "structs.h"
#include "utility.h"
#include "initializers.h"
#include "pipelineBuilder.h"

class Renderer{
public:
    GLFWwindow* _window;
    VkSurfaceKHR _surface;

    VkInstance _instance;
    VkDebugUtilsMessengerEXT _debugMessenger;

    VkPhysicalDevice _physicalDevice;
    VkDevice _device;

    VmaAllocator _allocator;

    DescriptorAllocator _backgroundDescriptorAllocator;     // Use for the background draw image

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

    VkPipeline _backgroundShaderPipeline;
    VkPipelineLayout _backgroundShaderPipelineLayout;
    VkDescriptorSet _drawImageDescriptors;
    VkDescriptorSetLayout _drawImageDescriptorLayout;

    std::vector<ComputeEffect> _backgroundEffects;
    int _currentBackground{0};

    std::vector<Mesh*> _meshes;

    VkFence _immediateFence;
    VkCommandBuffer _immediateCommandBuffer;
    VkCommandPool _immediateCommandPool;
    
    bool frameBufferResized;

    glm::mat4 _view, _proj;
    float _fov{45.f};
    int _useOrtho{0};

    void init(){
        setupWindow();
        setupVulkan();
        setupSwapchain();
        setupCommandResources();
        setupSyncStructures();
        setupDescriptors();
        setupViewAndProjMatrices();
        setupPipeline();
        // setupDefaultRectangleData();
        setupImgui();

        frameBufferResized = false;
    }

    void run(){
        double totalFrameTime = 0.0f;
        int frameCount = 0;
        auto startTime = std::chrono::high_resolution_clock::now();


        while(!glfwWindowShouldClose(_window)){
            // fmt::println("in loop");

            auto frameStartTime = std::chrono::high_resolution_clock::now();
            glfwPollEvents();

            if(frameBufferResized) {
                frameBufferResized = false;
                recreateSwapChain();
            }
            // fmt::println("After checking framebuffer");

            ImGui_ImplGlfw_NewFrame();
            ImGui_ImplVulkan_NewFrame();
            ImGui::NewFrame();
            // fmt::println("About to render imgui");
            renderImgui();

            // fmt::println("REaching Draw");

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

        getCurrentFrame().deletionQueue.flush();

        for (size_t i = 0; i < FRAME_OVERLAP; i++)
        {
            vkDestroyCommandPool(_device, _frames[i].commandPool, nullptr);

            vkDestroyFence(_device, _frames[i].renderFence, nullptr);
            vkDestroySemaphore(_device, _frames[i].renderSemaphore, nullptr);
            vkDestroySemaphore(_device, _frames[i].swapchainSemaphore, nullptr);
        }

        for(auto& mesh: _meshes){
            mesh->bufferDeletionQueue.flush();
            mesh->uniformDeletionQueue.flush();
            mesh->pipelineDeletionQueue.flush();
            mesh->deletionQueue.flush();
        }

        _descriptorDeletionQueue.flush();
        cleanupSwapchain();

        _mainDeletionQueue.flush();

        vkDestroySurfaceKHR(_instance, _surface, nullptr);
        vkDestroyDevice(_device, nullptr);
        destroyDebugUtilsMessengerEXT(_instance, _debugMessenger, nullptr);
        vkDestroyInstance(_instance, nullptr);
        
        cleanupWindow();
    }

    void addMesh(Mesh* newMesh){
        _meshes.push_back(newMesh);
    }

private:
    DeletionQueue _mainDeletionQueue;
    DeletionQueue _swapchainDeletionQueue;
    DeletionQueue _descriptorDeletionQueue;

    float elapsedTimeMandelbrot = 0.0f; //for Mandelbrot 
    float elapsedTimeJulia = 0.0f; //for Mandelbrot 

    void draw(){
        // fmt::println("In Draw()");
        
        VK_CHECK(vkWaitForFences(_device, 1, &getCurrentFrame().renderFence, VK_TRUE, 1000000000));

        getCurrentFrame().deletionQueue.flush();
        getCurrentFrame().frameDescriptors.clearDescriptors(_device);

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

        Utility::transitionImage(command, _drawImage.image,VK_IMAGE_LAYOUT_GENERAL, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        Utility::transitionImage(command, _depthImage.image,VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        drawGeometry(command);

        Utility::transitionImage(command, _drawImage.image, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
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

    void drawGeometry(VkCommandBuffer command){
        
        // Check if buffer needs to be updated, instead of in keyUpdate
        for(auto& mesh: _meshes){
            if(mesh->updateIndexBuffer){
                size_t s = mesh->indices.size() * sizeof(uint32_t);
                copyBuffer(mesh->indexBuffer.buffer, mesh->indices.data(), s);

                mesh->updateIndexBuffer = false;
            }

            if(mesh->updateVertexBuffer){
                size_t s = mesh->vertices.size() * sizeof(Vertex);
                copyBuffer(mesh->vertexBuffer.buffer, mesh->vertices.data(), s);

                mesh->updateVertexBuffer = false;
            }
        }

        VkRenderingAttachmentInfo colorAttachment = Initializers::attachmentInfo(_drawImage.imageView, nullptr, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        VkRenderingAttachmentInfo depthAttachment = Initializers::depthAttachmentInfo(_depthImage.imageView, VK_IMAGE_LAYOUT_DEPTH_ATTACHMENT_OPTIMAL);

        VkRenderingInfo renderInfo = Initializers::renderingInfo(_drawExtent, &colorAttachment, &depthAttachment);

        VkViewport viewport{};
        viewport.x = 0;
        viewport.y = 0;
        viewport.width = _drawExtent.width;
        viewport.height = _drawExtent.height;
        viewport.minDepth = 0.f;
        viewport.maxDepth = 1.f;

        VkRect2D scissor{};
        scissor.offset.x = 0;
        scissor.offset.y = 0;
        scissor.extent.width = _drawExtent.width;
        scissor.extent.height = _drawExtent.height;

        vkCmdBeginRendering(command, &renderInfo);  
        vkCmdSetViewport(command, 0, 1, &viewport);
        vkCmdSetScissor(command, 0, 1, &scissor);

        for(auto& mesh: _meshes){
            mesh->update(_device, _allocator, getCurrentFrame().frameDescriptors);

            mesh->draw(command, _proj * _view);
        }

        vkCmdEndRendering(command);
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
        // setupMeshPipeline();

        for(auto& mesh: _meshes){
            mesh->setup(_device, _allocator, _drawImage.imageFormat, _depthImage.imageFormat);

            uploadExternalMesh(*mesh);

            mesh->bufferDeletionQueue.pushFunction([this, mesh]{
                // fmt::println("About to destroy mesh vertex buffer");
                Utility::destroyBuffer(_allocator, mesh->vertexBuffer);
                // fmt::println("About to destroy mesh index buffer");
                Utility::destroyBuffer(_allocator, mesh->indexBuffer);
                // fmt::println("About to destroy desc set layout");
                vkDestroyDescriptorSetLayout(_device, mesh->setLayout, nullptr);
            });

            // fmt::println("Uploaded mesh");
        }
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

        VkShaderModule mandelbrotShader;
        if(!Utility::loadShaderModule("shaders\\mandelbrot.comp.spv", _device, &mandelbrotShader)){
            fmt::print("Failed to load mandelbrot Shader!");
        }

        VkShaderModule juliaShader;
        if(!Utility::loadShaderModule("shaders\\julia.comp.spv", _device, &juliaShader)){
            fmt::print("Failed to load julia Shader!");
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
        gradient.data.viewMatrix = _view;

        VK_CHECK(vkCreateComputePipelines(_device,VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &gradient.pipeline));

        computePipelineCreateInfo.stage.module = skyShader;

        ComputeEffect sky;
        sky.layout = _backgroundShaderPipelineLayout;
        sky.name = "sky";
        sky.data = {};
        sky.data.color1 = glm::vec4(0.709f, 0.113f, 0.333f, 0.97f);
        sky.data.viewMatrix = _view;

        VK_CHECK(vkCreateComputePipelines(_device,VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &sky.pipeline));

        computePipelineCreateInfo.stage.module = mandelbrotShader;

        ComputeEffect mandelbrot;
        mandelbrot.layout = _backgroundShaderPipelineLayout;
        mandelbrot.name = "mandelbrot";
        mandelbrot.data = {};
        mandelbrot.data.color1 = glm::vec4(0.0465f, 0.2252f, 0.f, 0.f);    // z and w dont matter
        mandelbrot.data.viewMatrix = _view;

        VK_CHECK(vkCreateComputePipelines(_device,VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &mandelbrot.pipeline));

        computePipelineCreateInfo.stage.module = juliaShader;

        ComputeEffect julia;
        julia.layout = _backgroundShaderPipelineLayout;
        julia.name = "julia";
        julia.data = {};
        julia.data.color1 = glm::vec4(0.0465f, 0.2252f, 0.f, 0.f);    // z and w dont matter
        julia.data.color2 = glm::vec4(-0.618f, 0.f, 0.f, 0.f);
        julia.data.viewMatrix = _view;

        VK_CHECK(vkCreateComputePipelines(_device,VK_NULL_HANDLE,1,&computePipelineCreateInfo, nullptr, &julia.pipeline));

        _backgroundEffects.push_back(gradient);
        _backgroundEffects.push_back(sky);
        _backgroundEffects.push_back(mandelbrot);
        _backgroundEffects.push_back(julia);

        vkDestroyShaderModule(_device, gradientShader, nullptr);
        vkDestroyShaderModule(_device, skyShader, nullptr);
        vkDestroyShaderModule(_device, mandelbrotShader, nullptr);
        vkDestroyShaderModule(_device, juliaShader, nullptr);

        _mainDeletionQueue.pushFunction([&]() {
            vkDestroyPipelineLayout(_device, _backgroundShaderPipelineLayout, nullptr);
            for (size_t i = 0; i < _backgroundEffects.size(); i++)
            {
                vkDestroyPipeline(_device, _backgroundEffects[i].pipeline, nullptr);
            }            
        });
    }

    float getTimeMandelbrot(float& timeVariable) {
        static auto lastTimeMandelbrot = std::chrono::high_resolution_clock::now();

        if (_currentBackground == 2) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> delta = now - lastTimeMandelbrot;
            timeVariable += delta.count();
            lastTimeMandelbrot = now;
            // fmt::println("elapsedTime: {}", timeVariable);
        } else {
            lastTimeMandelbrot = std::chrono::high_resolution_clock::now(); // Reset the timer if condition is false
        }

        return timeVariable;
    }

    float getTimeJulia(float& timeVariable) {
        static auto lastTimeJulia = std::chrono::high_resolution_clock::now();

        if (_currentBackground == 3) {
            auto now = std::chrono::high_resolution_clock::now();
            std::chrono::duration<float> delta = now - lastTimeJulia;
            timeVariable += delta.count();
            lastTimeJulia = now;
            // fmt::println("elapsedTime: {}", timeVariable);
        } else {
            lastTimeJulia = std::chrono::high_resolution_clock::now(); // Reset the timer if condition is false
        }

        return timeVariable;
    }

    void renderImgui(){
        if(ImGui::Begin("Background")) {
            ComputeEffect& selected = _backgroundEffects[_currentBackground];

            ImGui::Text("Selected Effect: %s", selected.name);

            ImGui::SliderInt("Effect Index: ", &_currentBackground, 0, _backgroundEffects.size() - 1);

            _currentBackground = std::clamp(_currentBackground, 0, (int)(_backgroundEffects.size() - 1));

            // Temp colors so that we can easily set RGB values in range of 255
            
            if(strcmp(selected.name, "gradient") == 0){
                glm::vec4 tempColor1 = selected.data.color1 * 255.f;
                glm::vec4 tempColor2 = selected.data.color2 * 255.f;

                ImGui::SliderFloat4("Color 1", (float*)& tempColor1, 0.f, 255.f);
                ImGui::SliderFloat4("Color 2", (float*)& tempColor2, 0.f, 255.f);

                selected.data.color1 = tempColor1 / 255.f;
                selected.data.color2 = tempColor2 / 255.f;
            } else if(strcmp(selected.name, "sky") == 0) {
                glm::vec4 tempColor1 = selected.data.color1 * 255.f;

                ImGui::SliderFloat4("Color 1", (float*)& tempColor1, 0.f, 255.f);

                selected.data.color1 = tempColor1 / 255.f;
            }
            
            // For Time Dependant ones
            if(strcmp(selected.name, "mandelbrot") == 0) {
                ImGui::SliderFloat2("Position of Zoom", (float*)& selected.data.color1, -1.0f, 1.0f);
                ImGui::SliderFloat("Zoom Level", (float*)& selected.data.color2, 0.f, 100.f);
                // ImGui::Text("Time: %d", selected.data.color3.x);

                selected.data.color3.x = getTimeMandelbrot(elapsedTimeMandelbrot);
            } else {
                getTimeMandelbrot(elapsedTimeMandelbrot);
            }

            if(strcmp(selected.name, "julia") == 0) {
                ImGui::SliderFloat2("Position of Zoom", (float*)& selected.data.color1, -1.0f, 1.0f);
                ImGui::SliderFloat2("C", (float*)& selected.data.color2, -2.0f, 2.0f);
                ImGui::SliderFloat("Zoom Level", (float*)& selected.data.color3, -10.f, 100.f);

                selected.data.color4.x = getTimeJulia(elapsedTimeJulia);
            } else {
                getTimeJulia(elapsedTimeJulia);
            }
            
            /*
            if(strcmp(selected.name, "sky") == 0){
                // Temp colors so that we can easily set RGB values in range of 255
                glm::vec4 tempColor3 = selected.data.color3 * 255.f;
                glm::vec4 tempColor4 = selected.data.color4 * 255.f;

                ImGui::InputFloat4("Color 3", (float*)& tempColor3);
                ImGui::InputFloat4("Color 4", (float*)& tempColor4);

                selected.data.color3 = tempColor3 / 255.f;
                selected.data.color4 = tempColor4 / 255.f;
            }
            */
        }
        ImGui::End();

        static int prevUseOrtho = _useOrtho;
        static glm::vec3 prevEye = initialEye, prevCenter = initialCenter, prevUp = initialUp;
        static float prevFOV = _fov;

        if(ImGui::Begin("Camera Setting")) {
            
            // Switch between perspective and ortho cameras
            if(_useOrtho == 0)
                ImGui::Text("Perspective Camera");
            else
                ImGui::Text("Orthographic Camera");

            ImGui::SliderInt("##", &_useOrtho, 0, 1);
            if (_useOrtho != prevUseOrtho) {
                prevUseOrtho = _useOrtho;
                setProjMatrix();
            }

            // Setting camera eye
            glm::vec3 newEye = prevEye, newCenter = prevCenter, newUp = prevUp;

            float rangeOfMovement = 20.f;
            ImGui::SliderFloat3("Eye", (float*)& newEye, -rangeOfMovement, rangeOfMovement);
            ImGui::SliderFloat3("Center", (float*)& newCenter, -rangeOfMovement, rangeOfMovement);
            ImGui::SliderFloat3("Up", (float*)& newUp, -rangeOfMovement, rangeOfMovement);

            if (newEye != prevEye || newCenter != prevCenter || newUp != prevUp) {
                setViewMatrix(newEye, newCenter, newUp);
                prevEye = newEye;
                prevCenter = newCenter;
                prevUp = newUp;
            }

            if(_useOrtho == 0){
                ImGui::SliderFloat("FOV", &_fov, 20.f, 180.f);

                if(_fov != prevFOV){
                    prevFOV = _fov;
                    setProjMatrix();
                }
            }
        }

        ImGui::End();

        for(auto mesh: _meshes){
            mesh->imguiInterface();
        }

        ImGui::Render();
    }

    void setupDescriptors(){
        std::vector<DescriptorAllocator::PoolSizeRatio> sizeRatios = {
            {VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 1}
        } ;

        _backgroundDescriptorAllocator.setupPool(_device, 10, sizeRatios);

        {
            DescriptorLayoutBuilder builder;
            builder.addBinding(0, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
            _drawImageDescriptorLayout = builder.build(_device, VK_SHADER_STAGE_COMPUTE_BIT);
        }

        _drawImageDescriptors = _backgroundDescriptorAllocator.allocate(_device, _drawImageDescriptorLayout);

        DescriptorWriter writer;
        writer.writeImage(0, _drawImage.imageView, VK_NULL_HANDLE, VK_IMAGE_LAYOUT_GENERAL, VK_DESCRIPTOR_TYPE_STORAGE_IMAGE);
        writer.updateSet(_device, _drawImageDescriptors);

        _descriptorDeletionQueue.pushFunction([&](){
            _backgroundDescriptorAllocator.destroyPool(_device);
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

            std::vector<DescriptorAllocator::PoolSizeRatio> frame_sizes = { 
                { VK_DESCRIPTOR_TYPE_STORAGE_IMAGE, 3 },
                { VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, 3 },
                { VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 3 },
                { VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 4 },
            };

            _frames[i].frameDescriptors = DescriptorAllocator{};
            _frames[i].frameDescriptors.setupPool(_device, 1000, frame_sizes);

            _mainDeletionQueue.pushFunction([&, i]() {
                _frames[i].frameDescriptors.destroyPool(_device);
            });
        }

        VK_CHECK(vkCreateCommandPool(_device, &createInfo, nullptr, &_immediateCommandPool));

        VkCommandBufferAllocateInfo allocInfo = Initializers::commandBufferAllocateInfo(_immediateCommandPool, 1);

        VK_CHECK(vkAllocateCommandBuffers(_device, &allocInfo, &_immediateCommandBuffer));

        _mainDeletionQueue.pushFunction([&](){
            vkDestroyCommandPool(_device, _immediateCommandPool, nullptr);
        });
    }

    void uploadExternalMesh(Mesh& newSurface){
        const size_t vertexBufferSize = newSurface.vertices.size() * sizeof(Vertex);
        const size_t indexBufferSize = newSurface.indices.size() * sizeof(uint32_t);

        const size_t maxVertexBufferSize = newSurface.maxVertexCount * sizeof(Vertex);
        const size_t maxIndexBufferSize = newSurface.maxIndexCount * sizeof(uint32_t);

        newSurface.vertexBuffer = Utility::createBuffer(_allocator, maxVertexBufferSize, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_SHADER_DEVICE_ADDRESS_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        VkBufferDeviceAddressInfo deviceAddressInfo{};
        deviceAddressInfo.sType = VK_STRUCTURE_TYPE_BUFFER_DEVICE_ADDRESS_INFO;
        deviceAddressInfo.buffer = newSurface.vertexBuffer.buffer;

        newSurface.vertexBufferAddress = vkGetBufferDeviceAddress(_device, &deviceAddressInfo);

        newSurface.indexBuffer = Utility::createBuffer(_allocator, maxIndexBufferSize, VK_BUFFER_USAGE_INDEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_DST_BIT, VMA_MEMORY_USAGE_GPU_ONLY);

        AllocatedBuffer stagingBuffer = Utility::createBuffer(_allocator, vertexBufferSize + indexBufferSize, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        void* data = stagingBuffer.allocation->GetMappedData();

        memcpy(data, newSurface.vertices.data(), vertexBufferSize);
        memcpy((char*)data + vertexBufferSize, newSurface.indices.data(), indexBufferSize);

        immediateSubmit([&](VkCommandBuffer command){
            VkBufferCopy vertexCopy{0};
            vertexCopy.dstOffset = 0;
            vertexCopy.srcOffset = 0;
            vertexCopy.size = vertexBufferSize;

            VkBufferCopy indexCopy{0};
            indexCopy.dstOffset = 0;
            indexCopy.srcOffset = vertexBufferSize;
            indexCopy.size = indexBufferSize;

            vkCmdCopyBuffer(command, stagingBuffer.buffer, newSurface.vertexBuffer.buffer, 1, &vertexCopy);
            vkCmdCopyBuffer(command, stagingBuffer.buffer, newSurface.indexBuffer.buffer, 1, &indexCopy);
        });

        Utility::destroyBuffer(_allocator, stagingBuffer);

        return;
    }

    void copyBuffer(VkBuffer dstBuffer, void* src, size_t size){
        AllocatedBuffer stagingBuffer = Utility::createBuffer(_allocator, size, VK_BUFFER_USAGE_TRANSFER_SRC_BIT, VMA_MEMORY_USAGE_CPU_ONLY);

        void* data = stagingBuffer.allocation->GetMappedData();

        memcpy(data, src, size);

        immediateSubmit([&](VkCommandBuffer command){
            VkBufferCopy copyRegion{0};
            copyRegion.dstOffset = 0;
            copyRegion.srcOffset = 0;
            copyRegion.size = size;

            vkCmdCopyBuffer(command, stagingBuffer.buffer, dstBuffer, 1, &copyRegion);
        });

        Utility::destroyBuffer(_allocator, stagingBuffer);
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

        VK_CHECK(vkCreateFence(_device, &fenceCreateInfo, nullptr, &_immediateFence));

        _mainDeletionQueue.pushFunction([&](){
            vkDestroyFence(_device, _immediateFence, nullptr);
        });
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

        // Get new Width and Height
        int width = 0, height = 0;
        glfwGetFramebufferSize(_window, &width, &height);
        while(width == 0 || height == 0){
            glfwGetFramebufferSize(_window, &width, &height);
            glfwWaitEvents();
        }

        WIDTH = static_cast<uint32_t>(width);
        HEIGHT = static_cast<uint32_t>(height);

        // Wait for device to get idlt
        vkDeviceWaitIdle(_device);

        // Cleanup swapchain, mesh pipeline and descriptors for that stuff
        cleanupSwapchain();
        _descriptorDeletionQueue.flush();

        // Set everything back up again
        setupSwapchain();
        // setupMeshPipeline();

        for(auto& mesh: _meshes){
            mesh->remakePipeline(_device, _drawImage.imageFormat, _depthImage.imageFormat);
        }

        setupDescriptors();

        // Set the new projection matrix
        setProjMatrix();

        // Make sure the frameBufferResized flag is set to false
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

        glfwSetKeyCallback(_window, keyCallback);
        glfwSetCursorPosCallback(_window, cursorCallback);
    }

    static void cursorCallback(GLFWwindow* window, double xpos, double ypos){
        // float normalized_x = -1.f + 2.f*(xpos/(float)WIDTH);
        // float normalized_y = -1.f + 2.f*(ypos/(float)HEIGHT);
        // fmt::println("({}, {}) ===> ({}, {})", xpos, ypos, normalized_x, normalized_y);
    }

    static void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
        auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
        app->appKeyCallback(window, key, scancode, action, mods);
    }

    static void frameBufferResizeCallback(GLFWwindow* window, int width, int heigh){
            auto app = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
            app->frameBufferResized = true;
        }

    void appKeyCallback(GLFWwindow* window, int key, int scancode, int action, int mods){
        for(auto& mesh: _meshes){
            mesh->keyUpdate(window, key, scancode, action, mods);
        }
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

    void setupViewAndProjMatrices(){
        setViewMatrix(initialEye, initialCenter, initialUp);
        setProjMatrix();
    }

    void setProjMatrix(){
        if(_useOrtho == 0){
            _proj = glm::perspective(glm::radians(_fov), (float)_swapchainExtent.width / (float)_swapchainExtent.height, .1f, 100.f);
        } else {
            float aspectRatio = (float)_swapchainExtent.width / (float)_swapchainExtent.height;
            _proj = glm::ortho(-1.f*aspectRatio, 1.f*aspectRatio, -1.0f, 1.0f, 0.1f, 100.f);
        }

        _proj[1][1] *= -1;
    }

    void setViewMatrix(glm::vec3 eye, glm::vec3 center, glm::vec3 up){
        _view = glm::lookAt(eye, center, up);

        for(ComputeEffect& effect: _backgroundEffects){
            effect.data.viewMatrix = _view;
        }
    }

};