#include "encoders/mini_pbuf.h"

#include <stdexcept>

#include "varint.h"

namespace polybench {

std::string MiniPbufEncoder::Name() const { return "pbuf"; }

static void AppendTag(uint32_t field_number, uint32_t wire_type, std::vector<uint8_t>& out) {
  AppendUVarint((field_number << 3U) | wire_type, out);
}

static std::vector<uint8_t> EncodePoint(const Point& p) {
  std::vector<uint8_t> out;
  out.reserve(16);
  AppendTag(1, 0, out);
  AppendUVarint(ZigZagEncode32(p.x), out);
  AppendTag(2, 0, out);
  AppendUVarint(ZigZagEncode32(p.y), out);
  return out;
}

static std::vector<uint8_t> EncodePolygon(const Polygon& poly) {
  std::vector<uint8_t> out;
  out.reserve(poly.size() * 16);
  for (const auto& pt : poly) {
    auto msg = EncodePoint(pt);
    AppendTag(1, 2, out);
    AppendUVarint(static_cast<uint32_t>(msg.size()), out);
    out.insert(out.end(), msg.begin(), msg.end());
  }
  return out;
}

EncodeResult MiniPbufEncoder::Encode(const PolygonSet& polygon_set) const {
  EncodeResult r;
  auto& out = r.bytes;
  out.reserve(PointCount(polygon_set) * 8);
  for (const auto& poly : polygon_set) {
    auto msg = EncodePolygon(poly);
    AppendTag(1, 2, out);
    AppendUVarint(static_cast<uint32_t>(msg.size()), out);
    out.insert(out.end(), msg.begin(), msg.end());
  }
  return r;
}

static void SkipField(uint32_t wire, const uint8_t*& p, const uint8_t* end) {
  switch (wire) {
    case 0:
      (void)ReadUVarint(p, end);
      return;
    case 2: {
      const uint32_t len = ReadUVarint(p, end);
      if (static_cast<size_t>(end - p) < len) throw std::runtime_error("pbuf: unexpected EOF (len)");
      p += len;
      return;
    }
    default:
      throw std::runtime_error("pbuf: unsupported wire type");
  }
}

static Point DecodePoint(const uint8_t* p, const uint8_t* end) {
  Point pt;
  while (p < end) {
    const uint32_t tag = ReadUVarint(p, end);
    const uint32_t field = tag >> 3U;
    const uint32_t wire = tag & 7U;
    if (wire != 0) {
      SkipField(wire, p, end);
      continue;
    }
    const uint32_t v = ReadUVarint(p, end);
    if (field == 1) pt.x = ZigZagDecode32(v);
    else if (field == 2) pt.y = ZigZagDecode32(v);
  }
  return pt;
}

static Polygon DecodePolygon(const uint8_t* p, const uint8_t* end) {
  Polygon poly;
  while (p < end) {
    const uint32_t tag = ReadUVarint(p, end);
    const uint32_t field = tag >> 3U;
    const uint32_t wire = tag & 7U;
    if (field != 1 || wire != 2) {
      SkipField(wire, p, end);
      continue;
    }
    const uint32_t len = ReadUVarint(p, end);
    if (static_cast<size_t>(end - p) < len) throw std::runtime_error("pbuf: unexpected EOF (point msg)");
    const uint8_t* sub_end = p + len;
    poly.push_back(DecodePoint(p, sub_end));
    p = sub_end;
  }
  return poly;
}

DecodeResult MiniPbufEncoder::Decode(const std::vector<uint8_t>& bytes) const {
  const uint8_t* p = bytes.data();
  const uint8_t* end = bytes.data() + bytes.size();

  DecodeResult r;
  while (p < end) {
    const uint32_t tag = ReadUVarint(p, end);
    const uint32_t field = tag >> 3U;
    const uint32_t wire = tag & 7U;
    if (field != 1 || wire != 2) {
      SkipField(wire, p, end);
      continue;
    }
    const uint32_t len = ReadUVarint(p, end);
    if (static_cast<size_t>(end - p) < len) throw std::runtime_error("pbuf: unexpected EOF (polygon msg)");
    const uint8_t* sub_end = p + len;
    r.polygon_set.push_back(DecodePolygon(p, sub_end));
    p = sub_end;
  }
  return r;
}

}  // namespace polybench
