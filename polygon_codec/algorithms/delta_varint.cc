#include "delta_varint.h"

#include <stdexcept>

#include "../varint.h"

namespace polygon_codec {

namespace {

constexpr size_t kVarintReservePerCount = 5;
constexpr size_t kEstimatedBytesPerPoint = 4;

}  // namespace

std::vector<uint8_t> DeltaVarintCodec::EncodePayload(const PolygonSet& ps) const {
  size_t total_points = 0;
  for (const auto& poly : ps) total_points += poly.size();

  std::vector<uint8_t> out;
  out.reserve(kVarintReservePerCount + ps.size() * kVarintReservePerCount + total_points * kEstimatedBytesPerPoint);

  AppendUVarint(static_cast<uint32_t>(ps.size()), out);
  for (const auto& poly : ps) {
    AppendUVarint(static_cast<uint32_t>(poly.size()), out);

    long long prev_x = 0;
    long long prev_y = 0;
    bool first = true;

    for (const auto& pt : poly) {
      long long dx = pt.x;
      long long dy = pt.y;
      if (!first) {
        dx = pt.x - prev_x;
        dy = pt.y - prev_y;
      }
      first = false;
      prev_x = pt.x;
      prev_y = pt.y;

      AppendUVarint64(ZigZagEncode64(dx), out);
      AppendUVarint64(ZigZagEncode64(dy), out);
    }
  }
  return out;
}

PolygonSet DeltaVarintCodec::DecodePayload(PayloadView payload) const {
  const uint8_t* p = payload.data;
  const uint8_t* end = payload.data + payload.size;

  const uint32_t polygons = ReadUVarint(p, end);
  PolygonSet ps(polygons);

  for (uint32_t i = 0; i < polygons; ++i) {
    const uint32_t points = ReadUVarint(p, end);
    Polygon& poly = ps[i];
    poly.resize(points);

    long long prev_x = 0;
    long long prev_y = 0;
    bool first = true;

    for (uint32_t j = 0; j < points; ++j) {
      const long long dx = ZigZagDecode64(ReadUVarint64(p, end));
      const long long dy = ZigZagDecode64(ReadUVarint64(p, end));

      Point& pt = poly[j];
      if (first) {
        pt.x = dx;
        pt.y = dy;
        first = false;
      } else {
        pt.x = prev_x + dx;
        pt.y = prev_y + dy;
      }
      prev_x = pt.x;
      prev_y = pt.y;
    }
  }

  if (p != end) throw std::runtime_error("delta_varint: trailing bytes");
  return ps;
}

}  // namespace polygon_codec
