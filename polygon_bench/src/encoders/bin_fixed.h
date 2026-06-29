#pragma once

#include "encoders/encoder.h"

namespace polybench {

// 自定义二进制编码（基线）：
// - polygons_count: uvarint
// - for each polygon:
//     - points_count: uvarint
//     - for each point: x(int32 LE) + y(int32 LE)
class BinFixedEncoder final : public Encoder {
 public:
  std::string Name() const override;
  EncodeResult Encode(const PolygonSet& polygon_set) const override;
  DecodeResult Decode(const std::vector<uint8_t>& bytes) const override;
};

}  // namespace polybench
