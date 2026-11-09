#include <jni.h>
#include <string>
#include <vector>
#include <algorithm>
#include <atomic>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <android/asset_manager_jni.h>
#include <android/asset_manager.h>
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

static std::atomic<bool> g_stopRendering{false};

static bool hasInstanceExtension(const char* name) {
    uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkExtensionProperties> values(count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, values.data()) != VK_SUCCESS) return false;
    for (const auto& value : values) if (std::string(value.extensionName) == name) return true;
    return false;
}

static bool hasDeviceExtension(VkPhysicalDevice device, const char* name) {
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkExtensionProperties> values(count);
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, values.data()) != VK_SUCCESS) return false;
    for (const auto& value : values) if (std::string(value.extensionName) == name) return true;
    return false;
}

static bool loadShader(AAssetManager* manager, const char* name, std::vector<uint32_t>& code) {
    if (!manager) return false;
    AAsset* asset = AAssetManager_open(manager, name, AASSET_MODE_BUFFER);
    if (!asset) return false;
    const off_t length = AAsset_getLength(asset);
    if (length <= 0 || (length % 4) != 0) {
        AAsset_close(asset);
        return false;
    }
    code.resize(static_cast<size_t>(length) / sizeof(uint32_t));
    const int bytes = AAsset_read(asset, code.data(), static_cast<size_t>(length));
    AAsset_close(asset);
    return bytes == length && !code.empty() && code[0] == 0x07230203;
}

extern "C" JNIEXPORT void JNICALL
Java_com_p5s_android_MainActivity_requestStopVulkanRendering(JNIEnv*, jobject) {
    g_stopRendering.store(true);
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_p5s_android_MainActivity_getVulkanInfo(JNIEnv* env, jobject) {
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "P5S Android Diagnostic";
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo info{};
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pApplicationInfo = &app;
    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&info, nullptr, &instance);
    if (result != VK_SUCCESS) {
        std::string message = "Não foi possível inicializar Vulkan. Código: " + std::to_string(result);
        return env->NewStringUTF(message.c_str());
    }
    uint32_t count = 0;
    result = vkEnumeratePhysicalDevices(instance, &count, nullptr);
    if (result != VK_SUCCESS || count == 0) {
        vkDestroyInstance(instance, nullptr);
        return env->NewStringUTF("Vulkan iniciou, mas nenhum dispositivo físico foi encontrado.");
    }
    std::vector<VkPhysicalDevice> devices(count);
    vkEnumeratePhysicalDevices(instance, &count, devices.data());
    std::string output = "Vulkan inicializado com sucesso.\nDispositivos encontrados: " + std::to_string(count);
    for (uint32_t i = 0; i < count; ++i) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        output += "\n\nDispositivo " + std::to_string(i + 1) + ": " + properties.deviceName;
        output += "\nAPI Vulkan: " + std::to_string(VK_VERSION_MAJOR(properties.apiVersion)) + "." +
                  std::to_string(VK_VERSION_MINOR(properties.apiVersion)) + "." +
                  std::to_string(VK_VERSION_PATCH(properties.apiVersion));
    }
    vkDestroyInstance(instance, nullptr);
    return env->NewStringUTF(output.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_p5s_android_MainActivity_renderVulkanLoop(JNIEnv* env, jobject, jobject javaSurface, jobject javaAssets) {
    g_stopRendering.store(false);
    if (!javaSurface || !javaAssets) return env->NewStringUTF("ERRO: superfície ou assets Android inválidos.");
    if (!hasInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME) ||
        !hasInstanceExtension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
        return env->NewStringUTF("ERRO: extensões de superfície Android indisponíveis.");
    }

    AAssetManager* assets = AAssetManager_fromJava(env, javaAssets);
    std::vector<uint32_t> vertexCode, fragmentCode;
    if (!loadShader(assets, "triangle.vert.spv", vertexCode) ||
        !loadShader(assets, "triangle.frag.spv", fragmentCode)) {
        return env->NewStringUTF("ERRO: não foi possível carregar os shaders SPIR-V do APK.");
    }

    const char* instanceExtensions[] = { VK_KHR_SURFACE_EXTENSION_NAME, VK_KHR_ANDROID_SURFACE_EXTENSION_NAME };
    VkApplicationInfo app{};
    app.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    app.pApplicationName = "P5S Android Vulkan Triangle";
    app.apiVersion = VK_API_VERSION_1_0;
    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &app;
    instanceInfo.enabledExtensionCount = 2;
    instanceInfo.ppEnabledExtensionNames = instanceExtensions;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE, renderFinished = VK_NULL_HANDLE;
    VkFence frameFence = VK_NULL_HANDLE;
    VkRenderPass renderPass = VK_NULL_HANDLE;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkPipeline pipeline = VK_NULL_HANDLE;
    VkShaderModule vertexModule = VK_NULL_HANDLE, fragmentModule = VK_NULL_HANDLE;
    ANativeWindow* window = ANativeWindow_fromSurface(env, javaSurface);
    std::vector<VkImageView> imageViews;
    std::vector<VkFramebuffer> framebuffers;

    auto cleanup = [&]() {
        if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);
        if (device != VK_NULL_HANDLE) {
            for (VkFramebuffer fb : framebuffers) if (fb) vkDestroyFramebuffer(device, fb, nullptr);
            if (pipeline) vkDestroyPipeline(device, pipeline, nullptr);
            if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            if (vertexModule) vkDestroyShaderModule(device, vertexModule, nullptr);
            if (fragmentModule) vkDestroyShaderModule(device, fragmentModule, nullptr);
            if (renderPass) vkDestroyRenderPass(device, renderPass, nullptr);
            for (VkImageView view : imageViews) if (view) vkDestroyImageView(device, view, nullptr);
            if (imageAvailable) vkDestroySemaphore(device, imageAvailable, nullptr);
            if (renderFinished) vkDestroySemaphore(device, renderFinished, nullptr);
            if (frameFence) vkDestroyFence(device, frameFence, nullptr);
            if (commandPool) vkDestroyCommandPool(device, commandPool, nullptr);
            if (swapchain) vkDestroySwapchainKHR(device, swapchain, nullptr);
            vkDestroyDevice(device, nullptr);
        }
        if (surface && instance) vkDestroySurfaceKHR(instance, surface, nullptr);
        if (instance) vkDestroyInstance(instance, nullptr);
        if (window) ANativeWindow_release(window);
    };
    auto fail = [&](const std::string& message) -> jstring {
        cleanup();
        return env->NewStringUTF(message.c_str());
    };

    if (!window) return fail("ERRO: não foi possível obter a janela nativa Android.");
    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (result != VK_SUCCESS) return fail("ERRO ao criar instância Vulkan: " + std::to_string(result));
    auto createSurface = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
        vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR"));
    if (!createSurface) return fail("ERRO: função de superfície Android indisponível.");
    VkAndroidSurfaceCreateInfoKHR androidInfo{};
    androidInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    androidInfo.window = window;
    result = createSurface(instance, &androidInfo, nullptr, &surface);
    if (result != VK_SUCCESS) return fail("ERRO ao criar superfície: " + std::to_string(result));

    uint32_t physicalCount = 0;
    result = vkEnumeratePhysicalDevices(instance, &physicalCount, nullptr);
    if (result != VK_SUCCESS || physicalCount == 0) return fail("ERRO: nenhum dispositivo Vulkan encontrado.");
    std::vector<VkPhysicalDevice> physicalDevices(physicalCount);
    result = vkEnumeratePhysicalDevices(instance, &physicalCount, physicalDevices.data());
    if (result != VK_SUCCESS) return fail("ERRO ao enumerar GPUs.");

    VkPhysicalDevice physical = VK_NULL_HANDLE;
    uint32_t queueIndex = UINT32_MAX;
    VkPhysicalDeviceProperties properties{};
    for (VkPhysicalDevice candidate : physicalDevices) {
        if (!hasDeviceExtension(candidate, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());
        for (uint32_t i = 0; i < familyCount; ++i) {
            VkBool32 present = VK_FALSE;
            if (!(families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT)) continue;
            if (vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &present) == VK_SUCCESS && present) {
                physical = candidate;
                queueIndex = i;
                vkGetPhysicalDeviceProperties(candidate, &properties);
                break;
            }
        }
        if (physical) break;
    }
    if (!physical) return fail("ERRO: não foi encontrada uma fila gráfica com suporte à apresentação.");

    VkSurfaceCapabilitiesKHR caps{};
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physical, surface, &caps);
    if (result != VK_SUCCESS) return fail("ERRO ao consultar superfície: " + std::to_string(result));
    if (!(caps.supportedUsageFlags & VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT))
        return fail("ERRO: a superfície não aceita imagens como color attachment.");

    uint32_t formatCount = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &formatCount, nullptr);
    if (result != VK_SUCCESS || !formatCount) return fail("ERRO: nenhum formato de superfície disponível.");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physical, surface, &formatCount, formats.data());
    VkSurfaceFormatKHR format = formats[0];
    for (const auto& candidate : formats) {
        if (candidate.format == VK_FORMAT_R8G8B8A8_UNORM || candidate.format == VK_FORMAT_B8G8R8A8_UNORM ||
            candidate.format == VK_FORMAT_R8G8B8A8_SRGB || candidate.format == VK_FORMAT_B8G8R8A8_SRGB) {
            format = candidate;
            break;
        }
    }

    VkExtent2D extent = caps.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = static_cast<uint32_t>(ANativeWindow_getWidth(window));
        extent.height = static_cast<uint32_t>(ANativeWindow_getHeight(window));
        extent.width = std::max(caps.minImageExtent.width, std::min(caps.maxImageExtent.width, extent.width));
        extent.height = std::max(caps.minImageExtent.height, std::min(caps.maxImageExtent.height, extent.height));
    }
    if (!extent.width || !extent.height) return fail("ERRO: dimensões da superfície são zero.");

    uint32_t desiredImages = caps.minImageCount + 1;
    if (caps.maxImageCount && desiredImages > caps.maxImageCount) desiredImages = caps.maxImageCount;
    float priority = 1.0f;
    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueIndex;
    queueInfo.queueCount = 1;
    queueInfo.pQueuePriorities = &priority;
    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;
    result = vkCreateDevice(physical, &deviceInfo, nullptr, &device);
    if (result != VK_SUCCESS) return fail("ERRO ao criar dispositivo lógico: " + std::to_string(result));
    VkQueue queue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queueIndex, 0, &queue);

    VkCompositeAlphaFlagBitsKHR alpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    const VkCompositeAlphaFlagBitsKHR alphaOptions[] = { VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR, VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR };
    for (auto option : alphaOptions) if (caps.supportedCompositeAlpha & option) { alpha = option; break; }

    VkSwapchainCreateInfoKHR swapInfo{};
    swapInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapInfo.surface = surface;
    swapInfo.minImageCount = desiredImages;
    swapInfo.imageFormat = format.format;
    swapInfo.imageColorSpace = format.colorSpace;
    swapInfo.imageExtent = extent;
    swapInfo.imageArrayLayers = 1;
    swapInfo.imageUsage = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;
    swapInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapInfo.preTransform = caps.currentTransform;
    swapInfo.compositeAlpha = alpha;
    swapInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapInfo.clipped = VK_TRUE;
    result = vkCreateSwapchainKHR(device, &swapInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS) return fail("ERRO ao criar swapchain: " + std::to_string(result));

    uint32_t imageCount = 0;
    vkGetSwapchainImagesKHR(device, swapchain, &imageCount, nullptr);
    if (!imageCount) return fail("ERRO: swapchain sem imagens.");
    std::vector<VkImage> images(imageCount);
    result = vkGetSwapchainImagesKHR(device, swapchain, &imageCount, images.data());
    if (result != VK_SUCCESS) return fail("ERRO ao obter imagens da swapchain.");

    imageViews.resize(imageCount, VK_NULL_HANDLE);
    framebuffers.resize(imageCount, VK_NULL_HANDLE);
    for (uint32_t i = 0; i < imageCount; ++i) {
        VkImageViewCreateInfo viewInfo{};
        viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
        viewInfo.image = images[i];
        viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
        viewInfo.format = format.format;
        viewInfo.components = { VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY,
                                VK_COMPONENT_SWIZZLE_IDENTITY, VK_COMPONENT_SWIZZLE_IDENTITY };
        viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        viewInfo.subresourceRange.levelCount = 1;
        viewInfo.subresourceRange.layerCount = 1;
        result = vkCreateImageView(device, &viewInfo, nullptr, &imageViews[i]);
        if (result != VK_SUCCESS) return fail("ERRO ao criar image view: " + std::to_string(result));
    }

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    renderInfo.attachmentCount = 1;
    renderInfo.pAttachments = &colorAttachment;
    renderInfo.subpassCount = 1;
    renderInfo.pSubpasses = &subpass;
    renderInfo.dependencyCount = 1;
    renderInfo.pDependencies = &dependency;
    result = vkCreateRenderPass(device, &renderInfo, nullptr, &renderPass);
    if (result != VK_SUCCESS) return fail("ERRO ao criar render pass: " + std::to_string(result));

    for (uint32_t i = 0; i < imageCount; ++i) {
        VkFramebufferCreateInfo fbInfo{};
        fbInfo.sType = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
        fbInfo.renderPass = renderPass;
        fbInfo.attachmentCount = 1;
        fbInfo.pAttachments = &imageViews[i];
        fbInfo.width = extent.width;
        fbInfo.height = extent.height;
        fbInfo.layers = 1;
        result = vkCreateFramebuffer(device, &fbInfo, nullptr, &framebuffers[i]);
        if (result != VK_SUCCESS) return fail("ERRO ao criar framebuffer: " + std::to_string(result));
    }

    VkShaderModuleCreateInfo shaderInfo{};
    shaderInfo.sType = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
    shaderInfo.codeSize = vertexCode.size() * sizeof(uint32_t);
    shaderInfo.pCode = vertexCode.data();
    result = vkCreateShaderModule(device, &shaderInfo, nullptr, &vertexModule);
    if (result != VK_SUCCESS) return fail("ERRO ao criar shader de vértice: " + std::to_string(result));
    shaderInfo.codeSize = fragmentCode.size() * sizeof(uint32_t);
    shaderInfo.pCode = fragmentCode.data();
    result = vkCreateShaderModule(device, &shaderInfo, nullptr, &fragmentModule);
    if (result != VK_SUCCESS) return fail("ERRO ao criar shader de fragmento: " + std::to_string(result));

    VkPipelineShaderStageCreateInfo stages[2]{};
    stages[0].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[0].stage = VK_SHADER_STAGE_VERTEX_BIT;
    stages[0].module = vertexModule;
    stages[0].pName = "main";
    stages[1].sType = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
    stages[1].stage = VK_SHADER_STAGE_FRAGMENT_BIT;
    stages[1].module = fragmentModule;
    stages[1].pName = "main";

    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    VkPipelineInputAssemblyStateCreateInfo assembly{};
    assembly.sType = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
    assembly.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    VkViewport viewport{};
    viewport.width = static_cast<float>(extent.width);
    viewport.height = static_cast<float>(extent.height);
    viewport.minDepth = 0.0f;
    viewport.maxDepth = 1.0f;
    VkRect2D scissor{};
    scissor.extent = extent;
    VkPipelineViewportStateCreateInfo viewportState{};
    viewportState.sType = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
    viewportState.viewportCount = 1;
    viewportState.pViewports = &viewport;
    viewportState.scissorCount = 1;
    viewportState.pScissors = &scissor;
    VkPipelineRasterizationStateCreateInfo raster{};
    raster.sType = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
    raster.polygonMode = VK_POLYGON_MODE_FILL;
    raster.cullMode = VK_CULL_MODE_NONE;
    raster.frontFace = VK_FRONT_FACE_COUNTER_CLOCKWISE;
    raster.lineWidth = 1.0f;
    VkPipelineMultisampleStateCreateInfo multisample{};
    multisample.sType = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
    multisample.rasterizationSamples = VK_SAMPLE_COUNT_1_BIT;
    VkPipelineColorBlendAttachmentState blendAttachment{};
    blendAttachment.colorWriteMask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT |
                                     VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
    VkPipelineColorBlendStateCreateInfo blend{};
    blend.sType = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
    blend.attachmentCount = 1;
    blend.pAttachments = &blendAttachment;
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    result = vkCreatePipelineLayout(device, &layoutInfo, nullptr, &pipelineLayout);
    if (result != VK_SUCCESS) return fail("ERRO ao criar pipeline layout: " + std::to_string(result));

    VkGraphicsPipelineCreateInfo pipelineInfo{};
    pipelineInfo.sType = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
    pipelineInfo.stageCount = 2;
    pipelineInfo.pStages = stages;
    pipelineInfo.pVertexInputState = &vertexInput;
    pipelineInfo.pInputAssemblyState = &assembly;
    pipelineInfo.pViewportState = &viewportState;
    pipelineInfo.pRasterizationState = &raster;
    pipelineInfo.pMultisampleState = &multisample;
    pipelineInfo.pColorBlendState = &blend;
    pipelineInfo.layout = pipelineLayout;
    pipelineInfo.renderPass = renderPass;
    pipelineInfo.subpass = 0;
    result = vkCreateGraphicsPipelines(device, VK_NULL_HANDLE, 1, &pipelineInfo, nullptr, &pipeline);
    if (result != VK_SUCCESS) return fail("ERRO ao criar pipeline gráfica: " + std::to_string(result));

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueIndex;
    result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
    if (result != VK_SUCCESS) return fail("ERRO ao criar command pool: " + std::to_string(result));
    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable);
    if (result != VK_SUCCESS) return fail("ERRO ao criar semáforo: " + std::to_string(result));
    result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished);
    if (result != VK_SUCCESS) return fail("ERRO ao criar semáforo: " + std::to_string(result));

    VkCommandBufferAllocateInfo commandAlloc{};
    commandAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAlloc.commandPool = commandPool;
    commandAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandAlloc.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    result = vkAllocateCommandBuffers(device, &commandAlloc, &command);
    if (result != VK_SUCCESS) return fail("ERRO ao alocar command buffer: " + std::to_string(result));

    VkFenceCreateInfo fenceInfo{};
    fenceInfo.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
    fenceInfo.flags = VK_FENCE_CREATE_SIGNALED_BIT;
    result = vkCreateFence(device, &fenceInfo, nullptr, &frameFence);
    if (result != VK_SUCCESS) return fail("ERRO ao criar fence de renderização: " + std::to_string(result));

    uint64_t renderedFrames = 0;
    bool interrupted = false;
    while (!g_stopRendering.load()) {
        result = vkWaitForFences(device, 1, &frameFence, VK_TRUE, UINT64_MAX);
        if (result != VK_SUCCESS) return fail("ERRO ao aguardar frame anterior: " + std::to_string(result));

        uint32_t imageIndex = 0;
        result = vkAcquireNextImageKHR(device, swapchain, 100000000ULL,
                                       imageAvailable, VK_NULL_HANDLE, &imageIndex);
        if (result == VK_TIMEOUT || result == VK_NOT_READY) continue;
        if (result == VK_ERROR_OUT_OF_DATE_KHR) {
            interrupted = true;
            break;
        }
        if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR)
            return fail("ERRO ao adquirir imagem: " + std::to_string(result));

        result = vkResetFences(device, 1, &frameFence);
        if (result != VK_SUCCESS) return fail("ERRO ao reiniciar fence: " + std::to_string(result));
        result = vkResetCommandBuffer(command, 0);
        if (result != VK_SUCCESS) return fail("ERRO ao reiniciar command buffer: " + std::to_string(result));

        VkCommandBufferBeginInfo begin{};
        begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
        begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
        result = vkBeginCommandBuffer(command, &begin);
        if (result != VK_SUCCESS) return fail("ERRO ao iniciar command buffer: " + std::to_string(result));

        VkClearValue clear{};
        clear.color = {{ 0.055f, 0.025f, 0.10f, 1.0f }};
        VkRenderPassBeginInfo passBegin{};
        passBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        passBegin.renderPass = renderPass;
        passBegin.framebuffer = framebuffers[imageIndex];
        passBegin.renderArea.extent = extent;
        passBegin.clearValueCount = 1;
        passBegin.pClearValues = &clear;
        vkCmdBeginRenderPass(command, &passBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdDraw(command, 3, 1, 0, 0);
        vkCmdEndRenderPass(command);
        result = vkEndCommandBuffer(command);
        if (result != VK_SUCCESS) return fail("ERRO ao finalizar command buffer: " + std::to_string(result));

        VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.waitSemaphoreCount = 1;
        submit.pWaitSemaphores = &imageAvailable;
        submit.pWaitDstStageMask = &waitStage;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        submit.signalSemaphoreCount = 1;
        submit.pSignalSemaphores = &renderFinished;
        result = vkQueueSubmit(queue, 1, &submit, frameFence);
        if (result != VK_SUCCESS) return fail("ERRO ao enviar desenho à GPU: " + std::to_string(result));

        VkPresentInfoKHR present{};
        present.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
        present.waitSemaphoreCount = 1;
        present.pWaitSemaphores = &renderFinished;
        present.swapchainCount = 1;
        present.pSwapchains = &swapchain;
        present.pImageIndices = &imageIndex;
        VkResult presentResult = vkQueuePresentKHR(queue, &present);
        if (presentResult != VK_SUCCESS && presentResult != VK_SUBOPTIMAL_KHR &&
            presentResult != VK_ERROR_OUT_OF_DATE_KHR)
            return fail("ERRO ao apresentar triângulo: " + std::to_string(presentResult));
        renderedFrames++;

        // Sincronização conservadora para permitir reutilizar o semáforo com segurança.
        result = vkQueueWaitIdle(queue);
        if (result != VK_SUCCESS) return fail("ERRO ao sincronizar fila gráfica: " + std::to_string(result));
        if (presentResult == VK_ERROR_OUT_OF_DATE_KHR || presentResult == VK_SUBOPTIMAL_KHR) {
            interrupted = true;
            break;
        }
    }

    if (device != VK_NULL_HANDLE && frameFence) {
        vkDestroyFence(device, frameFence, nullptr);
        frameFence = VK_NULL_HANDLE;
    }
    std::string output = "LOOP DE RENDERIZAÇÃO VULKAN ENCERRADO!\n";
    output += "GPU: " + std::string(properties.deviceName);
    output += "\nResolução: " + std::to_string(extent.width) + " x " + std::to_string(extent.height);
    output += "\nImagens na swapchain: " + std::to_string(imageCount);
    output += "\nPipeline gráfica: ativa";
    output += "\nGeometria: 3 vértices via gl_VertexIndex";
    output += "\nShader de fragmento: rosa";
    output += "\nFrames apresentados: " + std::to_string(renderedFrames);
    output += interrupted ? "\nMotivo: superfície redimensionada ou recriada." : "\nMotivo: ciclo interrompido pelo Android.";
    cleanup();
    return env->NewStringUTF(output.c_str());
}
