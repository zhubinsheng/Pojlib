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
#include <vector>
#include <GLES3/gl32.h>
#include <vulkan/vulkan.h>
#include <vulkan/vulkan_android.h>

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
        printf("Result for %s was %d!\n", func, res);
    }
}

void find_queue_families() {
    uint32_t queueFamilyCount = 0;
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &queueFamilyCount, nullptr);

    std::vector<VkQueueFamilyProperties> properties(queueFamilyCount);
    vkGetPhysicalDeviceQueueFamilyProperties(pdev, &queueFamilyCount, properties.data());

    int i = 0;
    for (const auto& queueFamily : properties) {
        if (queueFamily.queueFlags & VK_QUEUE_GRAPHICS_BIT) {
            graphicsFamily = i;
        }

        i++;
    }
    printf("Failed to find queue family!\n");
}

VKAPI_ATTR VkBool32 VKAPI_CALL DebugUtilsMessenger(
        VkDebugUtilsMessageSeverityFlagBitsEXT messageSeverity,
        VkDebugUtilsMessageTypeFlagsEXT messageTypes,
        const VkDebugUtilsMessengerCallbackDataEXT *callbackData,
        void *userData) {
    const char validation[]  = "Validation";
    const char performance[] = "Performance";
    const char error[]       = "ERROR";
    const char warning[]     = "WARNING";
    const char unknownType[] = "UNKNOWN_TYPE";
    const char unknownSeverity[] = "UNKNOWN_SEVERITY";
    const char* typeString      = unknownType;
    const char* severityString  = unknownSeverity;
    const char* messageIdName   = callbackData->pMessageIdName;
    int32_t messageIdNumber     = callbackData->messageIdNumber;
    const char* message         = callbackData->pMessage;
    android_LogPriority priority = ANDROID_LOG_UNKNOWN;

    if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT) {
        severityString = error;
        priority = ANDROID_LOG_ERROR;
    }
    else if (messageSeverity & VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT) {
        severityString = warning;
        priority = ANDROID_LOG_WARN;
    }
    if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT) {
        typeString = validation;
    }
    else if (messageTypes & VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT) {
        typeString = performance;
    }

    printf("%s %s: [%s] Code %i : %s", typeString, severityString, messageIdName, messageIdNumber, message);

    return VK_FALSE;
}

extern "C"
JNIEXPORT void JNICALL
Java_org_vivecraft_utils_VLoader_initVulkan(JNIEnv* env, jclass clazz) {
    VkApplicationInfo appInfo;
    appInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
    appInfo.pNext = nullptr;
    appInfo.pApplicationName = "QuestCraft";
    appInfo.applicationVersion = VK_MAKE_VERSION(4, 0, 0);
    appInfo.pEngineName = "None";
    appInfo.engineVersion = VK_MAKE_VERSION(0, 0, 1);
    appInfo.apiVersion = VK_VERSION_1_1;

    std::vector<const char*> instExtensions = {
            VK_KHR_GET_PHYSICAL_DEVICE_PROPERTIES_2_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_CAPABILITIES_EXTENSION_NAME,
            VK_KHR_SURFACE_EXTENSION_NAME,
            VK_KHR_ANDROID_SURFACE_EXTENSION_NAME
    };
    VkInstanceCreateInfo info;
    info.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
    info.pNext = nullptr;
    info.flags = 0;
    info.pApplicationInfo = &appInfo;
    info.enabledLayerCount = 0;
    info.enabledExtensionCount = static_cast<uint32_t>(instExtensions.size());
    info.ppEnabledExtensionNames = instExtensions.data();

    check_vulkan(vkCreateInstance(&info, nullptr, &inst), "vkCreateInstance");

    uint32_t count;
    check_vulkan(vkEnumeratePhysicalDevices(inst, &count, nullptr), "vkEnumeratePhysicalDevices");
    std::vector<VkPhysicalDevice> devices(count);
    check_vulkan(vkEnumeratePhysicalDevices(inst, &count, devices.data()), "vkEnumeratePhysicalDevices");
    for(const auto& device : devices) {
        pdev = device;
        break;
    }

    find_queue_families();

    float priorities = 1.0F;
    std::vector<const char*> extensions = {
            VK_KHR_EXTERNAL_MEMORY_EXTENSION_NAME,
            VK_KHR_EXTERNAL_MEMORY_FD_EXTENSION_NAME,
            VK_KHR_GET_MEMORY_REQUIREMENTS_2_EXTENSION_NAME,
            VK_KHR_SWAPCHAIN_EXTENSION_NAME
    };
    VkDeviceQueueCreateInfo queueCreateInfo{};
    queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
    queueCreateInfo.flags = 0;
    queueCreateInfo.queueFamilyIndex = static_cast<uint32_t>(graphicsFamily);
    queueCreateInfo.queueCount = 1;
    queueCreateInfo.pQueuePriorities = &priorities;

    VkPhysicalDeviceFeatures features{};
    VkDeviceCreateInfo deviceCreateInfo;
    deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
    deviceCreateInfo.pNext = nullptr;
    deviceCreateInfo.flags = 0;
    deviceCreateInfo.queueCreateInfoCount = 1;
    deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
    deviceCreateInfo.enabledLayerCount = 0;
    deviceCreateInfo.enabledExtensionCount = extensions.size();
    deviceCreateInfo.ppEnabledExtensionNames = extensions.data();
    deviceCreateInfo.pEnabledFeatures = &features;
    check_vulkan(vkCreateDevice(pdev, &deviceCreateInfo, nullptr, &dev), "vkCreateDevice");

    auto vkGetDeviceQueue_p = reinterpret_cast<PFN_vkGetDeviceQueue>(vkGetDeviceProcAddr(
            dev, "vkGetDeviceQueue"));
    vkGetDeviceQueue_p(dev, graphicsFamily, 0, &graphicsQueue);
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
            .pQueueFamilyIndices = nullptr,
            .initialLayout = VK_IMAGE_LAYOUT_UNDEFINED
    };
    VkImage image;
    check_vulkan(vkCreateImage(dev, &imageInfo, nullptr, &image), "vkCreateImage");

    VkMemoryRequirements memRequirements;
    vkGetImageMemoryRequirements(dev, image, &memRequirements);

    VkExportMemoryAllocateInfo expAlloc{};
    expAlloc.sType = VK_STRUCTURE_TYPE_EXPORT_MEMORY_ALLOCATE_INFO;
    expAlloc.handleTypes = VK_EXTERNAL_MEMORY_HANDLE_TYPE_OPAQUE_FD_BIT;
    expAlloc.pNext = nullptr;

    VkMemoryAllocateInfo allocInfo{};
    allocInfo.sType = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
    allocInfo.pNext = &expAlloc;
    allocInfo.allocationSize = memRequirements.size;
    allocInfo.memoryTypeIndex = findMemoryType(memRequirements.memoryTypeBits, VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT);

    VkDeviceMemory deviceMemory;
    check_vulkan(vkAllocateMemory(dev, &allocInfo, nullptr, &deviceMemory), "vkAllocateMemory");

    check_vulkan(vkBindImageMemory(dev, image, deviceMemory, 0), "vkBindImageMemory");

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
