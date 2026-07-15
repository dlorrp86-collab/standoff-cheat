LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)
LOCAL_MODULE    := serap
LOCAL_SRC_FILES := main.cpp renderer.cpp
LOCAL_CPPFLAGS  := -std=c++17 -O2 -fPIC -D__ANDROID_API__=24
LOCAL_LDLIBS    := -llog -lz -landroid -lEGL -lGLESv3 -lOpenSLES
LOCAL_C_INCLUDES := $(LOCAL_PATH)

include $(BUILD_SHARED_LIBRARY)