#pragma once

#include "encoders/encoder.h"

namespace polybench {

// JSON 数组点表达：
// - Point: [123,456]
// - PolygonSet: [ [ [x,y],[x,y] ], [ [x,y] ] ]
class JsonArrEncoder final : public Encoder {
 public:
  std::string Name() const override;
  EncodeResult Encode(const PolygonSet& polygon_set) const override;
  DecodeResult Decode(const std::vector<uint8_t>& bytes) const override;
};

}  // namespace polybench
