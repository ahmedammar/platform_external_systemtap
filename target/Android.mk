
LOCAL_PATH := $(call my-dir)
include $(CLEAR_VARS)

LOCAL_MODULE := stapio
LOCAL_C_INCLUDES += $(LOCAL_PATH)/includes

LOCAL_CFLAGS := -fno-strict-aliasing	\
		-fno-builtin-strftime -DBINDIR='"/system/bin"'

LOCAL_SRC_FILES := \
	runtime/staprun/stapio.c	\
	runtime/staprun/mainloop.c	\
	runtime/staprun/common.c	\
	runtime/staprun/ctl.c	\
	runtime/staprun/relay.c

LOCAL_SHARED_LIBRARIES := liblog
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)

include $(CLEAR_VARS)

LOCAL_MODULE := staprun
LOCAL_C_INCLUDES += $(LOCAL_PATH)/includes

LOCAL_CFLAGS := -fno-strict-aliasing	\
		-fno-builtin-strftime -D_FILE_OFFSET_BITS=64 \
		-DSINGLE_THREADED \
		-DPKGDATADIR='"/system/bin"'	\
		-DPKGLIBDIR='"/system/bin"'

LOCAL_SRC_FILES := \
	runtime/staprun/staprun.c	\
	runtime/staprun/ctl.c	\
	runtime/staprun/staprun_funcs.c	\
	runtime/staprun/common.c	\
	runtime/staprun/glob.c

LOCAL_SHARED_LIBRARIES := liblog
LOCAL_MODULE_TAGS := optional
include $(BUILD_EXECUTABLE)
