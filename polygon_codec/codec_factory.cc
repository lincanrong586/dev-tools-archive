#include "codec_factory.h"

#include "algorithms/bin_fixed.h"
#include "algorithms/bin_offset_varint.h"
#include "algorithms/delta_varint.h"
#include "algorithms/delta_windowmin_varint.h"
#include "algorithms/pbuf.h"
#include "algorithms/boost_bin.h"

namespace polygon_codec {

std::unique_ptr<IAlgorithmCodec> CodecFactory::Create(Algorithm algo) {
  switch (algo) {
    case Algorithm::kBinFixed:
      return std::make_unique<BinFixedCodec>();
    case Algorithm::kBinOffsetVarint:
      return std::make_unique<BinOffsetVarintCodec>();
    case Algorithm::kDeltaVarint:
      return std::make_unique<DeltaVarintCodec>();
    case Algorithm::kDeltaWindowMinVarint:
      return std::make_unique<DeltaWindowMinVarintCodec>();
    case Algorithm::kPbuf:
      return std::make_unique<PbufCodec>();
    case Algorithm::kBoostBin:
#ifdef POLYGON_CODEC_WITH_BOOST
      return std::make_unique<BoostBinCodec>();
#else
      return nullptr;
#endif
    case Algorithm::kButt:
      return nullptr;
  }
  return nullptr;
}

}  // namespace polygon_codec
