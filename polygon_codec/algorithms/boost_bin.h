#pragma once

#include "algorithm_codec.h"

namespace polygon_codec {

// boost_bin：
// - 使用 Boost.Serialization 的 binary archive
// - 该算法是否可用取决于编译时是否启用 Boost（宏 POLYGON_CODEC_WITH_BOOST）
class BoostBinCodec final : public IAlgorithmCodec {
 public:
  Algorithm algorithm() const override { return Algorithm::kBoostBin; }
  std::vector<uint8_t> EncodePayload(const PolygonSet& ps) const override;
  PolygonSet DecodePayload(PayloadView payload) const override;
};

}  // namespace polygon_codec
