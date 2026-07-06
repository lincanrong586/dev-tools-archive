#pragma once

#include "algorithm_codec.h"

namespace polygon_codec {

// bin_offset_varint：
// - 先计算整份 PolygonSet 的全局最小坐标 (min_x, min_y)
// - 头部写入 (min_x, min_y)（ZigZag + uvarint64）
// - 每个点写入 (x-min_x, y-min_y) 的 uvarint64（非负，且数值通常落在窗口范围内）
class BinOffsetVarintCodec final : public IAlgorithmCodec {
 public:
  Algorithm algorithm() const override { return Algorithm::kBinOffsetVarint; }
  std::vector<uint8_t> EncodePayload(const PolygonSet& ps) const override;
  PolygonSet DecodePayload(PayloadView payload) const override;
};

}  // namespace polygon_codec
