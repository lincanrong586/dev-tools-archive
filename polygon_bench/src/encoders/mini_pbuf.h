#pragma once

#include "encoders/encoder.h"

namespace polybench {

// "pbuf" 基准编码：protobuf wire-format 的最小兼容实现（无需依赖 libprotobuf）。
//
// 对应的参考 schema 见 schemas/polygon.proto：
// PolygonSet { repeated Polygon polygons = 1; }
// Polygon    { repeated Point points   = 1; }
// Point      { sint64 x = 1; sint64 y = 2; }
class MiniPbufEncoder final : public Encoder {
 public:
  std::string Name() const override;
  EncodeResult Encode(const PolygonSet& polygon_set) const override;
  DecodeResult Decode(const std::vector<uint8_t>& bytes) const override;
};

}  // namespace polybench
