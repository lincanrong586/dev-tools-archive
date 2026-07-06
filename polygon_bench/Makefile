CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -pedantic

SRC_DIR := src
TEST_DIR := tests
BUILD_DIR := build_make

LIB_SRCS := \
	$(SRC_DIR)/bench.cpp \
	$(SRC_DIR)/encoders/boost_bin.cpp \
	$(SRC_DIR)/encoders/bin_fixed.cpp \
	$(SRC_DIR)/encoders/bin_offset_varint.cpp \
	$(SRC_DIR)/encoders/delta_varint.cpp \
	$(SRC_DIR)/encoders/delta_windowmin_varint.cpp \
	$(SRC_DIR)/encoders/mini_pbuf.cpp \
	$(SRC_DIR)/generate.cpp \
	$(SRC_DIR)/polygon_types.cpp \
	$(SRC_DIR)/rss.cpp \
	$(SRC_DIR)/varint.cpp

MAIN_SRCS := $(SRC_DIR)/main.cpp
TEST_SRCS := $(TEST_DIR)/test_roundtrip.cpp

LIB_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(LIB_SRCS))
MAIN_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(MAIN_SRCS))
TEST_OBJS := $(patsubst %.cpp,$(BUILD_DIR)/%.o,$(TEST_SRCS))

INCLUDES := -I$(SRC_DIR)

WITH_BOOST ?= auto
BOOST_INC ?=
BOOST_LIB ?=

ifeq ($(WITH_BOOST),auto)
  ifneq ("$(wildcard /usr/include/boost/archive/binary_oarchive.hpp)","")
    WITH_BOOST := 1
  else
    WITH_BOOST := 0
  endif
endif

ifeq ($(WITH_BOOST),1)
  CXXFLAGS += -DPOLYBENCH_WITH_BOOST=1
  ifneq ($(BOOST_INC),)
    CXXFLAGS += -I$(BOOST_INC)
  endif
  ifneq ($(BOOST_LIB),)
    LDFLAGS += -L$(BOOST_LIB)
  endif
  LDLIBS += -lboost_serialization
endif

.PHONY: all clean test bench

all: $(BUILD_DIR)/polygon_bench $(BUILD_DIR)/polygon_bench_tests

bench: $(BUILD_DIR)/polygon_bench

test: $(BUILD_DIR)/polygon_bench_tests
	./$(BUILD_DIR)/polygon_bench_tests

$(BUILD_DIR)/polygon_bench: $(LIB_OBJS) $(MAIN_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/polygon_bench_tests: $(LIB_OBJS) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: %.cpp
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
