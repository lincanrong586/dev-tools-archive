#include "bin_fixed.h"

#include <stdexcept>

#include "../varint.h"

namespace polygon_codec {

namespace {

// 预估容量时，将“每个计数字段大概率落在 1~5 字节”抽象成统一常量。
constexpr size_t kVarintReservePerCount = 5;
constexpr size_t kBinFixedBytesPerPoint = 16;

}  // namespace

// payload 格式：
// - polygons_count: uvarint32
// - for each polygon:
//   - points_count: uvarint32
//   - for each point:
//     - x: int64 little-endian
//     - y: int64 little-endian
std::vector<uint8_t> BinFixedCodec::EncodePayload(const PolygonSet& ps) const {
  size_t total_points = 0;
  for (const auto& poly : ps) total_points += poly.size();

  std::vector<uint8_t> out;
  out.reserve(kVarintReservePerCount + ps.size() * kVarintReservePerCount + total_points * kBinFixedBytesPerPoint);

  AppendUVarint(static_cast<uint32_t>(ps.size()), out);
  for (const auto& poly : ps) {
    AppendUVarint(static_cast<uint32_t>(poly.size()), out);
    for (const auto& p : poly) {
      AppendI64LE(p.x, out);
      AppendI64LE(p.y, out);
    }
  }
  return out;
}

PolygonSet BinFixedCodec::DecodePayload(PayloadView payload) const {
  const uint8_t* p = payload.data;
  const uint8_t* end = payload.data + payload.size;

  const uint32_t polygons = ReadUVarint(p, end);
  PolygonSet ps(polygons);

  for (uint32_t i = 0; i < polygons; ++i) {
    const uint32_t points = ReadUVarint(p, end);
    Polygon& poly = ps[i];
    poly.resize(points);
    for (uint32_t j = 0; j < points; ++j) {
      Point& pt = poly[j];
      pt.x = ReadI64LE(p, end);
      pt.y = ReadI64LE(p, end);
    }
  }

  if (p != end) throw std::runtime_error("bin_fixed: trailing bytes");
  return ps;
}

}  // namespace polygon_codec
