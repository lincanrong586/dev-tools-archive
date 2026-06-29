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

    int32_t prev_x = 0;
    int32_t prev_y = 0;
    bool first = true;

    for (const auto& p : poly) {
      int32_t dx = p.x;
      int32_t dy = p.y;
      if (!first) {
        dx = p.x - prev_x;
        dy = p.y - prev_y;
      }
      first = false;
      prev_x = p.x;
      prev_y = p.y;

      AppendUVarint(ZigZagEncode32(dx), out);
      AppendUVarint(ZigZagEncode32(dy), out);
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

    int32_t prev_x = 0;
    int32_t prev_y = 0;
    bool first = true;

    for (uint32_t j = 0; j < points; ++j) {
      const int32_t dx = ZigZagDecode32(ReadUVarint(p, end));
      const int32_t dy = ZigZagDecode32(ReadUVarint(p, end));

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
