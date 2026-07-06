#pragma once

#include "encoders/encoder.h"

namespace polybench {

class DeltaWindowMinVarintEncoder final : public Encoder {
 public:
  std::string Name() const override;
  EncodeResult Encode(const PolygonSet& polygon_set) const override;
  DecodeResult Decode(const std::vector<uint8_t>& bytes) const override;
};

}  // namespace polybench

