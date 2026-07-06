CXX ?= g++
CXXFLAGS ?= -O2 -std=c++17 -Wall -Wextra -pedantic

TEST_DIR := tests
BUILD_DIR := build_make

COMMON_SRCS := \
	codec.cc \
	codec_frame.cc \
	codec_factory.cc \
	codec_metrics.cc \
	varint.cc

ALGO_SRCS := $(filter-out algorithms/boost_bin.cc,$(wildcard algorithms/*.cc))

LIB_SRCS := $(COMMON_SRCS) $(ALGO_SRCS)

TEST_SRCS := $(TEST_DIR)/test_codec.cc

LIB_OBJS := $(patsubst %.cc,$(BUILD_DIR)/%.o,$(LIB_SRCS))
TEST_OBJS := $(patsubst %.cc,$(BUILD_DIR)/%.o,$(TEST_SRCS))

INCLUDES := -I.

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
  CXXFLAGS += -DPOLYGON_CODEC_WITH_BOOST=1
  ifneq ($(BOOST_INC),)
    CXXFLAGS += -I$(BOOST_INC)
  endif
  ifneq ($(BOOST_LIB),)
    LDFLAGS += -L$(BOOST_LIB)
  endif
  LDLIBS += -lboost_serialization
  LIB_SRCS += algorithms/boost_bin.cc
  LIB_OBJS := $(patsubst %.cc,$(BUILD_DIR)/%.o,$(LIB_SRCS))
endif

.PHONY: all clean test

all: $(BUILD_DIR)/polygon_codec_tests

test: $(BUILD_DIR)/polygon_codec_tests
	./$(BUILD_DIR)/polygon_codec_tests

bench: $(BUILD_DIR)/polygon_codec_bench

run-bench: $(BUILD_DIR)/polygon_codec_bench
	./$(BUILD_DIR)/polygon_codec_bench

$(BUILD_DIR)/polygon_codec_tests: $(LIB_OBJS) $(TEST_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

BENCH_SRCS := bench-perftest/main.cc bench-perftest/generate.cc bench-perftest/report.cc
BENCH_OBJS := $(patsubst %.cc,$(BUILD_DIR)/%.o,$(BENCH_SRCS))

$(BUILD_DIR)/polygon_codec_bench: $(LIB_OBJS) $(BENCH_OBJS)
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o $@ $^ $(LDLIBS)

$(BUILD_DIR)/%.o: %.cc
	@mkdir -p $(dir $@)
	$(CXX) $(CXXFLAGS) $(INCLUDES) -c $< -o $@

clean:
	rm -rf $(BUILD_DIR)
