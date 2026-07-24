TARGET = topst-nn-server
BUILD_DIR := build
OBJDIR := $(BUILD_DIR)/obj
DEPDIR := $(BUILD_DIR)/dep

CROSS_COMPILE = aarch64-linux-gnu-
CC := $(CROSS_COMPILE)gcc
CXX := $(CROSS_COMPILE)gcc

OPENCV_ROOT ?= ./third_party/opencv
TOPST_SYSROOT ?=
VISION_PROTOCOL_ROOT ?= ./third_party/visionprotocol
JSON_C_ROOT ?= ./third_party/json-c
CUSTOM_POSTPROC ?= laneAF
CUSTOM_POSTPROC_SRC := custom_postproc/$(CUSTOM_POSTPROC)/custom_postproc.c


INCS := -I./inc -I./npu -I./common/message -I./custom_postproc/include -I$(OPENCV_ROOT)/include/opencv4 -I$(VISION_PROTOCOL_ROOT)/include -I$(JSON_C_ROOT)/include

CFLAGS := -Wall -std=c11 -mcpu=cortex-a53 -pthread -O2 -g -pg -MMD -MP
CXXFLAGS := -Wall -std=c++11 -mcpu=cortex-a53 -pthread -O2 -g -pg -MMD -MP
LDFLAGS :=
ifneq ($(strip $(TOPST_SYSROOT)),)
LDFLAGS += --sysroot=$(TOPST_SYSROOT)
endif

LIBS := -L$(OPENCV_ROOT)/lib -L$(VISION_PROTOCOL_ROOT)/lib -L$(JSON_C_ROOT)/lib -lpthread -ldl -lm -lstdc++
LIBS += -Wl,-rpath,'$$ORIGIN/third_party/visionprotocol/lib'
LIBS += -Wl,-rpath,'$$ORIGIN/third_party/json-c/lib'
LIBS += -Wl,-rpath,'$$ORIGIN/third_party/opencv/lib'
LIBS += -lopencv_core -lopencv_imgproc -lopencv_imgcodecs
LIBS += -lz -ltbb -ljpeg -lwebp -lpng16 -ltiff -llzma -lvisionprotocol -ljson-c

SRCS_C := \
	$(CUSTOM_POSTPROC_SRC) \
	npu/npu_api.c \
	npu/npu_api_ex.c \
	npu/npu_api_mem.c \
	common/message/message_api.c \
	src/app_config.c \
	src/app_control.c \
	src/app_inference.c \
	src/app_input.c \
	src/app_json.c \
	src/app_memory.c \
	src/app_monitor.c \
	src/app_pipeline.c \
	src/app_render.c \
	src/app_runtime.c \
	src/app_tracker.c \
	src/app_types.c \
	src/app_vision.c \
	src/main.c \
	src/platform_api.c

SRCS_CPP := src/opencv_api.cpp

OBJS := $(addprefix $(OBJDIR)/,$(SRCS_C:.c=.o) $(SRCS_CPP:.cpp=.o))
DEPS := $(addprefix $(DEPDIR)/,$(SRCS_C:.c=.d) $(SRCS_CPP:.cpp=.d))

all: check-custom-postproc $(TARGET)

.PHONY: check-custom-postproc
check-custom-postproc:
	@test -f $(CUSTOM_POSTPROC_SRC) || { echo "Unknown CUSTOM_POSTPROC=$(CUSTOM_POSTPROC)"; echo "Expected: $(CUSTOM_POSTPROC_SRC)"; exit 1; }


$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) $(LIBS) -o $@

$(OBJDIR)/%.o: %.c
	@mkdir -p $(dir $@) $(dir $(DEPDIR)/$*.d)
	$(CC) $(CFLAGS) $(INCS) -MF $(DEPDIR)/$*.d -c $< -o $@

$(OBJDIR)/%.o: %.cpp
	@mkdir -p $(dir $@) $(dir $(DEPDIR)/$*.d)
	$(CXX) -x c++ $(CXXFLAGS) $(INCS) -MF $(DEPDIR)/$*.d -c $< -o $@

.PHONY: clean
clean:
	rm -rf $(BUILD_DIR) $(TARGET)

-include $(DEPS)


