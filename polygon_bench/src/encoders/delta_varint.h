#pragma once

#include "encoders/encoder.h"

namespace polybench {

// 自定义二进制编码（polygon 内 delta + ZigZag varint）：
// - polygons_count: uvarint
// - for each polygon:
//     - points_count: uvarint
//     - point0: x0/y0 以绝对值 ZigZag varint 写入
//     - point1..: dx/dy 以增量 ZigZag varint 写入
//
// 该编码适合点序列具有局部连续性（相邻点坐标差相对较小）的情况。
class DeltaVarintEncoder final : public Encoder {
 public:
  std::string Name() const override;
  EncodeResult Encode(const PolygonSet& polygon_set) const override;
  DecodeResult Decode(const std::vector<uint8_t>& bytes) const override;
};

}  // namespace polybench
