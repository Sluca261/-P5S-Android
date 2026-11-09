#include <jni.h>
#include <string>
#include <vector>
#include <android/native_window_jni.h>
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

extern "C" JNIEXPORT jstring JNICALL
Java_com_p5s_android_MainActivity_getVulkanInfo(JNIEnv *env, jobject) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "P5S Android Diagnostic";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 2, 0);
    appInfo.pEngineName = "Diagnostic";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 2, 0);
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
Java_com_p5s_android_MainActivity_checkVulkanSurface(JNIEnv *env, jobject, jobject javaSurface) {
    if (javaSurface == nullptr) return env->NewStringUTF("Superfície Android inválida.");

    if (!hasInstanceExtension(VK_KHR_SURFACE_EXTENSION_NAME) ||
        !hasInstanceExtension(VK_KHR_ANDROID_SURFACE_EXTENSION_NAME)) {
        return env->NewStringUTF("O driver não anuncia as extensões Vulkan necessárias para apresentar imagens na tela Android.");
    }

    const char* extensions[] = {
        VK_KHR_SURFACE_EXTENSION_NAME,
        VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };

    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "P5S Android Surface Test";
    appInfo.apiVersion = VK_API_VERSION_1_0;

    VkInstanceCreateInfo instanceInfo{};
    instanceInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    instanceInfo.pApplicationInfo = &appInfo;
    instanceInfo.enabledExtensionCount = 2;
    instanceInfo.ppEnabledExtensionNames = extensions;

    VkInstance instance = VK_NULL_HANDLE;
    VkResult result = vkCreateInstance(&instanceInfo, nullptr, &instance);
    if (result != VK_SUCCESS) {
        return env->NewStringUTF(("Falha ao criar instância Vulkan para a tela. VkResult: " + std::to_string(result)).c_str());
    }

    ANativeWindow* window = ANativeWindow_fromSurface(env, javaSurface);
    if (window == nullptr) {
        vkDestroyInstance(instance, nullptr);
        return env->NewStringUTF("Não foi possível obter a janela nativa Android.");
    }

    auto createSurface = reinterpret_cast<PFN_vkCreateAndroidSurfaceKHR>(
        vkGetInstanceProcAddr(instance, "vkCreateAndroidSurfaceKHR"));
    if (createSurface == nullptr) {
        ANativeWindow_release(window);
        vkDestroyInstance(instance, nullptr);
        return env->NewStringUTF("A função de criação da superfície Android não está disponível no driver.");
    }

    VkAndroidSurfaceCreateInfoKHR surfaceInfo{};
    surfaceInfo.sType = VK_STRUCTURE_TYPE_ANDROID_SURFACE_CREATE_INFO_KHR;
    surfaceInfo.window = window;

    VkSurfaceKHR surface = VK_NULL_HANDLE;
    result = createSurface(instance, &surfaceInfo, nullptr, &surface);
    ANativeWindow_release(window);
    if (result != VK_SUCCESS) {
        vkDestroyInstance(instance, nullptr);
        return env->NewStringUTF(("Falha ao criar superfície Vulkan. VkResult: " + std::to_string(result)).c_str());
    }

    uint32_t count = 0;
    vkEnumeratePhysicalDevices(instance, &count, nullptr);
    std::vector<VkPhysicalDevice> devices(count);
    if (count > 0) vkEnumeratePhysicalDevices(instance, &count, devices.data());

    std::string output = "Superfície Vulkan Android criada.\n";
    bool presentSupported = false;
    for (const auto& device : devices) {
        uint32_t familyCount = 0;
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, nullptr);
        std::vector<VkQueueFamilyProperties> families(familyCount);
        vkGetPhysicalDeviceQueueFamilyProperties(device, &familyCount, families.data());
        for (uint32_t i = 0; i < familyCount; ++i) {
            VkBool32 supported = VK_FALSE;
            VkResult supportResult = vkGetPhysicalDeviceSurfaceSupportKHR(device, i, surface, &supported);
            if (supportResult == VK_SUCCESS && supported) {
                VkPhysicalDeviceProperties properties{};
                vkGetPhysicalDeviceProperties(device, &properties);
                output += "Apresentação suportada por: ";
                output += properties.deviceName;
                output += "\nFila gráfica compatível: " + std::to_string(i);
                presentSupported = true;
                break;
            }
        }
        if (presentSupported) break;
    }

    if (!presentSupported) {
        output += "Nenhuma fila de dispositivo foi confirmada para apresentação nesta superfície.";
    } else {
        VkSurfaceCapabilitiesKHR capabilities{};
        VkResult capResult = vkGetPhysicalDeviceSurfaceCapabilitiesKHR(devices[0], surface, &capabilities);
        if (capResult == VK_SUCCESS) {
            output += "\nTamanho mínimo de imagens na cadeia de apresentação: ";
            output += std::to_string(capabilities.minImageCount);
        }
    }

    vkDestroySurfaceKHR(instance, surface, nullptr);
    vkDestroyInstance(instance, nullptr);
    return env->NewStringUTF(output.c_str());
}
