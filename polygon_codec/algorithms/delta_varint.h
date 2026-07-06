#pragma once

#include "algorithm_codec.h"

namespace polygon_codec {

// delta_varint：
// - polygon 内按点序列写入相邻差值 dx/dy
// - dx/dy 可能为负数，因此先 ZigZag 再 uvarint64
// - 第一个点按绝对值写入（dx=x0, dy=y0）
class DeltaVarintCodec final : public IAlgorithmCodec {
 public:
  Algorithm algorithm() const override { return Algorithm::kDeltaVarint; }
  std::vector<uint8_t> EncodePayload(const PolygonSet& ps) const override;
  PolygonSet DecodePayload(PayloadView payload) const override;
};

}  // namespace polygon_codec
