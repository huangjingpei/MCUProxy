MY_LOCAL_PATH:= $(call my-dir)
LOCAL_PATH:= $(MY_LOCAL_PATH)

include $(CLEAR_VARS)
LOCAL_SRC_FILES:=       \
    Transport.cpp \
    RTPTransport.cpp \
    absTimerList.cpp \
    Poller.cpp \
    Sender.cpp \
    Holder.cpp

LOCAL_STATIC_LIBRARIES += \
    libckits
 
LOCAL_SHARED_LIBRARIES := \
    libutils \
    libcutils \
    libdl \
    libstlport \
    libavs_aux

LOCAL_C_INCLUDES:= \
    bionic external/stlport/stlport \
    $(LOCAL_PATH)/include  \
	$(LOCAL_PATH)/../include \
	$(LOCAL_PATH)/../libckits/include 

LOCAL_CFLAGS += -DLOG_NDEBUG=0
LOCAL_CFLAGS += -fno-omit-frame-pointer \
    -Wno-multichar

LOCAL_MODULE_TAGS := debug
LOCAL_MODULE:= libavs_net

include $(BUILD_SHARED_LIBRARY)
