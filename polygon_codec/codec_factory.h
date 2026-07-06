#pragma once

#include <memory>

#include "algorithms/algorithm_codec.h"
#include "codec.h"

namespace polygon_codec {

// CodecFactory 是框架层唯一需要显式感知“有哪些算法”的位置。
// 新增/删除算法的最小修改面：
// - 修改 Algorithm 枚举（保持 kButt 在末尾）
// - 在 Create() 的 switch 中增删对应 case
class CodecFactory {
 public:
  static std::unique_ptr<IAlgorithmCodec> Create(Algorithm algo);
};

}  // namespace polygon_codec

