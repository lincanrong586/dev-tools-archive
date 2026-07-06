#include "codec_metrics.h"

#include <iostream>

namespace polygon_codec {

size_t EstimateRawBytes(const PolygonSet& ps) {
  size_t bytes = sizeof(PolygonSet);
  bytes += ps.capacity() * sizeof(Polygon);
  for (const auto& poly : ps) {
    bytes += poly.capacity() * sizeof(Point);
  }
  return bytes;
}

static const char* AlgoName(Algorithm a) {
  switch (a) {
    case Algorithm::kBinFixed:
      return "bin_fixed";
    case Algorithm::kBinOffsetVarint:
      return "bin_offset_varint";
    case Algorithm::kDeltaVarint:
      return "delta_varint";
    case Algorithm::kDeltaWindowMinVarint:
      return "delta_windowmin_varint";
    case Algorithm::kPbuf:
      return "pbuf";
    case Algorithm::kBoostBin:
      return "boost_bin";
    case Algorithm::kButt:
      return "butt";
  }
  return "unknown";
}

void PrintMetricsLine(const char* stage, Algorithm algo, const Metrics& m) {
  std::cout << "[polygon_codec] " << (stage ? stage : "") << " algo=" << AlgoName(algo)
            << " encoded_bytes=" << m.encoded_bytes << " raw_bytes=" << m.raw_bytes_estimated
            << " ratio=" << m.compression_ratio << " bytes/polygon=" << m.bytes_per_polygon
            << " enc_ms=" << m.encode_ms << " dec_ms=" << m.decode_ms << "\n";
}

}  // namespace polygon_codec
