#include <jni.h>
#include <string>
#include <vector>
#include <vulkan/vulkan.h>

extern "C" JNIEXPORT jstring JNICALL
Java_com_p5s_android_MainActivity_getVulkanInfo(JNIEnv *env, jobject /* this */) {
    VkApplicationInfo appInfo{};
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pApplicationName = "P5S Android Diagnostic";
    appInfo.applicationVersion = VK_MAKE_VERSION(0, 1, 0);
    appInfo.pEngineName = "Diagnostic";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 1, 0);
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
        return env->NewStringUTF("Vulkan foi inicializado, mas nenhum dispositivo físico compatível foi encontrado.");
    }

    std::vector<VkPhysicalDevice> devices(deviceCount);
    VkResult devicesResult = vkEnumeratePhysicalDevices(instance, &deviceCount, devices.data());
    if (devicesResult != VK_SUCCESS) {
        vkDestroyInstance(instance, nullptr);
        return env->NewStringUTF("Falha ao consultar os dispositivos Vulkan.");
    }

    std::string output = "Vulkan inicializado com sucesso.\n\nDispositivos encontrados: " + std::to_string(deviceCount);
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
