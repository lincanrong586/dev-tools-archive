#pragma once

#include <cstddef>

#include "codec.h"
#include "types.h"

namespace polygon_codec {

// 估算 PolygonSet 的内存占用（bytes）。
// 该值用于计算 compression_ratio，是一种“可比较”的近似口径。
size_t EstimateRawBytes(const PolygonSet& ps);

// 打印一行统计信息（仅当业务开启 print_metrics 时调用）。
void PrintMetricsLine(const char* stage, Algorithm algo, const Metrics& m);

}  // namespace polygon_codec

