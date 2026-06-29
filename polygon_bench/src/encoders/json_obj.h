#pragma once

#include "encoders/encoder.h"

namespace polybench {

// JSON 对象点表达：
// - Point: {"x":123,"y":456}
// - PolygonSet: [ [ {..},{..} ], [ {..} ] ]
class JsonObjEncoder final : public Encoder {
 public:
  std::string Name() const override;
  EncodeResult Encode(const PolygonSet& polygon_set) const override;
  DecodeResult Decode(const std::vector<uint8_t>& bytes) const override;
};

}  // namespace polybench
