cmake_minimum_required(VERSION 3.16)

project(polygon_codec LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

file(GLOB POLYGON_CODEC_ALGO_SRCS CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/algorithms/*.cc")
list(REMOVE_ITEM POLYGON_CODEC_ALGO_SRCS "${CMAKE_CURRENT_SOURCE_DIR}/algorithms/boost_bin.cc")

add_library(polygon_codec_lib
  codec.cc
  codec_frame.cc
  codec_factory.cc
  codec_metrics.cc
  varint.cc
  ${POLYGON_CODEC_ALGO_SRCS}
)

target_include_directories(polygon_codec_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

find_package(Boost QUIET COMPONENTS serialization)
if(Boost_FOUND)
  target_compile_definitions(polygon_codec_lib PUBLIC POLYGON_CODEC_WITH_BOOST=1)
  target_include_directories(polygon_codec_lib PUBLIC ${Boost_INCLUDE_DIRS})
  target_link_libraries(polygon_codec_lib PUBLIC ${Boost_LIBRARIES})
  target_sources(polygon_codec_lib PRIVATE algorithms/boost_bin.cc)
endif()

add_executable(polygon_codec_tests
  tests/test_codec.cc
)

target_link_libraries(polygon_codec_tests PRIVATE polygon_codec_lib)

add_executable(polygon_codec_bench
  bench-perftest/main.cc
  bench-perftest/generate.cc
  bench-perftest/report.cc
)

target_link_libraries(polygon_codec_bench PRIVATE polygon_codec_lib)
