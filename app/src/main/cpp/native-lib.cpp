#include <jni.h>
#include <string>
#include <vector>
#include <algorithm>
#include <sstream>
#include <cstring>
#include <cstdint>
#include <array>
#include <cstddef>
#include <cmath>
#include <atomic>
#include <android/native_window_jni.h>
#include <android/native_window.h>
#include <android/asset_manager_jni.h>
#include <android/asset_manager.h>
#define VK_USE_PLATFORM_ANDROID_KHR
#include <vulkan/vulkan.h>

static std::atomic<bool> g_stopRendering{false};

struct MeshVertex {
    float position[3];
    float uv[2];
    float tint[3];
    float normal[3];
};

// 24 vértices: quatro independentes por face para preservar normais planas e UVs.
static const MeshVertex cubeVertices[24] = {
    // Frente (+Z)
    {{-0.7f,-0.7f, 0.7f},{0,0},{1,0.45f,0.45f},{0,0,1}},
    {{ 0.7f,-0.7f, 0.7f},{1,0},{1,0.45f,0.45f},{0,0,1}},
    {{ 0.7f, 0.7f, 0.7f},{1,1},{1,0.45f,0.45f},{0,0,1}},
    {{-0.7f, 0.7f, 0.7f},{0,1},{1,0.45f,0.45f},{0,0,1}},
    // Trás (-Z)
    {{ 0.7f,-0.7f,-0.7f},{0,0},{0.45f,1,0.45f},{0,0,-1}},
    {{-0.7f,-0.7f,-0.7f},{1,0},{0.45f,1,0.45f},{0,0,-1}},
    {{-0.7f, 0.7f,-0.7f},{1,1},{0.45f,1,0.45f},{0,0,-1}},
    {{ 0.7f, 0.7f,-0.7f},{0,1},{0.45f,1,0.45f},{0,0,-1}},
    // Direita (+X)
    {{0.7f,-0.7f, 0.7f},{0,0},{0.45f,0.55f,1},{1,0,0}},
    {{0.7f,-0.7f,-0.7f},{1,0},{0.45f,0.55f,1},{1,0,0}},
    {{0.7f, 0.7f,-0.7f},{1,1},{0.45f,0.55f,1},{1,0,0}},
    {{0.7f, 0.7f, 0.7f},{0,1},{0.45f,0.55f,1},{1,0,0}},
    // Esquerda (-X)
    {{-0.7f,-0.7f,-0.7f},{0,0},{1,0.85f,0.4f},{-1,0,0}},
    {{-0.7f,-0.7f, 0.7f},{1,0},{1,0.85f,0.4f},{-1,0,0}},
    {{-0.7f, 0.7f, 0.7f},{1,1},{1,0.85f,0.4f},{-1,0,0}},
    {{-0.7f, 0.7f,-0.7f},{0,1},{1,0.85f,0.4f},{-1,0,0}},
    // Topo (+Y)
    {{-0.7f,0.7f, 0.7f},{0,0},{1,0.45f,1},{0,1,0}},
    {{ 0.7f,0.7f, 0.7f},{1,0},{1,0.45f,1},{0,1,0}},
    {{ 0.7f,0.7f,-0.7f},{1,1},{1,0.45f,1},{0,1,0}},
    {{-0.7f,0.7f,-0.7f},{0,1},{1,0.45f,1},{0,1,0}},
    // Base (-Y)
    {{-0.7f,-0.7f,-0.7f},{0,0},{0.4f,1,1},{0,-1,0}},
    {{ 0.7f,-0.7f,-0.7f},{1,0},{0.4f,1,1},{0,-1,0}},
    {{ 0.7f,-0.7f, 0.7f},{1,1},{0.4f,1,1},{0,-1,0}},
    {{-0.7f,-0.7f, 0.7f},{0,1},{0.4f,1,1},{0,-1,0}}
};
static const uint16_t cubeIndices[36] = {
    0,1,2, 0,2,3, 4,5,6, 4,6,7,
    8,9,10, 8,10,11, 12,13,14, 12,14,15,
    16,17,18, 16,18,19, 20,21,22, 20,22,23
};

static void buildCameraMatrix(float aspect, float angle, float out[32]) {
    const float nearPlane = 0.1f;
    const float farPlane = 10.0f;
    const float f = 1.0f;
    float projection[16]{};
    projection[0] = f / aspect;
    projection[5] = -f;
    projection[10] = farPlane / (nearPlane - farPlane);
    projection[11] = -1.0f;
    projection[14] = (farPlane * nearPlane) / (nearPlane - farPlane);

    float view[16]{};
    view[0] = view[5] = view[10] = view[15] = 1.0f;
    view[14] = -3.0f;

    float viewProjection[16]{};
    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            for (int k = 0; k < 4; ++k)
                viewProjection[col * 4 + row] += projection[k * 4 + row] * view[col * 4 + k];
        }
    }

    // Rotação combinada nos eixos X, Y e Z.
    const float cx = std::cos(angle * 0.73f), sx = std::sin(angle * 0.73f);
    const float cy = std::cos(angle), sy = std::sin(angle);
    const float cz = std::cos(angle * 0.41f), sz = std::sin(angle * 0.41f);
    const float rotationX[16] = {
        1,0,0,0,  0,cx,sx,0,  0,-sx,cx,0,  0,0,0,1
    };
    const float rotationY[16] = {
        cy,0,-sy,0,  0,1,0,0,  sy,0,cy,0,  0,0,0,1
    };
    const float rotationZ[16] = {
        cz,sz,0,0,  -sz,cz,0,0,  0,0,1,0,  0,0,0,1
    };
    float temp[16]{}, model[16]{};
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                temp[col * 4 + row] += rotationY[k * 4 + row] * rotationX[col * 4 + k];
    for (int col = 0; col < 4; ++col)
        for (int row = 0; row < 4; ++row)
            for (int k = 0; k < 4; ++k)
                model[col * 4 + row] += rotationZ[k * 4 + row] * temp[col * 4 + k];

    for (int col = 0; col < 4; ++col) {
        for (int row = 0; row < 4; ++row) {
            for (int k = 0; k < 4; ++k)
                out[col * 4 + row] += viewProjection[k * 4 + row] * model[col * 4 + k];
            out[16 + col * 4 + row] = model[col * 4 + row];
        }
    }
}

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

// Leitura comum e limitada de arquivos empacotados no APK.
// O loop evita depender de uma única chamada de leitura completa do AAsset.
static bool readAssetBytes(AAssetManager* manager, const char* name,
                           std::vector<uint8_t>& output, size_t maxBytes) {
    output.clear();
    if (!manager || !name || maxBytes == 0) return false;

    AAsset* asset = AAssetManager_open(manager, name, AASSET_MODE_STREAMING);
    if (!asset) return false;

    const off_t length = AAsset_getLength(asset);
    if (length <= 0 || static_cast<uint64_t>(length) > maxBytes) {
        AAsset_close(asset);
        return false;
    }

    output.resize(static_cast<size_t>(length));
    size_t offset = 0;
    while (offset < output.size()) {
        const size_t remaining = output.size() - offset;
        const int chunk = AAsset_read(asset, output.data() + offset, remaining);
        if (chunk <= 0) {
            output.clear();
            AAsset_close(asset);
            return false;
        }
        offset += static_cast<size_t>(chunk);
    }

    AAsset_close(asset);
    return true;
}

static bool loadShader(AAssetManager* manager, const char* name, std::vector<uint32_t>& code) {
    std::vector<uint8_t> bytes;
    if (!readAssetBytes(manager, name, bytes, 16u * 1024u * 1024u) ||
        (bytes.size() % sizeof(uint32_t)) != 0) return false;

    code.resize(bytes.size() / sizeof(uint32_t));
    std::memcpy(code.data(), bytes.data(), bytes.size());
    return !code.empty() && code[0] == 0x07230203;
}


static VkFormat findDepthFormat(VkPhysicalDevice physical) {
    const VkFormat candidates[] = {
        VK_FORMAT_D32_SFLOAT,
        VK_FORMAT_D24_UNORM_S8_UINT,
        VK_FORMAT_D16_UNORM
    };
    for (VkFormat format : candidates) {
        VkFormatProperties props{};
        vkGetPhysicalDeviceFormatProperties(physical, format, &props);
        if (props.optimalTilingFeatures & VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT)
            return format;
    }
    return VK_FORMAT_UNDEFINED;
}

static uint32_t findMemoryType(VkPhysicalDevice physical, uint32_t bits, VkMemoryPropertyFlags flags) {
    VkPhysicalDeviceMemoryProperties props{};
    vkGetPhysicalDeviceMemoryProperties(physical, &props);
    for (uint32_t i = 0; i < props.memoryTypeCount; ++i)
        if ((bits & (1u << i)) && (props.memoryTypes[i].propertyFlags & flags) == flags) return i;
    return UINT32_MAX;
}

static bool createTestTexture(AAssetManager* assets, VkPhysicalDevice physical, VkDevice device,
    VkQueue queue, VkCommandPool pool, VkImage* image, VkDeviceMemory* imageMemory,
    VkImageView* view, VkSampler* sampler) {
    std::vector<uint8_t> textureBytes;
    if (!readAssetBytes(assets, "texture.ppm", textureBytes, 4u * 1024u * 1024u)) return false;
    const std::string source(textureBytes.begin(), textureBytes.end());

    std::istringstream input(source);
    std::string magic;
    uint32_t width = 0, height = 0, maximum = 0;
    input >> magic >> width >> height >> maximum;
    if (magic != "P3" || width == 0 || height == 0 || maximum != 255 || width > 512 || height > 512)
        return false;

    std::vector<uint8_t> pixels(static_cast<size_t>(width) * height * 4);
    for (size_t i = 0; i < static_cast<size_t>(width) * height; ++i) {
        int r = 0, g = 0, b = 0;
        if (!(input >> r >> g >> b)) return false;
        pixels[i * 4] = static_cast<uint8_t>(std::max(0, std::min(255, r)));
        pixels[i * 4 + 1] = static_cast<uint8_t>(std::max(0, std::min(255, g)));
        pixels[i * 4 + 2] = static_cast<uint8_t>(std::max(0, std::min(255, b)));
        pixels[i * 4 + 3] = 255;
    }

    VkBuffer staging = VK_NULL_HANDLE;
    VkDeviceMemory stagingMemory = VK_NULL_HANDLE;
    VkBufferCreateInfo bufferInfo{};
    bufferInfo.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
    bufferInfo.size = pixels.size();
    bufferInfo.usage = VK_BUFFER_USAGE_TRANSFER_SRC_BIT;
    bufferInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    if (vkCreateBuffer(device, &bufferInfo, nullptr, &staging) != VK_SUCCESS) return false;

    VkMemoryRequirements bufferReq{};
    vkGetBufferMemoryRequirements(device, staging, &bufferReq);
    const uint32_t hostType = findMemoryType(physical, bufferReq.memoryTypeBits,
        VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
    if (hostType == UINT32_MAX) {
        vkDestroyBuffer(device, staging, nullptr);
        return false;
    }
    VkMemoryAllocateInfo allocation{};
    allocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocation.allocationSize = bufferReq.size;
    allocation.memoryTypeIndex = hostType;
    if (vkAllocateMemory(device, &allocation, nullptr, &stagingMemory) != VK_SUCCESS) {
        vkDestroyBuffer(device, staging, nullptr);
        return false;
    }
    if (vkBindBufferMemory(device, staging, stagingMemory, 0) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, staging, nullptr);
        return false;
    }
    void* mapped = nullptr;
    if (vkMapMemory(device, stagingMemory, 0, pixels.size(), 0, &mapped) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, staging, nullptr);
        return false;
    }
    std::memcpy(mapped, pixels.data(), pixels.size());
    vkUnmapMemory(device, stagingMemory);

    VkImageCreateInfo imageInfo{};
    imageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    imageInfo.imageType = VK_IMAGE_TYPE_2D;
    imageInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    imageInfo.extent = {width, height, 1};
    imageInfo.mipLevels = 1;
    imageInfo.arrayLayers = 1;
    imageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    imageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    imageInfo.usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT;
    imageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    imageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    if (vkCreateImage(device, &imageInfo, nullptr, image) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, staging, nullptr);
        return false;
    }

    VkMemoryRequirements imageReq{};
    vkGetImageMemoryRequirements(device, *image, &imageReq);
    const uint32_t deviceType = findMemoryType(physical, imageReq.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (deviceType == UINT32_MAX) {
        vkDestroyImage(device, *image, nullptr);
        *image = VK_NULL_HANDLE;
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, staging, nullptr);
        return false;
    }
    allocation.allocationSize = imageReq.size;
    allocation.memoryTypeIndex = deviceType;
    if (vkAllocateMemory(device, &allocation, nullptr, imageMemory) != VK_SUCCESS ||
        vkBindImageMemory(device, *image, *imageMemory, 0) != VK_SUCCESS) {
        if (*imageMemory) vkFreeMemory(device, *imageMemory, nullptr);
        *imageMemory = VK_NULL_HANDLE;
        vkDestroyImage(device, *image, nullptr);
        *image = VK_NULL_HANDLE;
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, staging, nullptr);
        return false;
    }

    VkCommandBufferAllocateInfo commandAllocation{};
    commandAllocation.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
    commandAllocation.commandPool = pool;
    commandAllocation.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
    commandAllocation.commandBufferCount = 1;
    VkCommandBuffer command = VK_NULL_HANDLE;
    if (vkAllocateCommandBuffers(device, &commandAllocation, &command) != VK_SUCCESS) {
        vkFreeMemory(device, stagingMemory, nullptr);
        vkDestroyBuffer(device, staging, nullptr);
        return false;
    }
    VkCommandBufferBeginInfo begin{};
    begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
    begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
    VkResult result = vkBeginCommandBuffer(command, &begin);
    if (result == VK_SUCCESS) {
        VkImageMemoryBarrier barrier{};
        barrier.sType = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
        barrier.oldLayout = VK_IMAGE_LAYOUT_UNDEFINED;
        barrier.newLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.srcQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.dstQueueFamilyIndex = VK_QUEUE_FAMILY_IGNORED;
        barrier.image = *image;
        barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        barrier.subresourceRange.levelCount = 1;
        barrier.subresourceRange.layerCount = 1;
        barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT, VK_PIPELINE_STAGE_TRANSFER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);

        VkBufferImageCopy copy{};
        copy.imageSubresource.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
        copy.imageSubresource.layerCount = 1;
        copy.imageExtent = {width, height, 1};
        vkCmdCopyBufferToImage(command, staging, *image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &copy);

        barrier.oldLayout = VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL;
        barrier.newLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
        barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;
        vkCmdPipelineBarrier(command, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT,
            0, 0, nullptr, 0, nullptr, 1, &barrier);
        result = vkEndCommandBuffer(command);
    }
    if (result == VK_SUCCESS) {
        VkSubmitInfo submit{};
        submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
        submit.commandBufferCount = 1;
        submit.pCommandBuffers = &command;
        result = vkQueueSubmit(queue, 1, &submit, VK_NULL_HANDLE);
        if (result == VK_SUCCESS) result = vkQueueWaitIdle(queue);
    }
    vkFreeCommandBuffers(device, pool, 1, &command);
    vkFreeMemory(device, stagingMemory, nullptr);
    vkDestroyBuffer(device, staging, nullptr);
    if (result != VK_SUCCESS) return false;

    VkImageViewCreateInfo viewInfo{};
    viewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    viewInfo.image = *image;
    viewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    viewInfo.format = VK_FORMAT_R8G8B8A8_UNORM;
    viewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_COLOR_BIT;
    viewInfo.subresourceRange.levelCount = 1;
    viewInfo.subresourceRange.layerCount = 1;
    if (vkCreateImageView(device, &viewInfo, nullptr, view) != VK_SUCCESS) return false;

    VkSamplerCreateInfo samplerInfo{};
    samplerInfo.sType = VK_STRUCTURE_TYPE_SAMPLER_CREATE_INFO;
    samplerInfo.magFilter = VK_FILTER_NEAREST;
    samplerInfo.minFilter = VK_FILTER_NEAREST;
    samplerInfo.addressModeU = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeV = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.addressModeW = VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
    samplerInfo.maxLod = 0.0f;
    return vkCreateSampler(device, &samplerInfo, nullptr, sampler) == VK_SUCCESS;
}

extern "C" JNIEXPORT void JNICALL
Java_com_p5s_android_MainActivity_prepareVulkanRendering(JNIEnv*, jobject) {
    g_stopRendering.store(false);
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
    VkBuffer vertexBuffer = VK_NULL_HANDLE, indexBuffer = VK_NULL_HANDLE;
    VkDeviceMemory vertexMemory = VK_NULL_HANDLE, indexMemory = VK_NULL_HANDLE;
    VkImage depthImage = VK_NULL_HANDLE;
    VkDeviceMemory depthMemory = VK_NULL_HANDLE;
    VkImageView depthView = VK_NULL_HANDLE;
    VkFormat depthFormat = VK_FORMAT_UNDEFINED;
    VkPipelineLayout pipelineLayout = VK_NULL_HANDLE;
    VkDescriptorSetLayout descriptorLayout = VK_NULL_HANDLE;
    VkDescriptorPool descriptorPool = VK_NULL_HANDLE;
    VkDescriptorSet descriptorSet = VK_NULL_HANDLE;
    VkImage textureImage = VK_NULL_HANDLE;
    VkDeviceMemory textureMemory = VK_NULL_HANDLE;
    VkImageView textureView = VK_NULL_HANDLE;
    VkSampler textureSampler = VK_NULL_HANDLE;
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
            if (vertexBuffer) vkDestroyBuffer(device, vertexBuffer, nullptr);
            if (vertexMemory) vkFreeMemory(device, vertexMemory, nullptr);
            if (indexBuffer) vkDestroyBuffer(device, indexBuffer, nullptr);
            if (indexMemory) vkFreeMemory(device, indexMemory, nullptr);
            if (pipelineLayout) vkDestroyPipelineLayout(device, pipelineLayout, nullptr);
            if (depthView) vkDestroyImageView(device, depthView, nullptr);
            if (depthImage) vkDestroyImage(device, depthImage, nullptr);
            if (depthMemory) vkFreeMemory(device, depthMemory, nullptr);
            if (descriptorPool) vkDestroyDescriptorPool(device, descriptorPool, nullptr);
            if (textureSampler) vkDestroySampler(device, textureSampler, nullptr);
            if (textureView) vkDestroyImageView(device, textureView, nullptr);
            if (textureImage) vkDestroyImage(device, textureImage, nullptr);
            if (textureMemory) vkFreeMemory(device, textureMemory, nullptr);
            if (descriptorLayout) vkDestroyDescriptorSetLayout(device, descriptorLayout, nullptr);
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

    depthFormat = findDepthFormat(physical);
    if (depthFormat == VK_FORMAT_UNDEFINED)
        return fail("ERRO: GPU sem formato compatível com depth buffer.");

    VkImageCreateInfo depthImageInfo{};
    depthImageInfo.sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
    depthImageInfo.imageType = VK_IMAGE_TYPE_2D;
    depthImageInfo.format = depthFormat;
    depthImageInfo.extent = {extent.width, extent.height, 1};
    depthImageInfo.mipLevels = 1;
    depthImageInfo.arrayLayers = 1;
    depthImageInfo.samples = VK_SAMPLE_COUNT_1_BIT;
    depthImageInfo.tiling = VK_IMAGE_TILING_OPTIMAL;
    depthImageInfo.usage = VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT;
    depthImageInfo.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
    depthImageInfo.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    result = vkCreateImage(device, &depthImageInfo, nullptr, &depthImage);
    if (result != VK_SUCCESS) return fail("ERRO ao criar imagem de profundidade: " + std::to_string(result));

    VkMemoryRequirements depthRequirements{};
    vkGetImageMemoryRequirements(device, depthImage, &depthRequirements);
    const uint32_t depthMemoryType = findMemoryType(physical, depthRequirements.memoryTypeBits,
        VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);
    if (depthMemoryType == UINT32_MAX)
        return fail("ERRO: memória para depth buffer indisponível.");
    VkMemoryAllocateInfo depthAllocation{};
    depthAllocation.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    depthAllocation.allocationSize = depthRequirements.size;
    depthAllocation.memoryTypeIndex = depthMemoryType;
    result = vkAllocateMemory(device, &depthAllocation, nullptr, &depthMemory);
    if (result != VK_SUCCESS) return fail("ERRO ao alocar memória de profundidade: " + std::to_string(result));
    result = vkBindImageMemory(device, depthImage, depthMemory, 0);
    if (result != VK_SUCCESS) return fail("ERRO ao vincular memória de profundidade: " + std::to_string(result));

    VkImageViewCreateInfo depthViewInfo{};
    depthViewInfo.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
    depthViewInfo.image = depthImage;
    depthViewInfo.viewType = VK_IMAGE_VIEW_TYPE_2D;
    depthViewInfo.format = depthFormat;
    depthViewInfo.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
    depthViewInfo.subresourceRange.levelCount = 1;
    depthViewInfo.subresourceRange.layerCount = 1;
    result = vkCreateImageView(device, &depthViewInfo, nullptr, &depthView);
    if (result != VK_SUCCESS) return fail("ERRO ao criar visualização do depth buffer: " + std::to_string(result));

    VkAttachmentDescription colorAttachment{};
    colorAttachment.format = format.format;
    colorAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    colorAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    colorAttachment.storeOp = VK_ATTACHMENT_STORE_OP_STORE;
    colorAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    colorAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    colorAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    colorAttachment.finalLayout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;
    VkAttachmentDescription depthAttachment{};
    depthAttachment.format = depthFormat;
    depthAttachment.samples = VK_SAMPLE_COUNT_1_BIT;
    depthAttachment.loadOp = VK_ATTACHMENT_LOAD_OP_CLEAR;
    depthAttachment.storeOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.stencilLoadOp = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
    depthAttachment.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
    depthAttachment.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
    depthAttachment.finalLayout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

    VkAttachmentReference colorReference{};
    colorReference.attachment = 0;
    colorReference.layout = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
    VkAttachmentReference depthReference{};
    depthReference.attachment = 1;
    depthReference.layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
    VkSubpassDescription subpass{};
    subpass.pipelineBindPoint = VK_PIPELINE_BIND_POINT_GRAPHICS;
    subpass.colorAttachmentCount = 1;
    subpass.pColorAttachments = &colorReference;
    subpass.pDepthStencilAttachment = &depthReference;
    VkSubpassDependency dependency{};
    dependency.srcSubpass = VK_SUBPASS_EXTERNAL;
    dependency.dstSubpass = 0;
    dependency.srcStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                               VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstStageMask = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT |
                               VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT |
                               VK_PIPELINE_STAGE_LATE_FRAGMENT_TESTS_BIT;
    dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT |
                               VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;
    VkRenderPassCreateInfo renderInfo{};
    renderInfo.sType = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
    VkAttachmentDescription attachments[] = {colorAttachment, depthAttachment};
    renderInfo.attachmentCount = 2;
    renderInfo.pAttachments = attachments;
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
        VkImageView attachments[] = {imageViews[i], depthView};
        fbInfo.attachmentCount = 2;
        fbInfo.pAttachments = attachments;
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

    VkVertexInputBindingDescription meshBinding{};
    meshBinding.binding = 0;
    meshBinding.stride = sizeof(MeshVertex);
    meshBinding.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;
    VkVertexInputAttributeDescription meshAttributes[4]{};
    meshAttributes[0] = {0, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, position)};
    meshAttributes[1] = {1, 0, VK_FORMAT_R32G32_SFLOAT, offsetof(MeshVertex, uv)};
    meshAttributes[2] = {2, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, tint)};
    meshAttributes[3] = {3, 0, VK_FORMAT_R32G32B32_SFLOAT, offsetof(MeshVertex, normal)};
    VkPipelineVertexInputStateCreateInfo vertexInput{};
    vertexInput.sType = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
    vertexInput.vertexBindingDescriptionCount = 1;
    vertexInput.pVertexBindingDescriptions = &meshBinding;
    vertexInput.vertexAttributeDescriptionCount = 4;
    vertexInput.pVertexAttributeDescriptions = meshAttributes;
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
    VkPipelineDepthStencilStateCreateInfo depthState{};
    depthState.sType = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
    depthState.depthTestEnable = VK_TRUE;
    depthState.depthWriteEnable = VK_TRUE;
    depthState.depthCompareOp = VK_COMPARE_OP_LESS;
    depthState.depthBoundsTestEnable = VK_FALSE;
    depthState.stencilTestEnable = VK_FALSE;
    VkDescriptorSetLayoutBinding textureBinding{};
    textureBinding.binding = 0;
    textureBinding.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    textureBinding.descriptorCount = 1;
    textureBinding.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo descriptorLayoutInfo{};
    descriptorLayoutInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    descriptorLayoutInfo.bindingCount = 1;
    descriptorLayoutInfo.pBindings = &textureBinding;
    result = vkCreateDescriptorSetLayout(device, &descriptorLayoutInfo, nullptr, &descriptorLayout);
    if (result != VK_SUCCESS) return fail("ERRO ao criar layout de textura: " + std::to_string(result));
    VkPushConstantRange cameraRange{};
    cameraRange.stageFlags = VK_SHADER_STAGE_VERTEX_BIT;
    cameraRange.offset = 0;
    cameraRange.size = sizeof(float) * 32;
    VkPipelineLayoutCreateInfo layoutInfo{};
    layoutInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    layoutInfo.setLayoutCount = 1;
    layoutInfo.pSetLayouts = &descriptorLayout;
    layoutInfo.pushConstantRangeCount = 1;
    layoutInfo.pPushConstantRanges = &cameraRange;
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
    pipelineInfo.pDepthStencilState = &depthState;
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

    auto uploadMeshBuffer = [&](const void* data, VkDeviceSize size, VkBufferUsageFlags usage,
                                VkBuffer* buffer, VkDeviceMemory* memory) -> bool {
        VkBufferCreateInfo info{};
        info.sType = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
        info.size = size;
        info.usage = usage;
        info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
        if (vkCreateBuffer(device, &info, nullptr, buffer) != VK_SUCCESS) return false;
        VkMemoryRequirements req{};
        vkGetBufferMemoryRequirements(device, *buffer, &req);
        uint32_t type = findMemoryType(physical, req.memoryTypeBits,
            VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT);
        if (type == UINT32_MAX) return false;
        VkMemoryAllocateInfo alloc{};
        alloc.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
        alloc.allocationSize = req.size;
        alloc.memoryTypeIndex = type;
        if (vkAllocateMemory(device, &alloc, nullptr, memory) != VK_SUCCESS) return false;
        if (vkBindBufferMemory(device, *buffer, *memory, 0) != VK_SUCCESS) return false;
        void* mapped = nullptr;
        if (vkMapMemory(device, *memory, 0, size, 0, &mapped) != VK_SUCCESS) return false;
        std::memcpy(mapped, data, static_cast<size_t>(size));
        vkUnmapMemory(device, *memory);
        return true;
    };
    if (!uploadMeshBuffer(cubeVertices, sizeof(cubeVertices), VK_BUFFER_USAGE_VERTEX_BUFFER_BIT,
                          &vertexBuffer, &vertexMemory))
        return fail("ERRO ao criar buffer de vértices da malha 3D.");
    if (!uploadMeshBuffer(cubeIndices, sizeof(cubeIndices), VK_BUFFER_USAGE_INDEX_BUFFER_BIT,
                          &indexBuffer, &indexMemory))
        return fail("ERRO ao criar buffer de índices da malha 3D.");

    if (!createTestTexture(assets, physical, device, queue, commandPool,
        &textureImage, &textureMemory, &textureView, &textureSampler))
        return fail("ERRO: não foi possível carregar texture.ppm na GPU.");
    VkDescriptorPoolSize descriptorSize{VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1};
    VkDescriptorPoolCreateInfo descriptorPoolInfo{};
    descriptorPoolInfo.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
    descriptorPoolInfo.maxSets = 1;
    descriptorPoolInfo.poolSizeCount = 1;
    descriptorPoolInfo.pPoolSizes = &descriptorSize;
    result = vkCreateDescriptorPool(device, &descriptorPoolInfo, nullptr, &descriptorPool);
    if (result != VK_SUCCESS) return fail("ERRO ao criar descriptor pool: " + std::to_string(result));
    VkDescriptorSetAllocateInfo descriptorAlloc{};
    descriptorAlloc.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_ALLOCATE_INFO;
    descriptorAlloc.descriptorPool = descriptorPool;
    descriptorAlloc.descriptorSetCount = 1;
    descriptorAlloc.pSetLayouts = &descriptorLayout;
    result = vkAllocateDescriptorSets(device, &descriptorAlloc, &descriptorSet);
    if (result != VK_SUCCESS) return fail("ERRO ao alocar descriptor set: " + std::to_string(result));
    VkDescriptorImageInfo descriptorImage{};
    descriptorImage.sampler = textureSampler;
    descriptorImage.imageView = textureView;
    descriptorImage.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    VkWriteDescriptorSet descriptorWrite{};
    descriptorWrite.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    descriptorWrite.dstSet = descriptorSet;
    descriptorWrite.dstBinding = 0;
    descriptorWrite.descriptorCount = 1;
    descriptorWrite.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    descriptorWrite.pImageInfo = &descriptorImage;
    vkUpdateDescriptorSets(device, 1, &descriptorWrite, 0, nullptr);

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

        VkClearValue clearValues[2]{};
        clearValues[0].color = {{ 0.055f, 0.025f, 0.10f, 1.0f }};
        clearValues[1].depthStencil = {1.0f, 0};
        VkRenderPassBeginInfo passBegin{};
        passBegin.sType = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
        passBegin.renderPass = renderPass;
        passBegin.framebuffer = framebuffers[imageIndex];
        passBegin.renderArea.extent = extent;
        passBegin.clearValueCount = 2;
        passBegin.pClearValues = clearValues;
        vkCmdBeginRenderPass(command, &passBegin, VK_SUBPASS_CONTENTS_INLINE);
        vkCmdBindPipeline(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline);
        vkCmdBindDescriptorSets(command, VK_PIPELINE_BIND_POINT_GRAPHICS, pipelineLayout, 0, 1, &descriptorSet, 0, nullptr);
        float cameraMatrix[32]{};
        const float rotationAngle = static_cast<float>(renderedFrames) * 0.012f;
        buildCameraMatrix(static_cast<float>(extent.width) / static_cast<float>(extent.height), rotationAngle, cameraMatrix);
        vkCmdPushConstants(command, pipelineLayout, VK_SHADER_STAGE_VERTEX_BIT, 0,
                           sizeof(cameraMatrix), cameraMatrix);
        VkDeviceSize vertexOffset = 0;
        vkCmdBindVertexBuffers(command, 0, 1, &vertexBuffer, &vertexOffset);
        vkCmdBindIndexBuffer(command, indexBuffer, 0, VK_INDEX_TYPE_UINT16);
        vkCmdDrawIndexed(command, 36, 1, 0, 0, 0);
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
    output += "\nGeometria: 2 triângulos em coordenadas 3D";
    output += "\nCâmera: matriz View + Projection (perspectiva)";
    output += "\nParâmetros: FOV aproximado 90°, near 0.1, far 10.0";
    output += "\nTeste visual: triângulo frontal deve ocultar parte do triângulo traseiro";
    output += "\nTextura: imagem PPM 16x16 carregada na GPU";
    output += "\nAmostragem: VkImage + VkSampler + Descriptor Set";
    output += "\nProfundidade: depth buffer ativo (teste LESS, escrita habilitada)";
    output += "\nFormato de profundidade: " + std::to_string(static_cast<int>(depthFormat));
    output += "\nFrames apresentados: " + std::to_string(renderedFrames);
    output += interrupted ? "\nMotivo: superfície redimensionada ou recriada." : "\nMotivo: ciclo interrompido pelo Android.";
    cleanup();
    return env->NewStringUTF(output.c_str());
}
