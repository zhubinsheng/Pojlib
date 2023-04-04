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
#include <GLES2/gl2ext.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <dlfcn.h>
#include <vulkan/vulkan.h>
#include <android/hardware_buffer.h>

static JavaVM* jvm;
XrInstanceCreateInfoAndroidKHR* OpenComposite_Android_Create_Info;
XrGraphicsBindingOpenGLESAndroidKHR* OpenComposite_Android_GLES_Binding_Info;

std::string (*OpenComposite_Android_Load_Input_File)(const char *path);

PFN_vkVoidFunction (*vkGetDeviceProcAddr_p) (
        VkDevice                                    device,
        const char*                                 pName);

VkResult (*vkGetMemoryAndroidHardwareBufferANDROID_p) (
        VkDevice                                    device,
        const VkMemoryGetAndroidHardwareBufferInfoANDROID* pInfo,
        struct AHardwareBuffer**                    pBuffer);

static std::string load_file(const char *path);

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

extern "C"
JNIEXPORT jint JNICALL
Java_org_vivecraft_utils_VLoader_createGLImage(JNIEnv* env, jclass clazz, jlong device, jlong memory) {
    GLint image;
    glGenTextures(1, reinterpret_cast<GLuint *>(&image));
    glBindTexture(GL_TEXTURE_2D, image);
    glTexParameterf(GL_TEXTURE_2D , GL_TEXTURE_MIN_FILTER, 9729.0F);
    glTexParameterf(GL_TEXTURE_2D , GL_TEXTURE_MAG_FILTER, 9729.0F);

    *(void **)(&vkGetDeviceProcAddr_p)  = dlsym(getenv("VULKAN_PTR"), "vkGetDeviceProcAddr");

    printf("Vulkan PTR: %p.\n", vkGetDeviceProcAddr_p);

    vkGetMemoryAndroidHardwareBufferANDROID_p = reinterpret_cast<VkResult (*)(VkDevice,
                                                                              const VkMemoryGetAndroidHardwareBufferInfoANDROID *,
                                                                              AHardwareBuffer **)>(vkGetDeviceProcAddr_p(
            reinterpret_cast<VkDevice>(device), "vkGetMemoryAndroidHardwareBufferANDROID"));

    AHardwareBuffer* hardwareBuffer;
    VkMemoryGetAndroidHardwareBufferInfoANDROID info;
    info.sType = VK_STRUCTURE_TYPE_MEMORY_GET_ANDROID_HARDWARE_BUFFER_INFO_ANDROID;
    info.memory = reinterpret_cast<VkDeviceMemory>(memory);

    vkGetMemoryAndroidHardwareBufferANDROID_p(reinterpret_cast<VkDevice>(device), &info, &hardwareBuffer);
    EGLClientBuffer buffer = eglGetNativeClientBufferANDROID(hardwareBuffer);
    EGLImage eglImage = eglCreateImage(eglGetCurrentDisplay(), nullptr, EGL_NATIVE_BUFFER_ANDROID, buffer,
                   nullptr);
    glEGLImageTargetTexture2DOES(GL_TEXTURE_2D, eglImage);

    return image;
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
