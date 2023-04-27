//
// Created by Judge on 12/23/2021.
//
#include <thread>
#include <string>
#include <errno.h>
#include <fcntl.h>
#include <unistd.h>
#include <OpenOVR/openxr_platform.h>
#include <jni.h>
#include "log.h"
#include <GLES3/gl32.h>
#include <vulkan/vulkan.h>

static JavaVM* jvm;
XrInstanceCreateInfoAndroidKHR* OpenComposite_Android_Create_Info;
XrGraphicsBindingOpenGLESAndroidKHR* OpenComposite_Android_GLES_Binding_Info;

std::string (*OpenComposite_Android_Load_Input_File)(const char *path);

static std::string load_file(const char *path);
VkInstance inst;
VkPhysicalDevice pdev;
VkDevice dev;
int graphicsFamily;
VkQueue graphicsQueue;

jint JNI_OnLoad(JavaVM* vm, void* reserved) {
    if (jvm == nullptr) {
        jvm = vm;
    }
    return JNI_VERSION_1_4;
}

extern "C"
JNIEXPORT void JNICALL
Java_pojlib_util_VLoader_setAndroidInitInfo(JNIEnv *env, jclass clazz, jobject ctx) {
    OpenComposite_Android_Load_Input_File = load_file;

    env->GetJavaVM(&jvm);
    ctx = env->NewGlobalRef(ctx);
    OpenComposite_Android_Create_Info = new XrInstanceCreateInfoAndroidKHR{
            XR_TYPE_INSTANCE_CREATE_INFO_ANDROID_KHR,
            nullptr,
            jvm,
            ctx
    };

    PFN_xrInitializeLoaderKHR initializeLoader = nullptr;
    XrResult res;

    res = xrGetInstanceProcAddr(XR_NULL_HANDLE, "xrInitializeLoaderKHR",
                                (PFN_xrVoidFunction *) (&initializeLoader));

    if(!XR_SUCCEEDED(res)) {
        printf("Error!");
    }

    XrLoaderInitInfoAndroidKHR loaderInitInfoAndroidKhr = {
            XR_TYPE_LOADER_INIT_INFO_ANDROID_KHR,
            nullptr,
            jvm,
            ctx
    };

    initializeLoader((const XrLoaderInitInfoBaseHeaderKHR *) &loaderInitInfoAndroidKhr);
}

void check_vulkan(VkResult res, const char* func) {
    if(res != VK_SUCCESS) {
        printf("Result for %s was %d!", func, res);
    }
}

extern "C"
JNIEXPORT void JNICALL
Java_org_vivecraft_utils_VLoader_initVulkan(JNIEnv* env, jclass clazz) {
    VkApplicationInfo appInfo = {
            .sType = VK_STRUCTURE_TYPE_APPLICATION_INFO,
            .pNext = nullptr,
            .pApplicationName = "QuestCraft",
            .applicationVersion = VK_MAKE_VERSION(4, 0, 0),
            .pEngineName = "None",
            .engineVersion = VK_MAKE_VERSION(1, 0, 0),
            .apiVersion = VK_VERSION_1_1
    };

    const char* extensions = {
            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME
    };
    VkInstanceCreateInfo info = {
            .sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO,
            .pNext = nullptr,
            .pApplicationInfo = &appInfo,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = 1,
            .ppEnabledExtensionNames = &extensions
    };

    check_vulkan(vkCreateInstance(&info, nullptr, &inst), "vkCreateInstance");

    uint32_t count;
    check_vulkan(vkEnumeratePhysicalDevices(inst, &count, nullptr), "vkEnumeratePhysicalDevices");
    VkPhysicalDevice devices[count];
    check_vulkan(vkEnumeratePhysicalDevices(inst, &count, devices), "vkEnumeratePhysicalDevices");
    pdev = devices[0];

    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &queueFamilyCount, nullptr);

    VkQueueFamilyProperties properties[queueFamilyCount];
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &queueFamilyCount, properties);

    int i = 0;
    for (const auto& queueFamily : properties) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
        }

        i++;
    }

    extensions = {
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME
            VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME
    };
    VkDeviceCreateInfo deviceCreateInfo = {
            .sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .enabledLayerCount = 0,
            .ppEnabledLayerNames = nullptr,
            .enabledExtensionCount = 2,
            .ppEnabledExtensionNames = &extensions
    };
    check_vulkan(vkCreateDevice(pdev, &deviceCreateInfo, nullptr, &dev), "vkCreateDevice");

    vkGetDeviceQueue(dev, graphicsFamily, 0, &graphicsQueue);
}

extern "C"
JNIEXPORT void JNICALL
Java_pojlib_util_VLoader_setEGLGlobal(JNIEnv* env, jclass clazz, jlong ctx, jlong display, jlong cfg) {
    OpenComposite_Android_GLES_Binding_Info = new XrGraphicsBindingOpenGLESAndroidKHR {
            XR_TYPE_GRAPHICS_BINDING_OPENGL_ES_ANDROID_KHR,
            nullptr,
            (void*)display,
            (void*)cfg,
            (void*)ctx
    };
}

uint32_t findMemoryType(uint32_t typeFilter, VkMemoryPropertyFlags properties) {
    VkPhysicalDeviceMemoryProperties memProperties;
    vkGetPhysicalDeviceMemoryProperties(pdev, &memProperties);
    for (uint32_t i = 0; i < memProperties.memoryTypeCount; i++) {
        if (typeFilter & (1 << i)) {
            return i;
        }
    }
    printf("Failed to find suitable memory type!");
    return -1;
}

extern "C"
JNIEXPORT jlong JNICALL
Java_org_vivecraft_utils_VLoader_getInstance(JNIEnv* env, jclass clazz) {
    return reinterpret_cast<jlong>(inst);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_org_vivecraft_utils_VLoader_getPhysicalDevice(JNIEnv* env, jclass clazz) {
    return reinterpret_cast<jlong>(pdev);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_org_vivecraft_utils_VLoader_getDevice(JNIEnv* env, jclass clazz) {
    return reinterpret_cast<jlong>(dev);
}

extern "C"
JNIEXPORT jint JNICALL
Java_org_vivecraft_utils_VLoader_getGraphicsFamily(JNIEnv* env, jclass clazz) {
    return reinterpret_cast<jint>(graphicsFamily);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_org_vivecraft_utils_VLoader_getQueue(JNIEnv* env, jclass clazz) {
    return reinterpret_cast<jlong>(graphicsQueue);
}

extern "C"
JNIEXPORT jlong JNICALL
Java_org_vivecraft_utils_VLoader_createImage(JNIEnv* env, jclass clazz, jint width, jint height, jlong fd) {
    VkImageCreateInfo imageInfo = {
            .sType = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO,
            .pNext = nullptr,
            .flags = 0,
            .imageType = VK_IMAGE_TYPE_2D,
            .format = VK_FORMAT_R8G8B8A8_SRGB,
            .extent = {static_cast<uint32_t>(width), static_cast<uint32_t>(height), 1},
            .mipLevels = 1,
            .arrayLayers = 1,
            .samples = VK_SAMPLE_COUNT_1_BIT,
            .tiling = VK_IMAGE_TILING_OPTIMAL,
            .usage = VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
            .sharingMode = VK_SHARING_MODE_EXCLUSIVE,
            .queueFamilyIndexCount = 0,
            .pQueueFamilyIndices = nullptr
    };
    VkImage image;
    check_vulkan(vkCreateImage(dev, &imageInfo, nullptr, &image), "vkCreateImage");

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(dev, image, &memRequirements);

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory deviceMemory;
    check_vulkan(vkAllocateMemory(dev, &allocInfo, nullptr, &deviceMemory), "vkAllocateMemory");

    int* memory = reinterpret_cast<int *>(fd);
    VkMemoryGetFdInfoKHR fdInfo = {
            .sType = VK_STRUCTURE_TYPE_MEMORY_GET_FD_INFO_KHR,
            .pNext = nullptr,
            .memory = deviceMemory,
            .handleType = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT
    };
    auto vkGetMemoryFdKHR_p = reinterpret_cast<PFN_vkGetMemoryFdKHR>(vkGetDeviceProcAddr(
            dev, "vkGetMemoryFdKHR"));
    check_vulkan(vkGetMemoryFdKHR_p(dev, &fdInfo, memory), "vkGetMemoryFdKHR");

    return reinterpret_cast<jlong>(image);
}

static std::string load_file(const char *path) {
    // Just read the file from the filesystem, we changed the working directory earlier so
    // Vivecraft can extract it's manifest files.

    printf("Path: %s", path);
    int fd = open(path, O_RDONLY);
    if (!fd) {
        LOGE("Failed to load manifest file %s: %d %s", path, errno, strerror(errno));
    }

    int length = lseek(fd, 0, SEEK_END);
    lseek(fd, 0, SEEK_SET);

    std::string data;
    data.resize(length);
    if (!read(fd, (void *) data.data(), data.size())) {
        LOGE("Failed to load manifest file %s failed to read: %d %s", path, errno, strerror(errno));
    }

    if (close(fd)) {
        LOGE("Failed to load manifest file %s failed to close: %d %s", path, errno,
             strerror(errno));
    }

    return std::move(data);
}
