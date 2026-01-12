#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef ENGRAM_VULKAN_ENABLED

static const char *similarity_shader_src = 
    "#version 450\n"
    "#extension GL_EXT_shader_explicit_arithmetic_types : enable\n"
    "layout(local_size_x = 256) in;\n"
    "layout(set = 0, binding = 0) readonly buffer Neurons {\n"
    "    float embeddings[];\n"
    "};\n"
    "layout(set = 0, binding = 1) readonly buffer Query {\n"
    "    float query[256];\n"
    "};\n"
    "layout(set = 0, binding = 2) writeonly buffer Results {\n"
    "    float similarities[];\n"
    "};\n"
    "layout(push_constant) uniform Params {\n"
    "    uint neuron_count;\n"
    "    uint dim;\n"
    "} params;\n"
    "void main() {\n"
    "    uint idx = gl_GlobalInvocationID.x;\n"
    "    if (idx >= params.neuron_count) return;\n"
    "    float dot = 0.0;\n"
    "    uint base = idx * params.dim;\n"
    "    for (uint i = 0; i < params.dim; i++) {\n"
    "        dot += embeddings[base + i] * query[i];\n"
    "    }\n"
    "    similarities[idx] = dot;\n"
    "}\n";

static uint32_t find_memory_type(VkPhysicalDevice pd, uint32_t type_filter, VkMemoryPropertyFlags props) {
    VkPhysicalDeviceMemoryProperties mem_props;
    vkGetPhysicalDeviceMemoryProperties(pd, &mem_props);
    for (uint32_t i = 0; i < mem_props.memoryTypeCount; i++) {
        if ((type_filter & (1 << i)) && (mem_props.memoryTypes[i].propertyFlags & props) == props) {
            return i;
        }
    }
    return UINT32_MAX;
}

static bool create_buffer(vulkan_ctx_t *v, VkDeviceSize size, VkBufferUsageFlags usage,
                          VkMemoryPropertyFlags props, VkBuffer *buffer, VkDeviceMemory *memory) {
    VkBufferCreateInfo buf_info = {
        .sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO,
        .size = size,
        .usage = usage,
        .sharingMode = VK_SHARING_MODE_EXCLUSIVE
    };
    
    if (vkCreateBuffer(v->device, &buf_info, NULL, buffer) != VK_SUCCESS) {
        return false;
    }
    
    VkMemoryRequirements mem_req;
    vkGetBufferMemoryRequirements(v->device, *buffer, &mem_req);
    
    uint32_t mem_type = find_memory_type(v->physical_device, mem_req.memoryTypeBits, props);
    if (mem_type == UINT32_MAX) {
        vkDestroyBuffer(v->device, *buffer, NULL);
        return false;
    }
    
    VkMemoryAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO,
        .allocationSize = mem_req.size,
        .memoryTypeIndex = mem_type
    };
    
    if (vkAllocateMemory(v->device, &alloc_info, NULL, memory) != VK_SUCCESS) {
        vkDestroyBuffer(v->device, *buffer, NULL);
        return false;
    }
    
    vkBindBufferMemory(v->device, *buffer, *memory, 0);
    return true;
}

vulkan_ctx_t vulkan_create(void) {
    vulkan_ctx_t v = {0};
    
    VkApplicationInfo app_info = {
        .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
        .pApplicationName = "Engram",
        .applicationVersion = VK_MAKE_VERSION(1, 0, 0),
        .pEngineName = "Engram",
        .engineVersion = VK_MAKE_VERSION(1, 0, 0),
        .apiVersion = VK_API_VERSION_1_2
    };
    
    VkInstanceCreateInfo inst_info = {
        .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
        .pApplicationInfo = &app_info
    };
    
#ifdef __APPLE__
    inst_info.flags = VK_INSTANCE_CREATE_ENUMERATE_PORTABILITY_BIT_KHR;
    const char *extensions[] = {VK_KHR_PORTABILITY_ENUMERATION_EXTENSION_NAME};
    inst_info.enabledExtensionCount = 1;
    inst_info.ppEnabledExtensionNames = extensions;
#endif
    
    if (vkCreateInstance(&inst_info, NULL, &v.instance) != VK_SUCCESS) {
        return v;
    }
    
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(v.instance, &device_count, NULL);
    if (device_count == 0) {
        vkDestroyInstance(v.instance, NULL);
        v.instance = VK_NULL_HANDLE;
        return v;
    }
    
    VkPhysicalDevice *devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(v.instance, &device_count, devices);
    v.physical_device = devices[0];
    free(devices);
    
    uint32_t queue_count = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(v.physical_device, &queue_count, NULL);
    VkQueueFamilyProperties *queues = malloc(queue_count * sizeof(VkQueueFamilyProperties));
    vkGetPhysicalDeviceQueueFamilyProperties(v.physical_device, &queue_count, queues);
    
    v.compute_family = UINT32_MAX;
    for (uint32_t i = 0; i < queue_count; i++) {
        if (queues[i].queueFlags & VK_QUEUE_COMPUTE_BIT) {
            v.compute_family = i;
            break;
        }
    }
    free(queues);
    
    if (v.compute_family == UINT32_MAX) {
        vkDestroyInstance(v.instance, NULL);
        v.instance = VK_NULL_HANDLE;
        return v;
    }
    
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queue_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO,
        .queueFamilyIndex = v.compute_family,
        .queueCount = 1,
        .pQueuePriorities = &priority
    };
    
    VkDeviceCreateInfo dev_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info
    };
    
    if (vkCreateDevice(v.physical_device, &dev_info, NULL, &v.device) != VK_SUCCESS) {
        vkDestroyInstance(v.instance, NULL);
        v.instance = VK_NULL_HANDLE;
        return v;
    }
    
    vkGetDeviceQueue(v.device, v.compute_family, 0, &v.compute_queue);
    
    VkCommandPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO,
        .queueFamilyIndex = v.compute_family,
        .flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT
    };
    
    if (vkCreateCommandPool(v.device, &pool_info, NULL, &v.command_pool) != VK_SUCCESS) {
        vkDestroyDevice(v.device, NULL);
        vkDestroyInstance(v.instance, NULL);
        v.instance = VK_NULL_HANDLE;
        v.device = VK_NULL_HANDLE;
        return v;
    }
    
    v.initialized = true;
    return v;
}

void vulkan_destroy(vulkan_ctx_t *v) {
    if (!v->initialized) return;
    
    vkDeviceWaitIdle(v->device);
    
    if (v->similarity_pipeline) vkDestroyPipeline(v->device, v->similarity_pipeline, NULL);
    if (v->similarity_layout) vkDestroyPipelineLayout(v->device, v->similarity_layout, NULL);
    if (v->descriptor_layout) vkDestroyDescriptorSetLayout(v->device, v->descriptor_layout, NULL);
    if (v->similarity_shader) vkDestroyShaderModule(v->device, v->similarity_shader, NULL);
    if (v->descriptor_pool) vkDestroyDescriptorPool(v->device, v->descriptor_pool, NULL);
    if (v->neuron_buffer) vkDestroyBuffer(v->device, v->neuron_buffer, NULL);
    if (v->neuron_memory) vkFreeMemory(v->device, v->neuron_memory, NULL);
    if (v->result_buffer) vkDestroyBuffer(v->device, v->result_buffer, NULL);
    if (v->result_memory) vkFreeMemory(v->device, v->result_memory, NULL);
    if (v->command_pool) vkDestroyCommandPool(v->device, v->command_pool, NULL);
    if (v->device) vkDestroyDevice(v->device, NULL);
    if (v->instance) vkDestroyInstance(v->instance, NULL);
    
    memset(v, 0, sizeof(*v));
}

bool vulkan_sync_neurons(vulkan_ctx_t *v, substrate_t *s) {
    if (!v->initialized || s->neuron_count == 0) return false;
    
    size_t data_size = s->neuron_count * ENGRAM_VECTOR_DIM * sizeof(float);
    
    if (data_size > v->buffer_capacity) {
        if (v->neuron_buffer) vkDestroyBuffer(v->device, v->neuron_buffer, NULL);
        if (v->neuron_memory) vkFreeMemory(v->device, v->neuron_memory, NULL);
        if (v->result_buffer) vkDestroyBuffer(v->device, v->result_buffer, NULL);
        if (v->result_memory) vkFreeMemory(v->device, v->result_memory, NULL);
        
        size_t new_cap = data_size * 2;
        
        if (!create_buffer(v, new_cap, 
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           &v->neuron_buffer, &v->neuron_memory)) {
            return false;
        }
        
        if (!create_buffer(v, s->neuron_count * sizeof(float) * 2,
                           VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                           VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                           &v->result_buffer, &v->result_memory)) {
            return false;
        }
        
        v->buffer_capacity = new_cap;
    }
    
    float *mapped;
    if (vkMapMemory(v->device, v->neuron_memory, 0, data_size, 0, (void **)&mapped) != VK_SUCCESS) {
        return false;
    }
    
    for (size_t i = 0; i < s->neuron_count; i++) {
        memcpy(mapped + i * ENGRAM_VECTOR_DIM, s->neurons[i].embedding, 
               ENGRAM_VECTOR_DIM * sizeof(float));
    }
    
    vkUnmapMemory(v->device, v->neuron_memory);
    return true;
}

size_t vulkan_similarity_search(vulkan_ctx_t *v, const engram_vec_t query,
                                 engram_id_t *results, float *scores, 
                                 size_t max_results, float threshold) {
    (void)v;
    (void)query;
    (void)results;
    (void)scores;
    (void)max_results;
    (void)threshold;
    return 0;
}

#endif
