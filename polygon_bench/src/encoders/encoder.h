#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "polygon_types.h"

namespace polybench {

// 统一的编码/解码接口：
// - Encode: PolygonSet -> bytes
// - Decode: bytes -> PolygonSet
//
// 基准测试会对每种编码做 roundtrip 校验（Decode(Encode(x)) == x），并统计：
// - 编码耗时
// - 解码耗时
// - 编码后字节数
struct EncodeResult {
  std::vector<uint8_t> bytes;
};

struct DecodeResult {
  PolygonSet polygon_set;
};

class Encoder {
 public:
  virtual ~Encoder() = default;
  virtual std::string Name() const = 0;
  virtual EncodeResult Encode(const PolygonSet& polygon_set) const = 0;
  virtual DecodeResult Decode(const std::vector<uint8_t>& bytes) const = 0;
};

}  // namespace polybench
