#include <jni.h>
#include <string>
#include <vector>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

static bool hasInstanceExtension(const char* extensionName) {
    uint32_t count = 0;
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateInstanceExtensionProperties(nullptr, &count, extensions.data()) != VK_SUCCESS) return false;
    for (const auto& extension : extensions) {
        if (std::string(extension.extensionName) == extensionName) return true;
    }
    return false;
}

static bool hasDeviceExtension(VkPhysicalDevice device, const char* extensionName) {
    uint32_t count = 0;
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, nullptr) != VK_SUCCESS) return false;
    std::vector<VkExtensionProperties> extensions(count);
    if (vkEnumerateDeviceExtensionProperties(device, nullptr, &count, extensions.data()) != VK_SUCCESS) return false;
    for (const auto& extension : extensions) {
        if (std::string(extension.extensionName) == extensionName) return true;
    }
    return false;
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_p5s_android_MainActivity_getVulkanInfo(JNIEnv *env, jobject) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "P5S Android Diagnostic";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 3, 0);
    appInfo.pEngineName = "Diagnostic";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 3, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo createInfo{};
    createInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    createInfo.pApplicationInfo = &appInfo;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult createResult = vkCreateInstance(&createInfo, nullptr, &instance);
    if (createResult != VK_SUCCESS) {
        std::string message = "Não foi possível inicializar Vulkan. Código VkResult: " + std::to_string(createResult);
        return env->NewStringUTF(message.c_str());
    }

    uint32_t deviceCount = 0;
    VkResult countResult = vkEnumeratePhysicalDevices(instance, &deviceCount, nullptr);
    if (countResult != VK_SUCCESS || deviceCount == 0) {
        vkDestroyInstance(instance, nullptr);
        return env->NewStringUTF("Vulkan iniciou, mas nenhum dispositivo físico compatível foi encontrado.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VkResult devicesResult = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    if (devicesResult != VK_SUCCESS) {
        vkDestroyInstance(instance, nullptr);
        return env->NewStringUTF("Falha ao consultar os dispositivos Vulkan.");
    }

    std::string output = "Vulkan inicializado com sucesso.\nDispositivos encontrados: " + std::to_string(deviceCount);
    for (uint32_t i = 0; i < deviceCount; ++i) {
        VkPhysicalDeviceProperties properties{};
        vkGetPhysicalDeviceProperties(devices[i], &properties);
        output += "\n\nDispositivo " + std::to_string(i + 1) + ": " + properties.deviceName;
        output += "\nAPI Vulkan: " + std::to_string(VK_VERSION_MAJOR(properties.apiVersion)) + ".";
        output += std::to_string(VK_VERSION_MINOR(properties.apiVersion)) + ".";
        output += std::to_string(VK_VERSION_PATCH(properties.apiVersion));
    }

    vkDestroyInstance(instance, nullptr);
    return env->NewStringUTF(output.c_str());
}

extern "C" JNIEXPORT jstring JNICALL
Java_com_p5s_android_MainActivity_renderVulkanFrame(JNIEnv *env, jobject, jobject javaSurface) {
    if (javaSurface == nullptr) return env->NewStringUTF("Superfície Android inválida.");

    if (!hasInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME) ||
        !hasInstanceExtension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
        return env->NewStringUTF("ERRO: extensões Vulkan de superfície Android indisponíveis.");
    }

    const char* instanceExtensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "P5S Android Vulkan Frame Test";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 3, 0);
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = 2;
    instanceInfo.ppEnabledExtensionNames = instanceExtensions;

    VkInstance instance = VK_NULL_HANDLE;
    VkSurfaceKHR surface = VK_NULL_HANDLE;
    VkDevice device = VK_NULL_HANDLE;
    VkSwapchainKHR swapchain = VK_NULL_HANDLE;
    VkCommandPool commandPool = VK_NULL_HANDLE;
    VkSemaphore imageAvailable = VK_NULL_HANDLE;
    VkSemaphore renderFinished = VK_NULL_HANDLE;
    ANativeWindow* window = ANativeWindow_fromSurface(env, javaSurface);

    auto cleanup = [&]() {
        if (device != VK_NULL_HANDLE) vkDeviceWaitIdle(device);
        if (imageAvailable != VK_NULL_HANDLE) vkDestroySemaphore(device, imageAvailable, nullptr);
        if (renderFinished != VK_NULL_HANDLE) vkDestroySemaphore(device, renderFinished, nullptr);
        if (commandPool != VK_NULL_HANDLE) vkDestroyCommandPool(device, commandPool, nullptr);
        if (swapchain != VK_NULL_HANDLE) vkDestroySwapchainKHR(device, swapchain, nullptr);
        if (device != VK_NULL_HANDLE) vkDestroyDevice(device, nullptr);
        if (surface != VK_NULL_HANDLE) vkDestroySurfaceKHR(instance, surface, nullptr);
        if (instance != VK_NULL_HANDLE) vkDestroyInstance(instance, nullptr);
        if (window != nullptr) ANativeWindow_release(window);
    };
    auto fail = [&](const std::string& message) -> jstring {
        cleanup();
        return env->NewStringUTF(message.c_str());
    };

    if (window == nullptr) return fail("ERRO: não foi possível obter a janela nativa Android.");

    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (result != VK_SUCCESS) return fail("ERRO ao criar instância Vulkan: " + std::to_string(result));

    auto createAndroidSurface = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
        vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR"));
    if (createAndroidSurface == nullptr) return fail("ERRO: função de superfície Android indisponível.");

    VkAndroidSurfaceCreateInfoKHR androidSurfaceInfo{};
    androidSurfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    androidSurfaceInfo.window = window;
    result = createAndroidSurface(instance, &androidSurfaceInfo, nullptr, &surface);
    if (result != VK_SUCCESS) return fail("ERRO ao criar superfície: " + std::to_string(result));

    uint32_t physicalCount = 0;
    result = vkEnumeratePhysicalDevices(instance, &physicalCount, nullptr);
    if (result != VK_SUCCESS || physicalCount == 0) return fail("ERRO: nenhum dispositivo Vulkan encontrado.");

    std::vector<VkPhysicalDevice> physicalDevices(physicalCount);
    result = vkEnumeratePhysicalDevices(instance, &physicalCount, physicalDevices.data());
    if (result != VK_SUCCESS) return fail("ERRO ao enumerar dispositivos Vulkan.");

    VkPhysicalDevice physicalDevice = VK_NULL_HANDLE;
    uint32_t queueFamilyIndex = UINT32_MAX;
    VkPhysicalDeviceProperties physicalProperties{};

    for (const auto& candidate : physicalDevices) {
        if (!hasDeviceExtension(candidate, VK_KHR_SWAPCHAIN_EXTENSION_NAME)) continue;
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(candidate, &familyCount, families.data());
        for (uint32_t i = 0; i < familyCount; ++i) {
            VkBool32 present = VK_FALSE;
            if ((families[i].queueFlags & VK_QUEUE_GRAPHICS_BIT) == 0) continue;
            VkResult support = vkGetPhysicalDeviceSurfaceSupportKHR(candidate, i, surface, &present);
            if (support == VK_SUCCESS && present) {
                physicalDevice = candidate;
                queueFamilyIndex = i;
                vkGetPhysicalDeviceProperties(candidate, &physicalProperties);
                break;
            }
        }
        if (physicalDevice != VK_NULL_HANDLE) break;
    }

    if (physicalDevice == VK_NULL_HANDLE) {
        return fail("ERRO: não foi encontrada uma fila gráfica com suporte à apresentação e à extensão VK_KHR_swapchain.");
    }

    VkSurfaceCapabilitiesKHR capabilities{};
    result = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(physicalDevice, surface, &capabilities);
    if (result != VK_SUCCESS) return fail("ERRO ao consultar capacidades da superfície: " + std::to_string(result));

    if ((capabilities.supportedUsageFlags & VK_IMAGE_USAGE_TRANSFER_DST_BIT) == 0) {
        return fail("A GPU suporta apresentação, mas não permite limpar diretamente as imagens da tela por transferência. Será necessário usar uma etapa de renderização por attachment.");
    }

    uint32_t formatCount = 0;
    result = vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, nullptr);
    if (result != VK_SUCCESS || formatCount == 0) return fail("ERRO: nenhum formato de imagem de superfície disponível.");
    std::vector<VkSurfaceFormatKHR> formats(formatCount);
    vkGetPhysicalDeviceSurfaceFormatsKHR(physicalDevice, surface, &formatCount, formats.data());

    VkSurfaceFormatKHR surfaceFormat = formats[0];
    for (const auto& candidate : formats) {
        if (candidate.format == VK_FORMAT_R8G8B8A8_UNORM ||
            candidate.format == VK_FORMAT_B8G8R8A8_UNORM) {
            surfaceFormat = candidate;
            break;
        }
    }

    VkExtent2D extent = capabilities.currentExtent;
    if (extent.width == UINT32_MAX) {
        extent.width = static_cast<uint32_t>(ANativeWindow_getWidth(window));
        extent.height = static_cast<uint32_t>(ANativeWindow_getHeight(window));
        extent.width = std::max(capabilities.minImageExtent.width,
            std::min(capabilities.maxImageExtent.width, extent.width));
        extent.height = std::max(capabilities.minImageExtent.height,
            std::min(capabilities.maxImageExtent.height, extent.height));
    }
    if (extent.width == 0 || extent.height == 0) return fail("ERRO: dimensões da superfície são zero.");

    uint32_t imageCount = capabilities.minImageCount + 1;
    if (capabilities.maxImageCount > 0 && imageCount > capabilities.maxImageCount) {
        imageCount = capabilities.maxImageCount;
    }

    VkDeviceQueueCreateInfo queueInfo{};
    queueInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueInfo.queueFamilyIndex = queueFamilyIndex;
    queueInfo.queueCount = 1;
    const float queuePriority = 1.0f;
    queueInfo.pQueuePriorities = &queuePriority;

    const char* deviceExtensions[] = { VK_KHR_SWAPCHAIN_EXTENSION_NAME };
    VkDeviceCreateInfo deviceInfo{};
    deviceInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceInfo.queueCreateInfoCount = 1;
    deviceInfo.pQueueCreateInfos = &queueInfo;
    deviceInfo.enabledExtensionCount = 1;
    deviceInfo.ppEnabledExtensionNames = deviceExtensions;

    result = vkCreateDevice(physicalDevice, &deviceInfo, nullptr, &device);
    if (result != VK_SUCCESS) return fail("ERRO ao criar dispositivo lógico Vulkan: " + std::to_string(result));

    VkQueue graphicsQueue = VK_NULL_HANDLE;
    vkGetDeviceQueue(device, queueFamilyIndex, 0, &graphicsQueue);

    VkCompositeAlphaFlagBitsKHR compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
    const VkCompositeAlphaFlagBitsKHR alphaModes[] = {
        VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR,
        VK_COMPOSITE_ALPHA_PRE_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_POST_MULTIPLIED_BIT_KHR,
        VK_COMPOSITE_ALPHA_INHERIT_BIT_KHR
    };
    for (VkCompositeAlphaFlagBitsKHR mode : alphaModes) {
        if (capabilities.supportedCompositeAlpha & mode) {
            compositeAlpha = mode;
            break;
        }
    }

    VkSwapchainCreateInfoKHR swapchainInfo{};
    swapchainInfo.sType = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
    swapchainInfo.surface = surface;
    swapchainInfo.minImageCount = imageCount;
    swapchainInfo.imageFormat = surfaceFormat.format;
    swapchainInfo.imageColorSpace = surfaceFormat.colorSpace;
    swapchainInfo.imageExtent = extent;
    swapchainInfo.imageArrayLayers = 1;
    swapchainInfo.imageUsage = VK_IMAGE_USAGE_TRANSFER_DST_BIT;
    swapchainInfo.imageSharingMode = VK_SHARING_MODE_EXCLUSIVE;
    swapchainInfo.preTransform = capabilities.currentTransform;
    swapchainInfo.compositeAlpha = compositeAlpha;
    swapchainInfo.presentMode = VK_PRESENT_MODE_FIFO_KHR;
    swapchainInfo.clipped = VK_TRUE;

    result = vkCreateSwapchainKHR(device, &swapchainInfo, nullptr, &swapchain);
    if (result != VK_SUCCESS) return fail("ERRO ao criar swapchain: " + std::to_string(result));

    uint32_t swapchainImageCount = 0;
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, nullptr);
    if (result != VK_SUCCESS || swapchainImageCount == 0) return fail("ERRO ao obter imagens da swapchain.");
    std::vector<VkImage> swapchainImages(swapchainImageCount);
    result = vkGetSwapchainImagesKHR(device, swapchain, &swapchainImageCount, swapchainImages.data());
    if (result != VK_SUCCESS) return fail("ERRO ao enumerar imagens da swapchain.");

    VkCommandPoolCreateInfo poolInfo{};
    poolInfo.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
    poolInfo.flags = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;
    poolInfo.queueFamilyIndex = queueFamilyIndex;
    result = vkCreateCommandPool(device, &poolInfo, nullptr, &commandPool);
    if (result != VK_SUCCESS) return fail("ERRO ao criar command pool: " + std::to_string(result));

    VkSemaphoreCreateInfo semaphoreInfo{};
    semaphoreInfo.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;
    result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &imageAvailable);
    if (result != VK_SUCCESS) return fail("ERRO ao criar semáforo de aquisição: " + std::to_string(result));
    result = vkCreateSemaphore(device, &semaphoreInfo, nullptr, &renderFinished);
    if (result != VK_SUCCESS) return fail("ERRO ao criar semáforo de renderização: " + std::to_string(result));

    uint32_t imageIndex = 0;
    result = vkAcquireNextImageKHR(device, swapchain, UINT64_MAX, imageAvailable, VK_NULL_HANDLE, &imageIndex);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return fail("ERRO ao adquirir imagem da swapchain: " + std::to_string(result));
    }

    VkCommandBufferAllocateInfo commandAlloc{};
    commandAlloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAlloc.commandPool = commandPool;
    commandAlloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandAlloc.commandBufferCount = 1;
    VkCommandBuffer commandBuffer = VK_NULL_HANDLE;
    result = vkAllocateCommandBuffers(device, &commandAlloc, &commandBuffer);
    if (result != VK_SUCCESS) return fail("ERRO ao alocar command buffer: " + std::to_string(result));

    VkCommandBufferBeginInfo beginInfo{};
    beginInfo.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    beginInfo.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    result = vkBeginCommandBuffer(commandBuffer, &beginInfo);
    if (result != VK_SUCCESS) return fail("ERRO ao iniciar command buffer: " + std::to_string(result));

    VkImageMemoryBarrier toTransfer{};
    toTransfer.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toTransfer.srcAccessMask = 0;
    toTransfer.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toTransfer.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    toTransfer.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toTransfer.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toTransfer.image = swapchainImages[imageIndex];
    toTransfer.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    toTransfer.subresourceRange.baseMipLevel = 0;
    toTransfer.subresourceRange.levelCount = 1;
    toTransfer.subresourceRange.baseArrayLayer = 0;
    toTransfer.subresourceRange.layerCount = 1;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT,
        VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 0, nullptr, 0, nullptr, 1, &toTransfer);

    VkClearColorValue clearColor{};
    clearColor.float32[0] = 0.20f;
    clearColor.float32[1] = 0.02f;
    clearColor.float32[2] = 0.32f;
    clearColor.float32[3] = 1.0f;
    VkImageSubresourceRange range{};
    range.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    range.baseMipLevel = 0;
    range.levelCount = 1;
    range.baseArrayLayer = 0;
    range.layerCount = 1;
    vkCmdClearColorImage(commandBuffer, swapchainImages[imageIndex],
        VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &clearColor, 1, &range);

    VkImageMemoryBarrier toPresent{};
    toPresent.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
    toPresent.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
    toPresent.dstAccessMask = 0;
    toPresent.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
    toPresent.newLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    toPresent.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
    toPresent.image = swapchainImages[imageIndex];
    toPresent.subresourceRange = range;
    vkCmdPipelineBarrier(commandBuffer, VK_PIPELINE_STAGE_TRANSFER_BIT,
        VK_PIPELINE_STAGE_BOTTOM_OF_PIPE_BIT, 0, 0, nullptr, 0, nullptr, 1, &toPresent);

    result = vkEndCommandBuffer(commandBuffer);
    if (result != VK_SUCCESS) return fail("ERRO ao finalizar command buffer: " + std::to_string(result));

    VkPipelineStageFlags waitStage = VK_PIPELINE_STAGE_TRANSFER_BIT;
    VkSubmitInfo submitInfo{};
    submitInfo.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
    submitInfo.waitSemaphoreCount = 1;
    submitInfo.pWaitSemaphores = &imageAvailable;
    submitInfo.pWaitDstStageMask = &waitStage;
    submitInfo.commandBufferCount = 1;
    submitInfo.pCommandBuffers = &commandBuffer;
    submitInfo.signalSemaphoreCount = 1;
    submitInfo.pSignalSemaphores = &renderFinished;
    result = vkQueueSubmit(graphicsQueue, 1, &submitInfo, VK_NULL_HANDLE);
    if (result != VK_SUCCESS) return fail("ERRO ao enviar comandos à GPU: " + std::to_string(result));

    VkPresentInfoKHR presentInfo{};
    presentInfo.sType = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
    presentInfo.waitSemaphoreCount = 1;
    presentInfo.pWaitSemaphores = &renderFinished;
    presentInfo.swapchainCount = 1;
    presentInfo.pSwapchains = &swapchain;
    presentInfo.pImageIndices = &imageIndex;
    result = vkQueuePresentKHR(graphicsQueue, &presentInfo);
    if (result != VK_SUCCESS && result != VK_SUBOPTIMAL_KHR) {
        return fail("ERRO ao apresentar quadro na tela: " + std::to_string(result));
    }

    std::string output = "QUADRO VULKAN APRESENTADO COM SUCESSO!\n";
    output += "GPU: " + std::string(physicalProperties.deviceName);
    output += "\nResolução: " + std::to_string(extent.width) + " x " + std::to_string(extent.height);
    output += "\nImagens na swapchain: " + std::to_string(swapchainImageCount);
    output += "\nModo de apresentação: FIFO (VSync)";
    output += "\nCor de teste: roxo";
    cleanup();
    return env->NewStringUTF(output.c_str());
}
