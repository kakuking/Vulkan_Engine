#pragma once

#include "types.h"
#include "utility.h"
#include "initializers.h"
#include "structs.h"
#include "pipelineBuilder.h"

struct RectangleUniform {
    glm::mat4 modelMatrix;
};

struct RectangleMesh: public MeshBuffer {
public:
    std::string vertexShaderFile = "shaders\\shader.vert.spv", fragShaderFile = "shaders\\shader.frag.spv";

    void setup(VkDevice _device, VmaAllocator& _allocator, VkFormat drawImageFormat, VkFormat depthImageFormat) override {
        createDescriptorSetLayout(_device);
        createPipeline(_device, drawImageFormat, depthImageFormat);
        setupData();
    }

    void remakePipeline(VkDevice _device, VkFormat drawImageFormat, VkFormat depthImageFormat) override {
        pipelineDeletionQueue.flush();
        createPipeline(_device, drawImageFormat, depthImageFormat);
    }


    void update(VkDevice _device, VmaAllocator& allocator, DescriptorAllocator& _descriptorAllocator) override {
        updateUniformBuffer(_device, allocator);

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
        if (key == GLFW_KEY_SPACE){
            if(action == GLFW_PRESS){
                indices.push_back(2);
                indices.push_back(3);
                indices.push_back(4);
                indices.push_back(0);
                indices.push_back(5);
                indices.push_back(1);
                indexCount += 6;

                updateIndexBuffer = true;
            }
            
            if(action == GLFW_RELEASE){
                indices.pop_back();
                indices.pop_back();
                indices.pop_back();
                indices.pop_back();
                indices.pop_back();
                indices.pop_back();
                indexCount-=6;

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
    void updateUniformBuffer(VkDevice device, VmaAllocator& allocator) {
        uniformDeletionQueue.flush();

        uniformBuffer = Utility::createBuffer(allocator, sizeof(RectangleUniform), VK_BUFFER_USAGE_UNIFORM_BUFFER_BIT, VMA_MEMORY_USAGE_CPU_TO_GPU);

        uniformDeletionQueue.pushFunction([=]{
            Utility::destroyBuffer(allocator, uniformBuffer);
        });

        RectangleUniform* data = (RectangleUniform*)uniformBuffer.allocation->GetMappedData();
        *data = {
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

        pipelineDeletionQueue.pushFunction([=](){
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

        maxVertexCount = 6;
        maxIndexCount = 12;

        indexCount = 6;

        vertices.resize(6);
        indices.resize(6);

        vertices[0].position = {0.5,-0.5,0.0};
        vertices[1].position = {0.5,0.5,0.0};
        vertices[2].position = {-0.5,-0.5,0.0};
        vertices[3].position = {-0.5,0.5,0.0};
        vertices[4].position = {-0.75,0.0,0.0};
        vertices[5].position = {0.75,0.0,0.0};
        vertices[0].color = {1,0,1,1};
        vertices[1].color = {1,0,1,1};
        vertices[2].color = {1,0,1,1};
        vertices[3].color = {1,0,1,1};
        vertices[4].color = {0,1,0,1};
        vertices[5].color = {1,0,0,1};
        
        /*
        vertices[0].uv_x = 1;
        vertices[1].uv_x = 1;
        vertices[2].uv_x = 1;
        vertices[3].uv_x = 1;
        vertices[4].uv_x = 1;
        */

        indices[0] = 0;
        indices[1] = 1;
        indices[2] = 2;

        indices[3] = 2;
        indices[4] = 1;
        indices[5] = 3;

        /*
        indices[6] = 2;
        indices[7] = 3;
        indices[8] = 4;

        indices[9] = 0;
        indices[10] = 5;
        indices[11] = 1;
        */
    }
};