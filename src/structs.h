#pragma once

#include "types.h"

struct SwapChainInfomation{
    VkSwapchainKHR swapchain;
    std::vector<VkImage> swapchainImages;
    std::vector<VkImageView> swapchainImageViews;
    VkExtent2D swapchainExtent;
    VkFormat swapchainImageFormat;
};

struct BootstrapInstance{
    VkInstance instance;
    VkDebugUtilsMessengerEXT debugMessenger;
};

struct BootstrapDevice{
    VkDevice device;
    VkPhysicalDevice physicalDevice;
    VkQueue graphicsQueue;
    uint32_t graphicsQueueFamily;
};

struct DeletionQueue{
    std::deque<std::function<void()>> deletors;

    void pushFunction(std::function<void()>&& func){
        deletors.push_back(func);
    }

    void flush(){
        for(auto func = deletors.rbegin(); func != deletors.rend(); func++){
            (*func)();
        }

        deletors.clear();
    }
};

struct AllocatedImage {
    VkImage image;
    VkImageView imageView;
    VkDeviceMemory imageMemory;
    VmaAllocation allocation;
    VkExtent3D imageExtent;
    VkFormat imageFormat;
};

struct ComputeShaderPushConstants{
    glm::vec4 color1;
    glm::vec4 color2;
    glm::vec4 color3;
    glm::vec4 color4;
    glm::mat4 viewMatrix;
};

struct MeshPushConstants{
    glm::mat4 worldMatrix;
    VkDeviceAddress vertexBuffer;
};

struct AllocatedBuffer{
    VkBuffer buffer;
    VmaAllocation allocation;
    VmaAllocationInfo info;
};

struct Vertex {
    glm::vec3 position;
    float uv_x;
    glm::vec3 normal;
    float uv_y;
    glm::vec4 color;
};

struct ComputeEffect{
    const char* name;

    VkPipeline pipeline;
    VkPipelineLayout layout;

    ComputeShaderPushConstants data;
};

struct DescriptorAllocator{
public:
    struct PoolSizeRatio{
        VkDescriptorType type;
        float ratio;
    };

    void setupPool(VkDevice device, uint32_t initialSets, std::span<PoolSizeRatio> poolRatios){
        ratios.clear();

        for(auto r: poolRatios) {
            ratios.push_back(r);
        }

        VkDescriptorPool newPool = createPool(device, initialSets, poolRatios);
        setsPerPool = initialSets * 1.5;
        readyPools.push_back(newPool);
    }

    void clearDescriptors(VkDevice device){
        for (auto p : readyPools) {
            vkResetDescriptorPool(device, p, 0);
        }
        for (auto p : fullPools) {
            vkResetDescriptorPool(device, p, 0);
            readyPools.push_back(p);
        }
        fullPools.clear();
    }

    void destroyPool(VkDevice device){
        for (auto p : readyPools) {
            vkDestroyDescriptorPool(device, p, nullptr);
        }
        readyPools.clear();
        for (auto p : fullPools) {
            vkDestroyDescriptorPool(device,p,nullptr);
        }
        fullPools.clear();
    }

    VkDescriptorSet allocate(VkDevice device, VkDescriptorSetLayout layout, void* pNext = nullptr){
        VkDescriptorPool poolToUse = getPool(device);

        VkDescriptorSetAllocateInfo allocInfo{};
        allocInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
        allocInfo.pNext = pNext;
        allocInfo.descriptorPool = poolToUse;
        allocInfo.descriptorSetCount = 1;
        allocInfo.pSetLayouts = &layout;

        VkDescriptorSet ds;
        VkResult result = vkAllocateDescriptorSets(device, &allocInfo, &ds);

        //allocation failed. Try again
        if (result == VK_ERROR_OUT_OF_POOL_MEMORY || result == VK_ERROR_FRAGMENTED_POOL) {

            fullPools.push_back(poolToUse);
        
            poolToUse = getPool(device);
            allocInfo.descriptorPool = poolToUse;

        VK_CHECK( vkAllocateDescriptorSets(device, &allocInfo, &ds));
        }
    
        readyPools.push_back(poolToUse);
        return ds;
    }

private:
    std::vector<PoolSizeRatio> ratios;
    std::vector<VkDescriptorPool> fullPools;
    std::vector<VkDescriptorPool> readyPools;
    uint32_t setsPerPool;

    VkDescriptorPool getPool(VkDevice device){
        VkDescriptorPool newPool;

        // If there is a ready pool
        if(readyPools.size() != 0){
            newPool = readyPools.back();
            readyPools.pop_back();

            return newPool;
        }

        newPool = createPool(device, setsPerPool, ratios);
        setsPerPool *= 1.5;
        setsPerPool = setsPerPool > 4092 ? 4092 : setsPerPool; // cap value

        return newPool;
    }

    VkDescriptorPool createPool(VkDevice device, uint32_t setCount, std::span<PoolSizeRatio> poolRatios){
        std::vector<VkDescriptorPoolSize> poolSizes;
        for(PoolSizeRatio ratio: poolRatios){
            poolSizes.push_back(VkDescriptorPoolSize{
                .type = ratio.type,
                .descriptorCount = static_cast<uint32_t>(ratio.ratio * setCount)
            });
        }

        VkDescriptorPoolCreateInfo poolInfo{};
        poolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
        poolInfo.flags = 0;
        poolInfo.maxSets = setCount;
        poolInfo.poolSizeCount = static_cast<uint32_t>(poolSizes.size());
        poolInfo.pPoolSizes = poolSizes.data();

        VkDescriptorPool newPool;
        vkCreateDescriptorPool(device, &poolInfo, nullptr, &newPool);

        return newPool;
    }
};

struct DescriptorWriter {
    std::deque<VkDescriptorImageInfo> imageInfos;
    std::deque<VkDescriptorBufferInfo> bufferInfos;
    std::vector<VkWriteDescriptorSet> writes;

    void writeImage(int binding, VkImageView image, VkSampler sampler, VkImageLayout layout, VkDescriptorType type){
            VkDescriptorImageInfo& info = imageInfos.emplace_back(VkDescriptorImageInfo{
            .sampler = sampler,
            .imageView = image,
            .imageLayout = layout
        });

        VkWriteDescriptorSet write = { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET };

        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pImageInfo = &info;

        writes.push_back(write);
    }

    void writeBuffer(int binding, VkBuffer buffer, size_t size, size_t offset, VkDescriptorType type){
        VkDescriptorBufferInfo& info = bufferInfos.emplace_back(VkDescriptorBufferInfo{
            .buffer = buffer,
            .offset = offset,
            .range = size
        });

        VkWriteDescriptorSet write{};
        write.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        write.dstBinding = binding;
        write.dstSet = VK_NULL_HANDLE; //left empty for now until we need to write it
        write.descriptorCount = 1;
        write.descriptorType = type;
        write.pBufferInfo = &info;

        writes.push_back(write);
    }

    void clear(){
        imageInfos.clear();
        writes.clear();
        bufferInfos.clear();
    }

    void updateSet(VkDevice device, VkDescriptorSet set){
        for(VkWriteDescriptorSet& write: writes){
            write.dstSet = set;
        }

        vkUpdateDescriptorSets(device, static_cast<uint32_t>(writes.size()), writes.data(), 0, nullptr);
    }
};

struct DescriptorLayoutBuilder {
    std::vector<VkDescriptorSetLayoutBinding> bindings;

    void addBinding(uint32_t binding, VkDescriptorType type){
        VkDescriptorSetLayoutBinding newBind{};
        newBind.binding = binding;
        newBind.descriptorCount = 1;
        newBind.descriptorType = type;

        bindings.push_back(newBind);
    }

    void clear(){
        bindings.clear();
    }

    VkDescriptorSetLayout build(VkDevice device, VkShaderStageFlags shaderStages, void* pNext = nullptr, VkDescriptorSetLayoutCreateFlags flags = 0){
        for (auto& b: bindings)
        {
            b.stageFlags |= shaderStages;
        }

        VkDescriptorSetLayoutCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        info.pNext = pNext;

        info.pBindings = bindings.data();
        info.bindingCount = static_cast<uint32_t>(bindings.size());
        info.flags = flags;
        
        VkDescriptorSetLayout set;
        VK_CHECK(vkCreateDescriptorSetLayout(device, &info, nullptr, &set));

        return set;
    }
};

struct FrameData{
    VkCommandPool commandPool;
    VkCommandBuffer mainCommandBuffer;

    VkSemaphore swapchainSemaphore, renderSemaphore;
    VkFence renderFence;

    DeletionQueue deletionQueue;
    DescriptorAllocator frameDescriptors;
};

struct QueueFamilyIndices {
    std::optional<uint32_t> graphicsFamily;
    std::optional<uint32_t> presentFamily;

    bool isComplete() {
        return graphicsFamily.has_value() &&
        presentFamily.has_value();
    }

    static QueueFamilyIndices findQueueFamilies(VkPhysicalDevice device, VkSurfaceKHR surface) {
    QueueFamilyIndices indices;

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(device, &queueFamilyCount, queueFamilies.data());

    int i = 0;

    for (const auto& queueFamily: queueFamilies) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            indices.graphicsFamily = i;
        }

        VkBool32 presentSupport = false;
        vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &presentSupport);

        if(presentSupport) {
            indices.presentFamily = i;
        }

        if (indices.isComplete()) {
            break;
        }

        i++;
    }

    return indices;
}
};

struct SwapChainSupportDetails {
    VkSurfaceCapabilitiesKHR capabilities;
    std::vector<VkSurfaceFormatKHR> formats;
    std::vector<VkPresentModeKHR> presentModes;

    static SwapChainSupportDetails querySwapChainSupport(VkPhysicalDevice physicalDevice, VkSurfaceKHR surface) {
    SwapChainSupportDetails details;

    vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &details.capabilities);

    uint32_t formatCount;
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);

    if (formatCount != 0) {
        details.formats.resize(formatCount);
        vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, details.formats.data());
    }

    uint32_t presentModeCount;
    vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, nullptr);

    if(presentModeCount != 0){
        details.presentModes.resize(presentModeCount);
        vkGetPhysicalDeviceSurfacePresentModesKHR(physicalDevice, surface, &presentModeCount, details.presentModes.data());
    }

    return details;
}
};

struct MeshBuffer{
public:
    AllocatedBuffer vertexBuffer;
    AllocatedBuffer indexBuffer;
    AllocatedBuffer uniformBuffer;

    uint32_t indexCount, maxVertexCount, maxIndexCount;

    bool updateVertexBuffer, updateIndexBuffer;

    VkDeviceAddress vertexBufferAddress;

    std::vector<Vertex> vertices; 
    std::vector<uint32_t> indices;

    VkPipelineLayout pipelineLayout;
    VkPipeline pipeline;

    VkDescriptorSetLayout setLayout;
    VkDescriptorSet set;

    DeletionQueue pipelineDeletionQueue, uniformDeletionQueue, deletionQueue, bufferDeletionQueue;
    virtual ~MeshBuffer() = default;

    virtual void setup(VkDevice _device, VmaAllocator& _allocator, VkFormat drawImageFormat, VkFormat depthImageFormat){};
    virtual void setVertexBufferAddress(VkDeviceAddress newAddress){};

    virtual void remakePipeline(VkDevice _device, VkFormat drawImageFormat, VkFormat depthImageFormat){};

    virtual void update(VkDevice _device, VmaAllocator& allocator,  DescriptorAllocator& _descriptorAllocator){};
    virtual void draw(VkCommandBuffer& command, glm::mat4 viewProj){};

    virtual void keyUpdate(GLFWwindow* window, int key, int scancode, int action, int mods){};
};
