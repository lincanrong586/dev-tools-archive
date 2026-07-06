#include "encoders/delta_windowmin_varint.h"

#include <algorithm>
#include <stdexcept>

#include "varint.h"

namespace polybench {

std::string DeltaWindowMinVarintEncoder::Name() const { return "delta_windowmin_varint"; }

EncodeResult DeltaWindowMinVarintEncoder::Encode(const PolygonSet& polygon_set) const {
  long long global_min_x = 0;
  long long global_min_y = 0;
  bool first = true;
  for (const auto& poly : polygon_set) {
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

  EncodeResult r;
  auto& out = r.bytes;
  out.reserve(polygon_set.size() * 4);

  AppendUVarint64(ZigZagEncode64(global_min_x), out);
  AppendUVarint64(ZigZagEncode64(global_min_y), out);

  AppendUVarint(static_cast<uint32_t>(polygon_set.size()), out);
  for (const auto& poly : polygon_set) {
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

  return r;
}

DecodeResult DeltaWindowMinVarintEncoder::Decode(const std::vector<uint8_t>& bytes) const {
  const uint8_t* p = bytes.data();
  const uint8_t* end = bytes.data() + bytes.size();

  const long long global_min_x = ZigZagDecode64(ReadUVarint64(p, end));
  const long long global_min_y = ZigZagDecode64(ReadUVarint64(p, end));

  DecodeResult r;
  const uint32_t polygons = ReadUVarint(p, end);
  r.polygon_set.reserve(polygons);

  for (uint32_t i = 0; i < polygons; ++i) {
    const uint32_t points = ReadUVarint(p, end);
    Polygon poly;
    poly.reserve(points);

    long long prev_x = 0;
    long long prev_y = 0;
    bool is_first_point = true;
    for (uint32_t j = 0; j < points; ++j) {
      Point pt;
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
      poly.push_back(pt);
    }

    r.polygon_set.push_back(std::move(poly));
  }

  if (p != end) throw std::runtime_error("delta_windowmin_varint: trailing bytes");
  return r;
}

}  // namespace polybench
