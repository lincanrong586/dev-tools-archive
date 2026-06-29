#pragma once

#include "encoders/encoder.h"

namespace polybench {

// 自定义二进制编码（窗口偏移 + varint）：
// - min_x: sint32 ZigZag varint
// - min_y: sint32 ZigZag varint
// - polygons_count: uvarint
// - for each polygon:
//     - points_count: uvarint
//     - for each point:
//         - rx = x - min_x: uvarint
//         - ry = y - min_y: uvarint
//
// 该编码用于消除“绝对坐标越大，varint 越大”的位置相关性，适合本业务的“截取窗口”传输。
class BinOffsetVarintEncoder final : public Encoder {
 public:
  std::string Name() const override;
  EncodeResult Encode(const PolygonSet& polygon_set) const override;
  DecodeResult Decode(const std::vector<uint8_t>& bytes) const override;
};

}  // namespace polybench
