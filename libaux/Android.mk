MY_LOCAL_PATH:= $(call my-dir)
LOCAL_PATH:= $(MY_LOCAL_PATH)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:=       \
    gentone.c \
    ToneGene.cpp \
    TokenPool.cpp \
    MetaBuffer.cpp

LOCAL_STATIC_LIBRARIES += \
    libckits
 
LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libdl \
    libstlport 

LOCAL_C_INCLUDES:= \
    bionic external/stlport/stlport \
    $(LOCAL_PATH)/include  \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../libckits/include 

LOCAL_CFLAGS += -DLOG_NDEBUG=0
LOCAL_CFLAGS += -fno-omit-frame-pointer \
    -Wno-multichar 

LOCAL_MODULE_TAGS := debug

LOCAL_MODULE:= libavs_aux

#include $(BUILD_STATIC_LIBRARY)
include $(BUILD_SHARED_LIBRARY)

