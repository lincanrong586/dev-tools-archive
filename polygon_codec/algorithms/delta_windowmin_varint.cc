#include "delta_windowmin_varint.h"

#include <algorithm>
#include <stdexcept>

#include "../varint.h"

namespace polygon_codec {

namespace {

constexpr size_t kGlobalMinReserveBytes = 20;
constexpr size_t kVarintReservePerCount = 5;
constexpr size_t kEstimatedBytesPerPoint = 4;

}  // namespace

std::vector<uint8_t> DeltaWindowMinVarintCodec::EncodePayload(const PolygonSet& ps) const {
  long long global_min_x = 0;
  long long global_min_y = 0;
  bool first = true;
  size_t total_points = 0;
  for (const auto& poly : ps) {
    total_points += poly.size();
    for (const auto& pt : poly) {
      if (first) {
        global_min_x = pt.x;
        global_min_y = pt.y;
        first = false;
      } else {
        global_min_x = std::min(global_min_x, pt.x);
        global_min_y = std::min(global_min_y, pt.y);
      }
    }
  }

  std::vector<uint8_t> out;
  out.reserve(kGlobalMinReserveBytes + ps.size() * kVarintReservePerCount + total_points * kEstimatedBytesPerPoint);

  AppendUVarint64(ZigZagEncode64(global_min_x), out);
  AppendUVarint64(ZigZagEncode64(global_min_y), out);

  AppendUVarint(static_cast<uint32_t>(ps.size()), out);
  for (const auto& poly : ps) {
    AppendUVarint(static_cast<uint32_t>(poly.size()), out);

    long long prev_x = 0;
    long long prev_y = 0;
    bool is_first_point = true;
    for (const auto& pt : poly) {
      if (is_first_point) {
        const uint64_t rx = static_cast<uint64_t>(pt.x - global_min_x);
        const uint64_t ry = static_cast<uint64_t>(pt.y - global_min_y);
        AppendUVarint64(rx, out);
        AppendUVarint64(ry, out);
        is_first_point = false;
      } else {
        const long long dx = pt.x - prev_x;
        const long long dy = pt.y - prev_y;
        AppendUVarint64(ZigZagEncode64(dx), out);
        AppendUVarint64(ZigZagEncode64(dy), out);
      }
      prev_x = pt.x;
      prev_y = pt.y;
    }
  }

  return out;
}

PolygonSet DeltaWindowMinVarintCodec::DecodePayload(PayloadView payload) const {
  const uint8_t* p = payload.data;
  const uint8_t* end = payload.data + payload.size;

  const long long global_min_x = ZigZagDecode64(ReadUVarint64(p, end));
  const long long global_min_y = ZigZagDecode64(ReadUVarint64(p, end));

  const uint32_t polygons = ReadUVarint(p, end);
  PolygonSet ps(polygons);

  for (uint32_t i = 0; i < polygons; ++i) {
    const uint32_t points = ReadUVarint(p, end);
    Polygon& poly = ps[i];
    poly.resize(points);

    long long prev_x = 0;
    long long prev_y = 0;
    bool is_first_point = true;
    for (uint32_t j = 0; j < points; ++j) {
      Point& pt = poly[j];
      if (is_first_point) {
        const uint64_t rx = ReadUVarint64(p, end);
        const uint64_t ry = ReadUVarint64(p, end);
        pt.x = global_min_x + static_cast<long long>(rx);
        pt.y = global_min_y + static_cast<long long>(ry);
        is_first_point = false;
      } else {
        const long long dx = ZigZagDecode64(ReadUVarint64(p, end));
        const long long dy = ZigZagDecode64(ReadUVarint64(p, end));
        pt.x = prev_x + dx;
        pt.y = prev_y + dy;
      }
      prev_x = pt.x;
      prev_y = pt.y;
    }
  }

  if (p != end) throw std::runtime_error("delta_windowmin_varint: trailing bytes");
  return ps;
}

}  // namespace polygon_codec
