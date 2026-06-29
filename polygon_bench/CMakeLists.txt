cmake_minimum_required(VERSION 3.16)

project(polygon_bench LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_library(polygon_bench_lib
  src/bench.cpp
  src/encoders/bin_fixed.cpp
  src/encoders/bin_offset_varint.cpp
  src/encoders/delta_varint.cpp
  src/encoders/json_arr.cpp
  src/encoders/json_obj.cpp
  src/encoders/mini_pbuf.cpp
  src/generate.cpp
  src/polygon_types.cpp
  src/rss.cpp
  src/varint.cpp
)

target_include_directories(polygon_bench_lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/src)

add_executable(polygon_bench src/main.cpp)
target_link_libraries(polygon_bench PRIVATE polygon_bench_lib)

add_executable(polygon_bench_tests tests/test_roundtrip.cpp)
target_link_libraries(polygon_bench_tests PRIVATE polygon_bench_lib)
