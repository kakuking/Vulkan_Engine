#pragma once

#include "types.h"
#include "utility.h"
#include "initializers.h"
#include "structs.h"
#include "pipelineBuilder.h"

#include "world.h"

struct RectangleUniform {
    glm::mat4 modelMatrix;
};

struct RectangleMesh: public Mesh {
public:
    std::string vertexShaderFile = "shaders\\shader.vert.spv", fragShaderFile = "shaders\\shader.frag.spv";

    void setup(VkDevice _device, VmaAllocator& _allocator, VkFormat drawImageFormat, VkFormat depthImageFormat) override {
        createDescriptorSetLayout(_device);
        createPipeline(_device, drawImageFormat, depthImageFormat);
        setupData();
        setupUniformBuffer(_device, _allocator);
    }

    void remakePipeline(VkDevice _device, VkFormat drawImageFormat, VkFormat depthImageFormat) override {
        pipelineDeletionQueue.flush();
        createPipeline(_device, drawImageFormat, depthImageFormat);
    }

    void imguiInterface(){
        if(ImGui::Begin("External Mesh Test")){
            ImGui::SliderFloat("Rotation Speed", &rotationSpeed, -20.f, 20.f);

            ImGui::SliderFloat3("Axis of Rotation", (float*)& axisOfRotation, -20.f, 20.f);
        }
        ImGui::End();
    }

    void update(VkDevice _device, VmaAllocator& allocator, DescriptorAllocator& _descriptorAllocator) override {
        updateUniformBuffer();

        updateWorld();

        set = _descriptorAllocator.allocate(_device, setLayout);

        DescriptorWriter writer;
        writer.writeBuffer(0, uniformBuffer.buffer, sizeof(RectangleUniform), 0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        writer.updateSet(_device, set);
    }

    void draw(VkCommandBuffer& command, glm::mat4 viewProj) override {
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);

        MeshPushConstants pushConstantsOpaque;
        pushConstantsOpaque.worldMatrix = viewProj;
        pushConstantsOpaque.vertexBuffer = vertexBufferAddress;

        vkCmdPushConstants(command, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0, sizeof(MeshPushConstants), &pushConstantsOpaque);

        vkCmdBindIndexBuffer(command, indexBuffer.buffer, 0, VK_INDEX_TYPE_UINT32);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &set, 0, nullptr);

        vkCmdDrawIndexed(command, indexCount, 1, 0, 0, 0);
    }

    void setVertexBufferAddress(VkDeviceAddress address) override {
        vertexBufferAddress = address;
    }

    void keyUpdate(GLFWwindow* window, int key, int scancode, int action, int mods) override {
        if (key == GLFW_KEY_LEFT){
            if(action == GLFW_PRESS){
                indices.push_back(2);
                indices.push_back(3);
                indices.push_back(4);
                indexCount += 3;

                updateIndexBuffer = true;
            }
            
            if(action == GLFW_RELEASE){
                indices.pop_back();
                indices.pop_back();
                indices.pop_back();
                indexCount-=3;

                updateIndexBuffer = true;
            }
        } else 
        if (key == GLFW_KEY_RIGHT){
            if(action == GLFW_PRESS){
                indices.push_back(0);
                indices.push_back(5);
                indices.push_back(1);
                indexCount += 3;

                updateIndexBuffer = true;
            }
            
            if(action == GLFW_RELEASE){
                indices.pop_back();
                indices.pop_back();
                indices.pop_back();
                indexCount-=3;

                updateIndexBuffer = true;
            }
        } else
        if (key == GLFW_KEY_DOWN){
            if(action == GLFW_PRESS){
                indices.push_back(6);
                indices.push_back(0);
                indices.push_back(2);
                indexCount += 3;

                updateIndexBuffer = true;
            }
            
            if(action == GLFW_RELEASE){
                indices.pop_back();
                indices.pop_back();
                indices.pop_back();
                indexCount-=3;

                updateIndexBuffer = true;
            }
        } else 
        if (key == GLFW_KEY_UP){
            if(action == GLFW_PRESS){
                indices.push_back(3);
                indices.push_back(1);
                indices.push_back(7);
                indexCount += 3;

                updateIndexBuffer = true;
            }
            
            if(action == GLFW_RELEASE){
                indices.pop_back();
                indices.pop_back();
                indices.pop_back();
                indexCount-=3;

                updateIndexBuffer = true;
            }
        }
    }

    void setVertexShader(std::string newName){
        vertexShaderFile = newName;
    }

    void setFragShader(std::string newName){
        fragShaderFile = newName;
    }


private:
    World world;

    void setupUniformBuffer(VkDevice device, VmaAllocator& allocator){
        uniformBuffer = Utility::createBuffer(allocator, sizeof(RectangleUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        uniformDeletionQueue.pushFunction([this, allocator]{
            Utility::destroyBuffer(allocator, uniformBuffer);
        });
    }

    float rotationSpeed = 0.1f;
    float rotAngle = 0.f;
    glm::vec3 axisOfRotation = glm::vec3(0.0f, 0.0f, 1.0f);

    std::chrono::time_point<std::chrono::high_resolution_clock> prevTime = std::chrono::high_resolution_clock::now();

    void updateUniformBuffer() {
        // float timeDelta = getTimeDelta();
        // rotAngle += timeDelta * rotationSpeed;

        RectangleUniform* data = (RectangleUniform*)uniformBuffer.allocation->GetMappedData();
        *data = {
            // glm::rotate(glm::mat4(1.0f), rotAngle, axisOfRotation)
            glm::mat4(1.0f)
        };
    }

    void createPipeline(VkDevice _device, VkFormat drawImageFormat, VkFormat depthImageFormat){
        VkShaderModule vertexShader;
        if(!Utility::loadShaderModule(vertexShaderFile.c_str(), _device, &vertexShader)){
            fmt::println("Failed to load vertex shader");
        }

        VkShaderModule fragShader;
        if(!Utility::loadShaderModule(fragShaderFile.c_str(), _device, &fragShader)){
            fmt::println("Failed to load frag shader");
        }

        VkPushConstantRange bufferRange{};
        bufferRange.offset = 0;
        bufferRange.size = sizeof(MeshPushConstants);
        bufferRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;

        VkPipelineLayoutCreateInfo layoutInfo = Initializers::pipelineLayoutCreateInfo();
        layoutInfo.pushConstantRangeCount = 1;
        layoutInfo.pPushConstantRanges = &bufferRange;

        layoutInfo.setLayoutCount = 1;
        layoutInfo.pSetLayouts = &setLayout;

        VK_CHECK(vkCreatePipelineLayout(_device, &layoutInfo, nullptr, &pipelineLayout));

        PipelineBuilder pipelineBuilder;
        pipelineBuilder.pipelineLayout = pipelineLayout;
        pipelineBuilder.setShaders(vertexShader, fragShader);
        pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST);
        // pipelineBuilder.setInputTopology(VK_PRIMITIVE_TOPOLOGY_TRIANGLE_STRIP);
        pipelineBuilder.setPolygonMode(VK_POLYGON_MODE_FILL);
        pipelineBuilder.setCullMode(VK_CULL_MODE_NONE, VK_FRONT_FACE_CLOCKWISE);
        pipelineBuilder.setMultisamplingNone();
        pipelineBuilder.enableBlendingAlphablend();
        pipelineBuilder.enableDepthtest(VK_TRUE, VK_COMPARE_OP_LESS_OR_EQUAL);
        // pipelineBuilder.disableDepthtest();

        pipelineBuilder.setColorAttachmentFormat(drawImageFormat);
        pipelineBuilder.setDepthFormat(depthImageFormat);

        pipeline = pipelineBuilder.buildPipeline(_device);

        vkDestroyShaderModule(_device, vertexShader, nullptr);
        vkDestroyShaderModule(_device, fragShader, nullptr);

        pipelineDeletionQueue.pushFunction([this, _device](){
            // fmt::println("About to destroy mesh pipelinelayout");
            vkDestroyPipelineLayout(_device, pipelineLayout, nullptr);
            // fmt::println("About to destroy mesh pipeline");
            vkDestroyPipeline(_device, pipeline, nullptr);
        });
    }

    void createDescriptorSetLayout(VkDevice _device){
        DescriptorLayoutBuilder builder;
        builder.addBinding(0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER);
        setLayout = builder.build(_device, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT);
    }

    void setupData(){
        maxVertexCount = 100;
        maxIndexCount = 1000;

        world.setLevel(1);
        world.addCircle(glm::vec3(0.0, 0.0, 0.0), 0.2f, glm::vec4(0.5, 0.5, 0.0, 1.0),  8);

        vertices = world.getVertices();
        indices = world.getIndices();

        indexCount = indices.size();
    }

    void updateWorld(){
        world.update(getTimeDelta());

        vertices = world.getVertices();

        updateVertexBuffer = true;
    }

    float getTimeDelta() {
        // static auto startTime = std::chrono::high_resolution_clock::now();
        auto currentTime = std::chrono::high_resolution_clock::now();
        std::chrono::duration<float> elapsed = currentTime - prevTime;
        prevTime = currentTime;
        return elapsed.count();
    }
};