#pragma once

#include "algorithm_codec.h"

namespace polygon_codec {

// pbuf：
// - protobuf wire-format 的最小兼容实现（不依赖 libprotobuf）
// - 用于和业务自定义二进制格式对比，或与其他语言/系统的 protobuf 协议互通时使用
//
// 参考 schema（概念层面）：
// PolygonSet { repeated Polygon polygons = 1; }
// Polygon    { repeated Point points   = 1; }
// Point      { sint64 x = 1; sint64 y = 2; }
class PbufCodec final : public IAlgorithmCodec {
 public:
  Algorithm algorithm() const override { return Algorithm::kPbuf; }
  std::vector<uint8_t> EncodePayload(const PolygonSet& ps) const override;
  PolygonSet DecodePayload(PayloadView payload) const override;
};

}  // namespace polygon_codec
