#include "encoders/bin_offset_varint.h"

#include <algorithm>
#include <stdexcept>

#include "varint.h"

namespace polybench {

std::string BinOffsetVarintEncoder::Name() const { return "bin_offset_varint"; }

EncodeResult BinOffsetVarintEncoder::Encode(const PolygonSet& polygon_set) const {
  long long min_x = 0;
  long long min_y = 0;
  bool first = true;
  for (const auto& poly : polygon_set) {
    for (const auto& p : poly) {
      if (first) {
        min_x = p.x;
        min_y = p.y;
        first = false;
      } else {
        min_x = std::min(min_x, p.x);
        min_y = std::min(min_y, p.y);
      }
    }
  }

  EncodeResult r;
  auto& out = r.bytes;
  out.reserve(polygon_set.size() * 4);

  AppendUVarint64(ZigZagEncode64(min_x), out);
  AppendUVarint64(ZigZagEncode64(min_y), out);

  AppendUVarint(static_cast<uint32_t>(polygon_set.size()), out);
  for (const auto& poly : polygon_set) {
    AppendUVarint(static_cast<uint32_t>(poly.size()), out);
    for (const auto& p : poly) {
      const uint64_t rx = static_cast<uint64_t>(p.x - min_x);
      const uint64_t ry = static_cast<uint64_t>(p.y - min_y);
      AppendUVarint64(rx, out);
      AppendUVarint64(ry, out);
    }
  }
  return r;
}

DecodeResult BinOffsetVarintEncoder::Decode(const std::vector<uint8_t>& bytes) const {
  const uint8_t* p = bytes.data();
  const uint8_t* end = bytes.data() + bytes.size();

  const long long min_x = ZigZagDecode64(ReadUVarint64(p, end));
  const long long min_y = ZigZagDecode64(ReadUVarint64(p, end));

  DecodeResult r;
  const uint32_t polygons = ReadUVarint(p, end);
  r.polygon_set.reserve(polygons);

  for (uint32_t i = 0; i < polygons; ++i) {
    const uint32_t points = ReadUVarint(p, end);
    Polygon poly;
    poly.reserve(points);
    for (uint32_t j = 0; j < points; ++j) {
      const uint64_t rx = ReadUVarint64(p, end);
      const uint64_t ry = ReadUVarint64(p, end);
      Point pt;
      pt.x = min_x + static_cast<long long>(rx);
      pt.y = min_y + static_cast<long long>(ry);
      poly.push_back(pt);
    }
    r.polygon_set.push_back(std::move(poly));
  }
  if (p != end) throw std::runtime_error("bin_offset_varint: trailing bytes");
  return r;
}

}  // namespace polybench
