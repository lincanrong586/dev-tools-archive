#include "encoders/delta_varint.h"

#include <stdexcept>

#include "varint.h"

namespace polybench {

std::string DeltaVarintEncoder::Name() const { return "delta_varint"; }

EncodeResult DeltaVarintEncoder::Encode(const PolygonSet& polygon_set) const {
  EncodeResult r;
  auto& out = r.bytes;
  out.reserve(polygon_set.size() * 4);

  AppendUVarint(static_cast<uint32_t>(polygon_set.size()), out);
  for (const auto& poly : polygon_set) {
    AppendUVarint(static_cast<uint32_t>(poly.size()), out);

    long long prev_x = 0;
    long long prev_y = 0;
    bool first = true;

    for (const auto& p : poly) {
      long long dx = p.x;
      long long dy = p.y;
      if (!first) {
        dx = p.x - prev_x;
        dy = p.y - prev_y;
      }
      first = false;
      prev_x = p.x;
      prev_y = p.y;

      AppendUVarint64(ZigZagEncode64(dx), out);
      AppendUVarint64(ZigZagEncode64(dy), out);
    }
  }
  return r;
}

DecodeResult DeltaVarintEncoder::Decode(const std::vector<uint8_t>& bytes) const {
  const uint8_t* p = bytes.data();
  const uint8_t* end = bytes.data() + bytes.size();

  DecodeResult r;
  const uint32_t polygons = ReadUVarint(p, end);
  r.polygon_set.reserve(polygons);

  for (uint32_t i = 0; i < polygons; ++i) {
    const uint32_t points = ReadUVarint(p, end);
    Polygon poly;
    poly.reserve(points);

    long long prev_x = 0;
    long long prev_y = 0;
    bool first = true;

    for (uint32_t j = 0; j < points; ++j) {
      const long long dx = ZigZagDecode64(ReadUVarint64(p, end));
      const long long dy = ZigZagDecode64(ReadUVarint64(p, end));

      Point pt;
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
      poly.push_back(pt);
    }
    r.polygon_set.push_back(std::move(poly));
  }
  if (p != end) throw std::runtime_error("delta_varint: trailing bytes");
  return r;
}

}  // namespace polybench
