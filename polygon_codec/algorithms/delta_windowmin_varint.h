#pragma once

#include "algorithm_codec.h"

namespace polygon_codec {

// delta_windowmin_varint：
// - 先计算整份 PolygonSet 的全局最小坐标 (global_min_x, global_min_y)
// - payload 头部写入 global_min_x/global_min_y（ZigZag + uvarint64）
// - 每个 polygon 的第一个点写入相对 global_min 的偏移 (x0-global_min_x, y0-global_min_y)，直接 uvarint64
// - 后续点写入相邻差值 dx/dy（ZigZag + uvarint64）
//
// 该算法的设计目的：
// - 在数据“沿轮廓连续”时，delta 部分能压缩得很小
// - 同时避免首点写入很大的绝对坐标（用相对 global_min 的偏移代替）
class DeltaWindowMinVarintCodec final : public IAlgorithmCodec {
 public:
  Algorithm algorithm() const override { return Algorithm::kDeltaWindowMinVarint; }
  std::vector<uint8_t> EncodePayload(const PolygonSet& ps) const override;
  PolygonSet DecodePayload(PayloadView payload) const override;
};

}  // namespace polygon_codec
