#include "pbuf.h"

#include <stdexcept>

#include "../varint.h"

namespace polygon_codec {

namespace {

constexpr uint32_t kTagFieldShift = 3;
constexpr uint32_t kWireVarint = 0;
constexpr uint32_t kWireLengthDelimited = 2;
constexpr uint32_t kPointFieldX = 1;
constexpr uint32_t kPointFieldY = 2;
constexpr uint32_t kPolygonFieldPoints = 1;
constexpr size_t kPointReserveBytes = 24;
constexpr size_t kPolygonReserveBytesPerPoint = 16;
constexpr size_t kPayloadReservePerPolygon = 4;
constexpr size_t kPayloadReservePerPoint = 12;

static void AppendTag(uint32_t field_number, uint32_t wire_type, std::vector<uint8_t>& out) {
  AppendUVarint((field_number << kTagFieldShift) | wire_type, out);
}

static std::vector<uint8_t> EncodePoint(const Point& p) {
  std::vector<uint8_t> out;
  out.reserve(kPointReserveBytes);
  AppendTag(kPointFieldX, kWireVarint, out);
  AppendUVarint64(ZigZagEncode64(p.x), out);
  AppendTag(kPointFieldY, kWireVarint, out);
  AppendUVarint64(ZigZagEncode64(p.y), out);
  return out;
}

static std::vector<uint8_t> EncodePolygon(const Polygon& poly) {
  std::vector<uint8_t> out;
  out.reserve(poly.size() * kPolygonReserveBytesPerPoint);
  for (const auto& pt : poly) {
    auto msg = EncodePoint(pt);
    AppendTag(kPolygonFieldPoints, kWireLengthDelimited, out);
    AppendUVarint(static_cast<uint32_t>(msg.size()), out);
    out.insert(out.end(), msg.begin(), msg.end());
  }
  return out;
}

static void SkipField(uint32_t wire, const uint8_t*& p, const uint8_t* end) {
  switch (wire) {
    case kWireVarint:
      (void)ReadUVarint(p, end);
      return;
    case kWireLengthDelimited: {
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
    const uint32_t field = tag >> kTagFieldShift;
    const uint32_t wire = tag & ((1U << kTagFieldShift) - 1U);
    if (wire != kWireVarint) {
      SkipField(wire, p, end);
      continue;
    }
    const uint64_t v = ReadUVarint64(p, end);
    if (field == kPointFieldX) pt.x = ZigZagDecode64(v);
    else if (field == kPointFieldY) pt.y = ZigZagDecode64(v);
  }
  return pt;
}

static Polygon DecodePolygon(const uint8_t* p, const uint8_t* end) {
  Polygon poly;
  while (p < end) {
    const uint32_t tag = ReadUVarint(p, end);
    const uint32_t field = tag >> kTagFieldShift;
    const uint32_t wire = tag & ((1U << kTagFieldShift) - 1U);
    if (field != kPolygonFieldPoints || wire != kWireLengthDelimited) {
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

}  // namespace

std::vector<uint8_t> PbufCodec::EncodePayload(const PolygonSet& ps) const {
  size_t total_points = 0;
  for (const auto& poly : ps) total_points += poly.size();

  std::vector<uint8_t> out;
  out.reserve(ps.size() * kPayloadReservePerPolygon + total_points * kPayloadReservePerPoint);
  for (const auto& poly : ps) {
    auto msg = EncodePolygon(poly);
    AppendTag(kPolygonFieldPoints, kWireLengthDelimited, out);
    AppendUVarint(static_cast<uint32_t>(msg.size()), out);
    out.insert(out.end(), msg.begin(), msg.end());
  }
  return out;
}

PolygonSet PbufCodec::DecodePayload(PayloadView payload) const {
  const uint8_t* p = payload.data;
  const uint8_t* end = payload.data + payload.size;

  PolygonSet ps;
  while (p < end) {
    const uint32_t tag = ReadUVarint(p, end);
    const uint32_t field = tag >> kTagFieldShift;
    const uint32_t wire = tag & ((1U << kTagFieldShift) - 1U);
    if (field != kPolygonFieldPoints || wire != kWireLengthDelimited) {
      SkipField(wire, p, end);
      continue;
    }
    const uint32_t len = ReadUVarint(p, end);
    if (static_cast<size_t>(end - p) < len) throw std::runtime_error("pbuf: unexpected EOF (polygon msg)");
    const uint8_t* sub_end = p + len;
    ps.push_back(DecodePolygon(p, sub_end));
    p = sub_end;
  }
  return ps;
}

}  // namespace polygon_codec
