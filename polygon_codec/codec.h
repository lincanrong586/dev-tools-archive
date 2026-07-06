#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#include "status.h"
#include "types.h"

namespace polygon_codec {

// Algorithm 表示可选的编码算法。
//
// 约定：
// - 编码输出的字节流会在包头中写入算法类型，解码时据此自动选择解码器
// - kButt 永远放在最后，用于判断算法个数/遍历边界（不代表真实算法）
enum class Algorithm : uint8_t {
  kBinFixed = 1,
  kBinOffsetVarint = 2,
  kDeltaVarint = 3,
  kDeltaWindowMinVarint = 4,
  kPbuf = 5,
  kBoostBin = 6,
  kButt = 7,
};

// Metrics 为“可选统计信息”。
//
// 注意：
// - enable_metrics 默认关闭，避免影响业务性能
// - 当 enabled=false 时，其余字段保持默认值
struct Metrics {
  bool enabled = false;
  double encode_ms = 0.0;
  double decode_ms = 0.0;

  // raw_bytes_estimated 是对 PolygonSet 原始内存占用的粗略估算（用于评估压缩比）。
  // 口径：vector 的 capacity 参与估算，接近“实际持有内存”而非“逻辑数据量”。
  size_t raw_bytes_estimated = 0;

  // encoded_bytes 是编码后的字节数（包含包头）。
  size_t encoded_bytes = 0;

  // bytes_per_polygon = encoded_bytes / polygons
  double bytes_per_polygon = 0.0;

  // compression_ratio = encoded_bytes / raw_bytes_estimated
  double compression_ratio = 0.0;
};

// Options 控制库行为。
struct Options {
  // enable_metrics 控制是否统计时延/压缩比；默认关闭。
  bool enable_metrics = false;

  // print_metrics 控制是否打印统计信息；只有 enable_metrics=true 时才生效；默认关闭。
  bool print_metrics = false;
};

struct EncodeOutput {
  Algorithm algorithm = Algorithm::kBinFixed;
  std::vector<uint8_t> bytes;
  Metrics metrics;
};

struct DecodeOutput {
  Algorithm algorithm = Algorithm::kBinFixed;
  PolygonSet polygon_set;
  Metrics metrics;
};

// Codec 为对外统一入口：
// - Encode：由调用方指定算法，输出“自描述字节流”（含包头）；
// - Decode：从包头读取算法类型，自动路由到对应解码器。
class Codec {
 public:
  static StatusOr<EncodeOutput> Encode(const PolygonSet& ps, Algorithm algo, const Options& opt = {});
  static StatusOr<DecodeOutput> Decode(const std::vector<uint8_t>& bytes, const Options& opt = {});
};

}  // namespace polygon_codec
