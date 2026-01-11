#ifdef ENGRAM_VULKAN_ENABLED

#include "internal.h"
#include <vulkan/vulkan.h>
#include <stdlib.h>
#include <string.h>

struct engram_vk_context {
    VkInstance instance;
    VkPhysicalDevice physical_device;
    VkDevice device;
    VkQueue compute_queue;
    uint32_t queue_family;
    VkCommandPool command_pool;
    VkCommandBuffer command_buffer;
    VkDescriptorPool descriptor_pool;
    VkDescriptorSetLayout descriptor_layout;
    VkPipelineLayout pipeline_layout;
    VkPipeline propagate_pipeline;
    VkPipeline decay_pipeline;
    VkBuffer neuron_buffer;
    VkDeviceMemory neuron_memory;
    VkBuffer synapse_buffer;
    VkDeviceMemory synapse_memory;
    VkBuffer input_buffer;
    VkDeviceMemory input_memory;
    VkBuffer output_buffer;
    VkDeviceMemory output_memory;
    uint32_t neuron_count;
    uint32_t synapse_capacity;
};

static const uint32_t propagate_shader[] = {
    0x07230203, 0x00010000, 0x00080001, 0x00000050,
    0x00000000, 0x00020011, 0x00000001, 0x0006000B,
    0x00000001, 0x4C534C47, 0x6474732E, 0x3035342E,
    0x00000000, 0x0003000E, 0x00000000, 0x00000001,
    0x0006000F, 0x00000005, 0x00000004, 0x6E69616D,
    0x00000000, 0x0000000C, 0x00060010, 0x00000004,
    0x00000011, 0x00000100, 0x00000001, 0x00000001,
    0x00030003, 0x00000002, 0x000001C2, 0x00040005,
    0x00000004, 0x6E69616D, 0x00000000, 0x00050048,
    0x00000008, 0x00000000, 0x00000023, 0x00000000,
    0x00030047, 0x00000008, 0x00000002, 0x00040047,
    0x0000000C, 0x0000000B, 0x0000001C, 0x00020013,
    0x00000002, 0x00030021, 0x00000003, 0x00000002,
    0x00040015, 0x00000006, 0x00000020, 0x00000001,
    0x0004001E, 0x00000008, 0x00000006, 0x00000006,
    0x00040020, 0x00000009, 0x00000009, 0x00000008,
    0x0004003B, 0x00000009, 0x0000000A, 0x00000009,
};

static uint32_t find_memory_type(VkPhysicalDevice pd, uint32_t filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(pd, &mem_props);
    
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return 0;
}

static int create_buffer(engram_vk_context_t *ctx, VkDeviceSize size,
                         VkBufferUsageFlags usage, VkMemoryPropertyFlags props,
                         VkBuffer *buffer, VkDeviceMemory *memory) {
    VkBufferCreateInfo buffer_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    
    if (vkCreateBuffer(ctx->device, &buffer_info, NULL, buffer) != VK_SUCCESS) {
        return -1;
    }
    
    VkMemoryRequirements mem_reqs;
    vkGetBufferMemoryRequirements(ctx->device, *buffer, &mem_reqs);
    
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_reqs.size,
        .memoryTypeIndex = find_memory_type(ctx->physical_device, mem_reqs.memoryTypeBits, props)
    };
    
    if (vkAllocateMemory(ctx->device, &alloc_info, NULL, memory) != VK_SUCCESS) {
        vkDestroyBuffer(ctx->device, *buffer, NULL);
        return -1;
    }
    
    vkBindBufferMemory(ctx->device, *buffer, *memory, 0);
    return 0;
}

engram_vk_context_t *engram_vk_init(void) {
    engram_vk_context_t *ctx = calloc(1, sizeof(engram_vk_context_t));
    if (!ctx) return NULL;
    
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Engram",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Engram",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2
    };
    
    VkInstanceCreateInfo create_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info
    };
    
    if (vkCreateInstance(&create_info, NULL, &ctx->instance) != VK_SUCCESS) {
        free(ctx);
        return NULL;
    }
    
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, NULL);
    if (device_count == 0) {
        vkDestroyInstance(ctx->instance, NULL);
        free(ctx);
        return NULL;
    }
    
    VkPhysicalDevice *devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(ctx->instance, &device_count, devices);
    ctx->physical_device = devices[0];
    free(devices);
    
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_count, NULL);
    VkQueueFamilyProperties *queue_props = malloc(queue_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(ctx->physical_device, &queue_count, queue_props);
    
    ctx->queue_family = UINT32_MAX;
    for (uint32_t i = 0; i < queue_count; i++) {
        if (queue_props[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            ctx->queue_family = i;
            break;
        }
    }
    free(queue_props);
    
    if (ctx->queue_family == UINT32_MAX) {
        vkDestroyInstance(ctx->instance, NULL);
        free(ctx);
        return NULL;
    }
    
    float queue_priority = 1.0f;
    VkDeviceQueueCreateInfo queue_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = ctx->queue_family,
        .queueCount = 1,
        .pQueuePriorities = &queue_priority
    };
    
    VkDeviceCreateInfo device_create_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_create_info
    };
    
    if (vkCreateDevice(ctx->physical_device, &device_create_info, NULL, &ctx->device) != VK_SUCCESS) {
        vkDestroyInstance(ctx->instance, NULL);
        free(ctx);
        return NULL;
    }
    
    vkGetDeviceQueue(ctx->device, ctx->queue_family, 0, &ctx->compute_queue);
    
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = ctx->queue_family,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    
    if (vkCreateCommandPool(ctx->device, &pool_info, NULL, &ctx->command_pool) != VK_SUCCESS) {
        vkDestroyDevice(ctx->device, NULL);
        vkDestroyInstance(ctx->instance, NULL);
        free(ctx);
        return NULL;
    }
    
    VkCommandBufferAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = ctx->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    
    vkAllocateCommandBuffers(ctx->device, &alloc_info, &ctx->command_buffer);
    
    return ctx;
}

void engram_vk_destroy(engram_vk_context_t *ctx) {
    if (!ctx) return;
    
    vkDeviceWaitIdle(ctx->device);
    
    if (ctx->output_buffer) {
        vkDestroyBuffer(ctx->device, ctx->output_buffer, NULL);
        vkFreeMemory(ctx->device, ctx->output_memory, NULL);
    }
    if (ctx->input_buffer) {
        vkDestroyBuffer(ctx->device, ctx->input_buffer, NULL);
        vkFreeMemory(ctx->device, ctx->input_memory, NULL);
    }
    if (ctx->synapse_buffer) {
        vkDestroyBuffer(ctx->device, ctx->synapse_buffer, NULL);
        vkFreeMemory(ctx->device, ctx->synapse_memory, NULL);
    }
    if (ctx->neuron_buffer) {
        vkDestroyBuffer(ctx->device, ctx->neuron_buffer, NULL);
        vkFreeMemory(ctx->device, ctx->neuron_memory, NULL);
    }
    if (ctx->propagate_pipeline) vkDestroyPipeline(ctx->device, ctx->propagate_pipeline, NULL);
    if (ctx->decay_pipeline) vkDestroyPipeline(ctx->device, ctx->decay_pipeline, NULL);
    if (ctx->pipeline_layout) vkDestroyPipelineLayout(ctx->device, ctx->pipeline_layout, NULL);
    if (ctx->descriptor_layout) vkDestroyDescriptorSetLayout(ctx->device, ctx->descriptor_layout, NULL);
    if (ctx->descriptor_pool) vkDestroyDescriptorPool(ctx->device, ctx->descriptor_pool, NULL);
    if (ctx->command_pool) vkDestroyCommandPool(ctx->device, ctx->command_pool, NULL);
    if (ctx->device) vkDestroyDevice(ctx->device, NULL);
    if (ctx->instance) vkDestroyInstance(ctx->instance, NULL);
    
    free(ctx);
}

int engram_vk_setup_buffers(engram_vk_context_t *ctx, uint32_t neuron_count, uint32_t synapse_capacity) {
    ctx->neuron_count = neuron_count;
    ctx->synapse_capacity = synapse_capacity;
    
    VkMemoryPropertyFlags props = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
    VkBufferUsageFlags usage = VK_BUFFER_USAGE_STORAGE_BUFFER_BIT;
    
    if (create_buffer(ctx, neuron_count * sizeof(engram_neuron_t), usage, props,
                      &ctx->neuron_buffer, &ctx->neuron_memory) < 0) {
        return -1;
    }
    
    if (create_buffer(ctx, synapse_capacity * sizeof(engram_synapse_t), usage, props,
                      &ctx->synapse_buffer, &ctx->synapse_memory) < 0) {
        return -1;
    }
    
    if (create_buffer(ctx, ENGRAM_PATTERN_DIM * sizeof(float), usage, props,
                      &ctx->input_buffer, &ctx->input_memory) < 0) {
        return -1;
    }
    
    if (create_buffer(ctx, ENGRAM_PATTERN_DIM * sizeof(float), usage, props,
                      &ctx->output_buffer, &ctx->output_memory) < 0) {
        return -1;
    }
    
    return 0;
}

void engram_vk_propagate(engram_vk_context_t *ctx, engram_substrate_t *s,
                          const engram_pattern_t *input, engram_pattern_t *output) {
    if (!ctx || !ctx->device) {
        engram_propagate_cpu(s, input, output);
        return;
    }
    
    void *data;
    vkMapMemory(ctx->device, ctx->neuron_memory, 0, 
                s->neuron_count * sizeof(engram_neuron_t), 0, &data);
    memcpy(data, s->neurons, s->neuron_count * sizeof(engram_neuron_t));
    vkUnmapMemory(ctx->device, ctx->neuron_memory);
    
    vkMapMemory(ctx->device, ctx->synapse_memory, 0,
                s->synapse_count * sizeof(engram_synapse_t), 0, &data);
    memcpy(data, s->synapses, s->synapse_count * sizeof(engram_synapse_t));
    vkUnmapMemory(ctx->device, ctx->synapse_memory);
    
    vkMapMemory(ctx->device, ctx->input_memory, 0,
                ENGRAM_PATTERN_DIM * sizeof(float), 0, &data);
    memcpy(data, input->values, ENGRAM_PATTERN_DIM * sizeof(float));
    vkUnmapMemory(ctx->device, ctx->input_memory);
    
    vkMapMemory(ctx->device, ctx->output_memory, 0,
                ENGRAM_PATTERN_DIM * sizeof(float), 0, &data);
    memcpy(output->values, data, ENGRAM_PATTERN_DIM * sizeof(float));
    vkUnmapMemory(ctx->device, ctx->output_memory);
    
    output->active_count = 0;
    float sum = 0.0f;
    for (uint32_t i = 0; i < ENGRAM_PATTERN_DIM; i++) {
        sum += output->values[i] * output->values[i];
        if (output->values[i] > 0.1f && output->active_count < 64) {
            output->active_indices[output->active_count++] = i;
        }
    }
    output->magnitude = sqrtf(sum);
}

void engram_vk_decay(engram_vk_context_t *ctx, engram_substrate_t *s, float rate) {
    (void)ctx;
    for (uint32_t i = 0; i < s->synapse_count; i++) {
        float w = atomic_load(&s->synapses[i].weight);
        w *= (1.0f - rate);
        atomic_store(&s->synapses[i].weight, w);
    }
}

#endif
