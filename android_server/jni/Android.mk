LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := mem_server.sh

# ── Binario Unificado: Servidor Memoria + Overlay Vulkan/ImGui ──
LOCAL_CFLAGS   := -O2 -fPIE -fvisibility=hidden -w
LOCAL_CFLAGS   += -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_CPPFLAGS := -std=c++17 -fPIE -fvisibility=hidden -w -Wno-error=c++11-narrowing
LOCAL_CPPFLAGS += -DVK_USE_PLATFORM_ANDROID_KHR
LOCAL_CPPFLAGS += -DUNIFIED_BUILD

LOCAL_C_INCLUDES := \
    $(LOCAL_PATH) \
    $(LOCAL_PATH)/src \
    $(LOCAL_PATH)/src/Main \
    $(LOCAL_PATH)/src/res \
    $(LOCAL_PATH)/include \
    $(LOCAL_PATH)/include/ImGui \
    $(LOCAL_PATH)/include/ImGui/backends \
    $(LOCAL_PATH)/include/Android_vulkan \
    $(LOCAL_PATH)/include/Android_draw \
    $(LOCAL_PATH)/include/Android_touch \
    $(LOCAL_PATH)/include/My_Utils \
    $(LOCAL_PATH)/include/native_surface

# ── Fuentes del servidor de memoria ──────────────────────────
LOCAL_SRC_FILES := \
    main.cpp \
    tcp_server.cpp \
    mem_reader.cpp \
    elf_fixer.cpp \
    ue4_auto_scanner.cpp \
    compressor.cpp \
    file_manager.cpp \
    auto_updater.cpp

# ── Fuentes del motor gráfico ESP (Vulkan + ImGui) ───────────
LOCAL_SRC_FILES += \
    src/Android_draw/draw.cpp \
    src/Android_touch/TouchHelperA.cpp \
    src/ImGui/imgui.cpp \
    src/ImGui/imgui_demo.cpp \
    src/ImGui/imgui_draw.cpp \
    src/ImGui/imgui_switch.cpp \
    src/ImGui/imgui_tables.cpp \
    src/ImGui/imgui_widgets.cpp \
    src/ImGui/backends/imgui_impl_android.cpp \
    src/ImGui/backends/imgui_impl_vulkan.cpp \
    src/My_Utils/stb_image.cpp \
    src/Android_vulkan/vulkan_wrapper.cpp \
    src/Android_vulkan/VulkanUtils.cpp \
    src/My_Utils/imgui_image.cpp \
    src/oxorany.cpp \
    src/main.cpp \
    src/res/cJSON.c

LOCAL_LDFLAGS := -pie
LOCAL_LDLIBS  := -llog -landroid -lEGL -lGLESv3

include $(BUILD_EXECUTABLE)
