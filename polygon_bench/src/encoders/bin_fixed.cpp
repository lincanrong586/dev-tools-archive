#include "encoders/bin_fixed.h"

#include <stdexcept>

#include "varint.h"

namespace polybench {

std::string BinFixedEncoder::Name() const { return "bin_fixed"; }

EncodeResult BinFixedEncoder::Encode(const PolygonSet& polygon_set) const {
  EncodeResult r;
  auto& out = r.bytes;
  out.reserve(polygon_set.size() * 4);

  AppendUVarint(static_cast<uint32_t>(polygon_set.size()), out);
  for (const auto& poly : polygon_set) {
    AppendUVarint(static_cast<uint32_t>(poly.size()), out);
    for (const auto& p : poly) {
      AppendI32LE(p.x, out);
      AppendI32LE(p.y, out);
    }
  }
  return r;
}

DecodeResult BinFixedEncoder::Decode(const std::vector<uint8_t>& bytes) const {
  const uint8_t* p = bytes.data();
  const uint8_t* end = bytes.data() + bytes.size();

  DecodeResult r;
  const uint32_t polygons = ReadUVarint(p, end);
  r.polygon_set.reserve(polygons);

  for (uint32_t i = 0; i < polygons; ++i) {
    const uint32_t points = ReadUVarint(p, end);
    Polygon poly;
    poly.reserve(points);
    for (uint32_t j = 0; j < points; ++j) {
      Point pt;
      pt.x = ReadI32LE(p, end);
      pt.y = ReadI32LE(p, end);
      poly.push_back(pt);
    }
    r.polygon_set.push_back(std::move(poly));
  }
  if (p != end) {
    throw std::runtime_error("bin_fixed: trailing bytes");
  }
  return r;
}

}  // namespace polybench
