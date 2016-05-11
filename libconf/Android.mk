LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=       \
    IMultiParty.cpp \
    CompoundMix.cpp \
    AudioConfMix.cpp \
    VideoConfMix.cpp \
    mix_assembly.s \
    WaitingRoom.cpp \
    MOHPlayer.cpp

LOCAL_STATIC_LIBRARIES += \
    libckits

LOCAL_C_INCLUDES:= \
    bionic external/stlport/stlport \
    $(LOCAL_PATH)/include  \
    $(LOCAL_PATH)/../include \
    $(LOCAL_PATH)/../libvideo/ \
    $(LOCAL_PATH)/../libnet/include \
    $(LOCAL_PATH)/../libaux/include \
    $(LOCAL_PATH)/../libckits/include \
    frameworks/base/include/ \
    frameworks/native/include/ \
    frameworks/native/include/media/openmax \
    frameworks/av/media/libstagefright \
    frameworks/av/media/libstagefright/include \
	device/softwinner/common/hardware/include 

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := debug

LOCAL_MODULE:= libavsconf

include $(BUILD_STATIC_LIBRARY)

