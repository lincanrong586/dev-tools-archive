#include "bin_offset_varint.h"

#include <algorithm>
#include <stdexcept>

#include "../varint.h"

namespace polygon_codec {

namespace {

// 经验性 reserve 常量：
// - global min_x/min_y 各预留最多 10 字节 varint，总计约 20 字节
// - polygon/point 计数字段按“每项最多 5 字节”预估
// - 单点在 offset-varint 下常见为 2 个较短 uvarint，这里按 4 字节粗估
constexpr size_t kGlobalMinReserveBytes = 20;
constexpr size_t kVarintReservePerCount = 5;
constexpr size_t kEstimatedBytesPerPoint = 4;

}  // namespace

std::vector<uint8_t> BinOffsetVarintCodec::EncodePayload(const PolygonSet& ps) const {
  long long min_x = 0;
  long long min_y = 0;
  bool first = true;
  size_t total_points = 0;
  for (const auto& poly : ps) {
    total_points += poly.size();
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

  std::vector<uint8_t> out;
  out.reserve(kGlobalMinReserveBytes + ps.size() * kVarintReservePerCount + total_points * kEstimatedBytesPerPoint);

  AppendUVarint64(ZigZagEncode64(min_x), out);
  AppendUVarint64(ZigZagEncode64(min_y), out);

  AppendUVarint(static_cast<uint32_t>(ps.size()), out);
  for (const auto& poly : ps) {
    AppendUVarint(static_cast<uint32_t>(poly.size()), out);
    for (const auto& p : poly) {
      const uint64_t rx = static_cast<uint64_t>(p.x - min_x);
      const uint64_t ry = static_cast<uint64_t>(p.y - min_y);
      AppendUVarint64(rx, out);
      AppendUVarint64(ry, out);
    }
  }
  return out;
}

PolygonSet BinOffsetVarintCodec::DecodePayload(PayloadView payload) const {
  const uint8_t* p = payload.data;
  const uint8_t* end = payload.data + payload.size;

  const long long min_x = ZigZagDecode64(ReadUVarint64(p, end));
  const long long min_y = ZigZagDecode64(ReadUVarint64(p, end));

  const uint32_t polygons = ReadUVarint(p, end);
  PolygonSet ps(polygons);

  for (uint32_t i = 0; i < polygons; ++i) {
    const uint32_t points = ReadUVarint(p, end);
    Polygon& poly = ps[i];
    poly.resize(points);
    for (uint32_t j = 0; j < points; ++j) {
      const uint64_t rx = ReadUVarint64(p, end);
      const uint64_t ry = ReadUVarint64(p, end);
      Point& pt = poly[j];
      pt.x = min_x + static_cast<long long>(rx);
      pt.y = min_y + static_cast<long long>(ry);
    }
  }

  if (p != end) throw std::runtime_error("bin_offset_varint: trailing bytes");
  return ps;
}

}  // namespace polygon_codec
