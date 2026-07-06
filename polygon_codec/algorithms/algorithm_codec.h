#pragma once

#include <cstdint>
#include <vector>

#include "../codec.h"
#include "../codec_frame.h"
#include "../types.h"

namespace polygon_codec {

// IAlgorithmCodec 是“算法层”的统一接口。
//
// 设计目标：
// - 每个算法只关心 payload 的编解码，不关心包头/统计/打印
// - 新增/删除算法时，只需要：
//   1) 增删 algorithms/xxx.h/.cc
//   2) 修改 codec_factory.cc 的 Create() 路由
class IAlgorithmCodec {
 public:
  virtual ~IAlgorithmCodec() = default;
  virtual Algorithm algorithm() const = 0;

  // EncodePayload：将 PolygonSet 编成该算法的 payload 字节（不含统一包头）。
  virtual std::vector<uint8_t> EncodePayload(const PolygonSet& ps) const = 0;

  // DecodePayload：从 payload 视图直接解码为 PolygonSet。
  // 这里使用 PayloadView 而不是拷贝出来的 vector，目的是减少一次整段 payload 复制。
  virtual PolygonSet DecodePayload(PayloadView payload) const = 0;

  // 兼容“直接传 vector”的调用方式，默认转成 PayloadView 后复用零拷贝实现。
  virtual PolygonSet DecodePayload(const std::vector<uint8_t>& payload) const {
    return DecodePayload(PayloadView{payload.data(), payload.size()});
  }
};

}  // namespace polygon_codec
