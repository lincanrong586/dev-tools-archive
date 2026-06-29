#include "encoders/json_arr.h"

#include <string>

#include "encoders/json_utils.h"

namespace polybench {

std::string JsonArrEncoder::Name() const { return "json_arr"; }

EncodeResult JsonArrEncoder::Encode(const PolygonSet& polygon_set) const {
  std::string s;
  s.reserve(PointCount(polygon_set) * 10);

  s.push_back('[');
  for (size_t i = 0; i < polygon_set.size(); ++i) {
    if (i) s.push_back(',');
    s.push_back('[');
    const auto& poly = polygon_set[i];
    for (size_t j = 0; j < poly.size(); ++j) {
      if (j) s.push_back(',');
      s.push_back('[');
      s += std::to_string(poly[j].x);
      s.push_back(',');
      s += std::to_string(poly[j].y);
      s.push_back(']');
    }
    s.push_back(']');
  }
  s.push_back(']');

  EncodeResult r;
  r.bytes.assign(s.begin(), s.end());
  return r;
}

DecodeResult JsonArrEncoder::Decode(const std::vector<uint8_t>& bytes) const {
  const char* p = reinterpret_cast<const char*>(bytes.data());
  const char* end = p + bytes.size();

  DecodeResult r;
  ExpectChar(p, end, '[');
  if (ConsumeChar(p, end, ']')) {
    SkipWS(p, end);
    if (p != end) throw std::runtime_error("json_arr: trailing bytes");
    return r;
  }

  while (true) {
    Polygon poly;
    ExpectChar(p, end, '[');
    if (!ConsumeChar(p, end, ']')) {
      while (true) {
        ExpectChar(p, end, '[');
        const int32_t x = ParseI32(p, end);
        ExpectChar(p, end, ',');
        const int32_t y = ParseI32(p, end);
        ExpectChar(p, end, ']');
        poly.push_back(Point{x, y});
        if (ConsumeChar(p, end, ',')) continue;
        break;
      }
      ExpectChar(p, end, ']');
    }
    r.polygon_set.push_back(std::move(poly));
    if (ConsumeChar(p, end, ',')) continue;
    break;
  }
  ExpectChar(p, end, ']');
  SkipWS(p, end);
  if (p != end) throw std::runtime_error("json_arr: trailing bytes");
  return r;
}

}  // namespace polybench
