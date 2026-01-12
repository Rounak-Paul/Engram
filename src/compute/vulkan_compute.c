#include "internal.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#ifdef ENGRAM_VULKAN_ENABLED

#ifdef ENGRAM_HAS_SHADERC
#include <shaderc/shaderc.h>
#endif

static const char *similarity_glsl = 
    "#version 450\n"
    "layout(local_size_x = 256) in;\n"
    "layout(set = 0, binding = 0) readonly buffer Neurons { float embeddings[]; };\n"
    "layout(set = 0, binding = 1) readonly buffer Query { float query[]; };\n"
    "layout(set = 0, binding = 2) writeonly buffer Results { float similarities[]; };\n"
    "layout(push_constant) uniform Params { uint neuron_count; uint dim; } params;\n"
    "void main() {\n"
    "    uint idx = gl_GlobalInvocationID.x;\n"
    "    if (idx >= params.neuron_count) return;\n"
    "    float d = 0.0;\n"
    "    uint base = idx * params.dim;\n"
    "    for (uint i = 0; i < params.dim; i++) d += embeddings[base + i] * query[i];\n"
    "    similarities[idx] = d;\n"
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
    
    if (vkCreateBuffer(v->device, &buf_info, NULL, buffer) != VK_SUCCESS) return false;
    
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

#ifdef ENGRAM_HAS_SHADERC
static bool compile_shader(vulkan_ctx_t *v) {
    shaderc_compiler_t compiler = shaderc_compiler_initialize();
    if (!compiler) return false;
    
    shaderc_compile_options_t options = shaderc_compile_options_initialize();
    shaderc_compile_options_set_optimization_level(options, shaderc_optimization_level_performance);
    shaderc_compile_options_set_target_env(options, shaderc_target_env_vulkan, shaderc_env_version_vulkan_1_2);
    
    shaderc_compilation_result_t result = shaderc_compile_into_spv(
        compiler, similarity_glsl, strlen(similarity_glsl),
        shaderc_glsl_compute_shader, "similarity.comp", "main", options);
    
    shaderc_compile_options_release(options);
    
    if (shaderc_result_get_compilation_status(result) != shaderc_compilation_status_success) {
        fprintf(stderr, "Shader compile error: %s\n", shaderc_result_get_error_message(result));
        shaderc_result_release(result);
        shaderc_compiler_release(compiler);
        return false;
    }
    
    size_t spirv_size = shaderc_result_get_length(result);
    const uint32_t *spirv_code = (const uint32_t *)shaderc_result_get_bytes(result);
    
    VkShaderModuleCreateInfo shader_info = {
        .sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO,
        .codeSize = spirv_size,
        .pCode = spirv_code
    };
    
    VkResult vk_result = vkCreateShaderModule(v->device, &shader_info, NULL, &v->similarity_shader);
    
    shaderc_result_release(result);
    shaderc_compiler_release(compiler);
    
    return vk_result == VK_SUCCESS;
}
#else
static bool compile_shader(vulkan_ctx_t *v) {
    (void)v;
    (void)similarity_glsl;
    fprintf(stderr, "Shaderc not available, cannot compile shaders at runtime\n");
    return false;
}
#endif

static bool create_compute_pipeline(vulkan_ctx_t *v) {
    if (!compile_shader(v)) return false;
    
    VkDescriptorSetLayoutBinding bindings[] = {
        { .binding = 0, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT },
        { .binding = 2, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 1, .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT }
    };
    
    VkDescriptorSetLayoutCreateInfo layout_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO,
        .bindingCount = 3,
        .pBindings = bindings
    };
    
    if (vkCreateDescriptorSetLayout(v->device, &layout_info, NULL, &v->descriptor_layout) != VK_SUCCESS) return false;
    
    VkPushConstantRange push_range = { .stageFlags = VK_SHADER_STAGE_COMPUTE_BIT, .offset = 0, .size = 8 };
    
    VkPipelineLayoutCreateInfo pipeline_layout_info = {
        .sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO,
        .setLayoutCount = 1,
        .pSetLayouts = &v->descriptor_layout,
        .pushConstantRangeCount = 1,
        .pPushConstantRanges = &push_range
    };
    
    if (vkCreatePipelineLayout(v->device, &pipeline_layout_info, NULL, &v->similarity_layout) != VK_SUCCESS) return false;
    
    VkComputePipelineCreateInfo pipeline_info = {
        .sType = VK_STRUCTURE_TYPE_COMPUTE_PIPELINE_CREATE_INFO,
        .stage = {
            .sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO,
            .stage = VK_SHADER_STAGE_COMPUTE_BIT,
            .module = v->similarity_shader,
            .pName = "main"
        },
        .layout = v->similarity_layout
    };
    
    if (vkCreateComputePipelines(v->device, VK_NULL_HANDLE, 1, &pipeline_info, NULL, &v->similarity_pipeline) != VK_SUCCESS) return false;
    
    VkDescriptorPoolSize pool_size = { .type = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .descriptorCount = 3 };
    
    VkDescriptorPoolCreateInfo pool_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO,
        .maxSets = 1,
        .poolSizeCount = 1,
        .pPoolSizes = &pool_size
    };
    
    if (vkCreateDescriptorPool(v->device, &pool_info, NULL, &v->descriptor_pool) != VK_SUCCESS) return false;
    
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
    
    if (vkCreateInstance(&inst_info, NULL, &v.instance) != VK_SUCCESS) return v;
    
    uint32_t device_count = 0;
    vkEnumeratePhysicalDevices(v.instance, &device_count, NULL);
    if (device_count == 0) {
        vkDestroyInstance(v.instance, NULL);
        v.instance = VK_NULL_HANDLE;
        return v;
    }
    
    VkPhysicalDevice *devices = malloc(device_count * sizeof(VkPhysicalDevice));
    vkEnumeratePhysicalDevices(v.instance, &device_count, devices);
    
    for (uint32_t i = 0; i < device_count; i++) {
        VkPhysicalDeviceProperties props;
        vkGetPhysicalDeviceProperties(devices[i], &props);
        if (props.deviceType == VK_PHYSICAL_DEVICE_TYPE_DISCRETE_GPU) {
            v.physical_device = devices[i];
            break;
        }
    }
    if (!v.physical_device) v.physical_device = devices[0];
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
    
#ifdef __APPLE__
    const char *dev_extensions[] = {"VK_KHR_portability_subset"};
    VkDeviceCreateInfo dev_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info,
        .enabledExtensionCount = 1,
        .ppEnabledExtensionNames = dev_extensions
    };
#else
    VkDeviceCreateInfo dev_info = {
        .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
        .queueCreateInfoCount = 1,
        .pQueueCreateInfos = &queue_info
    };
#endif
    
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
    
    if (!create_compute_pipeline(&v)) {
        vkDestroyCommandPool(v.device, v.command_pool, NULL);
        vkDestroyDevice(v.device, NULL);
        vkDestroyInstance(v.instance, NULL);
        memset(&v, 0, sizeof(v));
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
    if (v->query_buffer) vkDestroyBuffer(v->device, v->query_buffer, NULL);
    if (v->query_memory) vkFreeMemory(v->device, v->query_memory, NULL);
    if (v->result_buffer) vkDestroyBuffer(v->device, v->result_buffer, NULL);
    if (v->result_memory) vkFreeMemory(v->device, v->result_memory, NULL);
    if (v->command_pool) vkDestroyCommandPool(v->device, v->command_pool, NULL);
    if (v->device) vkDestroyDevice(v->device, NULL);
    if (v->instance) vkDestroyInstance(v->instance, NULL);
    
    memset(v, 0, sizeof(*v));
}

bool vulkan_sync_neurons(vulkan_ctx_t *v, substrate_t *s) {
    if (!v->initialized || s->neuron_count == 0) return false;
    
    size_t embed_size = s->neuron_count * ENGRAM_VECTOR_DIM * sizeof(float);
    size_t result_size = s->neuron_count * sizeof(float);
    size_t query_size = ENGRAM_VECTOR_DIM * sizeof(float);
    
    if (embed_size > v->buffer_capacity || !v->neuron_buffer) {
        if (v->neuron_buffer) vkDestroyBuffer(v->device, v->neuron_buffer, NULL);
        if (v->neuron_memory) vkFreeMemory(v->device, v->neuron_memory, NULL);
        if (v->query_buffer) vkDestroyBuffer(v->device, v->query_buffer, NULL);
        if (v->query_memory) vkFreeMemory(v->device, v->query_memory, NULL);
        if (v->result_buffer) vkDestroyBuffer(v->device, v->result_buffer, NULL);
        if (v->result_memory) vkFreeMemory(v->device, v->result_memory, NULL);
        
        size_t new_cap = embed_size * 2;
        VkMemoryPropertyFlags mem_flags = VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT;
        
        if (!create_buffer(v, new_cap, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, mem_flags, &v->neuron_buffer, &v->neuron_memory)) return false;
        if (!create_buffer(v, query_size, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, mem_flags, &v->query_buffer, &v->query_memory)) return false;
        if (!create_buffer(v, result_size * 2, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, mem_flags, &v->result_buffer, &v->result_memory)) return false;
        
        v->buffer_capacity = new_cap;
    }
    
    float *mapped;
    if (vkMapMemory(v->device, v->neuron_memory, 0, embed_size, 0, (void **)&mapped) != VK_SUCCESS) return false;
    
    for (size_t i = 0; i < s->neuron_count; i++) {
        memcpy(mapped + i * ENGRAM_VECTOR_DIM, s->neurons[i].embedding, ENGRAM_VECTOR_DIM * sizeof(float));
    }
    
    vkUnmapMemory(v->device, v->neuron_memory);
    v->synced_count = s->neuron_count;
    return true;
}

size_t vulkan_similarity_search(vulkan_ctx_t *v, substrate_t *s, const engram_vec_t query,
                                 engram_id_t *results, float *scores, 
                                 size_t max_results, float threshold) {
    if (!v->initialized || v->synced_count == 0) return 0;
    
    float *query_mapped;
    if (vkMapMemory(v->device, v->query_memory, 0, ENGRAM_VECTOR_DIM * sizeof(float), 0, (void **)&query_mapped) != VK_SUCCESS) return 0;
    memcpy(query_mapped, query, ENGRAM_VECTOR_DIM * sizeof(float));
    vkUnmapMemory(v->device, v->query_memory);
    
    VkDescriptorSetAllocateInfo alloc_info = {
        .sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO,
        .descriptorPool = v->descriptor_pool,
        .descriptorSetCount = 1,
        .pSetLayouts = &v->descriptor_layout
    };
    
    VkDescriptorSet descriptor_set;
    vkResetDescriptorPool(v->device, v->descriptor_pool, 0);
    if (vkAllocateDescriptorSets(v->device, &alloc_info, &descriptor_set) != VK_SUCCESS) return 0;
    
    VkDescriptorBufferInfo buffer_infos[] = {
        { .buffer = v->neuron_buffer, .offset = 0, .range = VK_WHOLE_SIZE },
        { .buffer = v->query_buffer, .offset = 0, .range = VK_WHOLE_SIZE },
        { .buffer = v->result_buffer, .offset = 0, .range = VK_WHOLE_SIZE }
    };
    
    VkWriteDescriptorSet writes[] = {
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set, .dstBinding = 0, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffer_infos[0] },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set, .dstBinding = 1, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffer_infos[1] },
        { .sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET, .dstSet = descriptor_set, .dstBinding = 2, .descriptorCount = 1, .descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER, .pBufferInfo = &buffer_infos[2] }
    };
    
    vkUpdateDescriptorSets(v->device, 3, writes, 0, NULL);
    
    VkCommandBufferAllocateInfo cmd_alloc = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO,
        .commandPool = v->command_pool,
        .level = VK_COMMAND_BUFFER_LEVEL_PRIMARY,
        .commandBufferCount = 1
    };
    
    VkCommandBuffer cmd;
    if (vkAllocateCommandBuffers(v->device, &cmd_alloc, &cmd) != VK_SUCCESS) return 0;
    
    VkCommandBufferBeginInfo begin_info = {
        .sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO,
        .flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT
    };
    
    vkBeginCommandBuffer(cmd, &begin_info);
    vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, v->similarity_pipeline);
    vkCmdBindDescriptorSets(cmd, VK_PIPELINE_BIND_POINT_COMPUTE, v->similarity_layout, 0, 1, &descriptor_set, 0, NULL);
    
    uint32_t push_data[2] = { (uint32_t)v->synced_count, ENGRAM_VECTOR_DIM };
    vkCmdPushConstants(cmd, v->similarity_layout, VK_SHADER_STAGE_COMPUTE_BIT, 0, 8, push_data);
    
    uint32_t group_count = ((uint32_t)v->synced_count + 255) / 256;
    vkCmdDispatch(cmd, group_count, 1, 1);
    
    vkEndCommandBuffer(cmd);
    
    VkSubmitInfo submit = {
        .sType = VK_STRUCTURE_TYPE_SUBMIT_INFO,
        .commandBufferCount = 1,
        .pCommandBuffers = &cmd
    };
    
    vkQueueSubmit(v->compute_queue, 1, &submit, VK_NULL_HANDLE);
    vkQueueWaitIdle(v->compute_queue);
    
    vkFreeCommandBuffers(v->device, v->command_pool, 1, &cmd);
    
    float *result_mapped;
    if (vkMapMemory(v->device, v->result_memory, 0, v->synced_count * sizeof(float), 0, (void **)&result_mapped) != VK_SUCCESS) return 0;
    
    size_t result_count = 0;
    for (size_t i = 0; i < v->synced_count; i++) {
        float sim = result_mapped[i];
        if (sim >= threshold) {
            size_t insert_pos = result_count;
            for (size_t j = 0; j < result_count; j++) {
                if (sim > scores[j]) {
                    insert_pos = j;
                    break;
                }
            }
            
            if (insert_pos < max_results) {
                size_t move_count = result_count - insert_pos;
                if (result_count >= max_results) move_count--;
                
                if (move_count > 0) {
                    memmove(&results[insert_pos + 1], &results[insert_pos], move_count * sizeof(engram_id_t));
                    memmove(&scores[insert_pos + 1], &scores[insert_pos], move_count * sizeof(float));
                }
                
                results[insert_pos] = s->neurons[i].id;
                scores[insert_pos] = sim;
                
                if (result_count < max_results) result_count++;
            }
        }
    }
    
    vkUnmapMemory(v->device, v->result_memory);
    return result_count;
}

#else

vulkan_ctx_t vulkan_create(void) {
    vulkan_ctx_t v = {0};
    return v;
}

void vulkan_destroy(vulkan_ctx_t *v) {
    (void)v;
}

bool vulkan_sync_neurons(vulkan_ctx_t *v, substrate_t *s) {
    (void)v;
    (void)s;
    return false;
}

size_t vulkan_similarity_search(vulkan_ctx_t *v, substrate_t *s, const engram_vec_t query,
                                 engram_id_t *results, float *scores, 
                                 size_t max_results, float threshold) {
    (void)v;
    (void)s;
    (void)query;
    (void)results;
    (void)scores;
    (void)max_results;
    (void)threshold;
    return 0;
}

#endif
