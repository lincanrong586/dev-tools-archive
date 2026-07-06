#pragma once

#include "algorithm_codec.h"

namespace polygon_codec {

// bin_fixed：
// - 采用定长 little-endian 写入坐标（int64）
// - 体积通常较大，但编解码逻辑简单、解码速度快
class BinFixedCodec final : public IAlgorithmCodec {
 public:
  Algorithm algorithm() const override { return Algorithm::kBinFixed; }
  std::vector<uint8_t> EncodePayload(const PolygonSet& ps) const override;
  PolygonSet DecodePayload(PayloadView payload) const override;
};

}  // namespace polygon_codec
