LOCAL_PATH:= $(call my-dir)

include $(CLEAR_VARS)

LOCAL_SRC_FILES:=       \
    wallClock.c myAlloc.c shmAlloc.c timeList.c ioStream.c hashTable.c epCore.c sh_print.c cJSON.c

LOCAL_C_INCLUDES:= \
    $(LOCAL_PATH)/include 

LOCAL_PRELINK_MODULE := false
LOCAL_MODULE_TAGS := debug

LOCAL_MODULE:= libckits

include $(BUILD_STATIC_LIBRARY)

