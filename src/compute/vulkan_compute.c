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

#else

vulkan_ctx_t vulkan_create(void) {
    vulkan_ctx_t v = {0};
    return v;
}

void vulkan_destroy(vulkan_ctx_t *v) {
    (void)v;
}

#endif
