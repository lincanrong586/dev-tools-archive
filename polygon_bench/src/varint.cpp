#include "varint.h"

namespace polybench {

void AppendUVarint(uint32_t value, std::vector<uint8_t>& out) {
  while (value >= 0x80U) {
    out.push_back(static_cast<uint8_t>(value) | 0x80U);
    value >>= 7U;
  }
  out.push_back(static_cast<uint8_t>(value));
}

void AppendUVarint64(uint64_t value, std::vector<uint8_t>& out) {
  while (value >= 0x80ULL) {
    out.push_back(static_cast<uint8_t>(value) | 0x80U);
    value >>= 7U;
  }
  out.push_back(static_cast<uint8_t>(value));
}

uint32_t ReadUVarint(const uint8_t*& p, const uint8_t* end) {
  uint32_t value = 0;
  uint32_t shift = 0;
  while (p < end && shift <= 28) {
    const uint8_t byte = *p++;
    value |= static_cast<uint32_t>(byte & 0x7FU) << shift;
    if ((byte & 0x80U) == 0) return value;
    shift += 7U;
  }
  throw std::runtime_error("invalid varint");
}

uint64_t ReadUVarint64(const uint8_t*& p, const uint8_t* end) {
  uint64_t value = 0;
  uint32_t shift = 0;
  while (p < end && shift <= 63) {
    const uint8_t byte = *p++;
    value |= static_cast<uint64_t>(byte & 0x7FU) << shift;
    if ((byte & 0x80U) == 0) return value;
    shift += 7U;
  }
  throw std::runtime_error("invalid varint64");
}

uint32_t ZigZagEncode32(int32_t v) {
  return (static_cast<uint32_t>(v) << 1U) ^ static_cast<uint32_t>(v >> 31);
}

int32_t ZigZagDecode32(uint32_t v) {
  return static_cast<int32_t>((v >> 1U) ^ static_cast<uint32_t>(-static_cast<int32_t>(v & 1U)));
}

uint64_t ZigZagEncode64(long long v) {
  return (static_cast<uint64_t>(v) << 1U) ^ static_cast<uint64_t>(v >> 63);
}

long long ZigZagDecode64(uint64_t v) {
  return static_cast<long long>((v >> 1U) ^ static_cast<uint64_t>(-static_cast<long long>(v & 1U)));
}

void AppendI32LE(int32_t v, std::vector<uint8_t>& out) {
  const uint32_t uv = static_cast<uint32_t>(v);
  out.push_back(static_cast<uint8_t>(uv & 0xFFU));
  out.push_back(static_cast<uint8_t>((uv >> 8U) & 0xFFU));
  out.push_back(static_cast<uint8_t>((uv >> 16U) & 0xFFU));
  out.push_back(static_cast<uint8_t>((uv >> 24U) & 0xFFU));
}

void AppendI64LE(long long v, std::vector<uint8_t>& out) {
  const uint64_t uv = static_cast<uint64_t>(v);
  for (uint32_t i = 0; i < 8; ++i) {
    out.push_back(static_cast<uint8_t>((uv >> (8U * i)) & 0xFFU));
  }
}

int32_t ReadI32LE(const uint8_t*& p, const uint8_t* end) {
  if (end - p < 4) throw std::runtime_error("unexpected EOF (i32)");
  const uint32_t b0 = p[0];
  const uint32_t b1 = p[1];
  const uint32_t b2 = p[2];
  const uint32_t b3 = p[3];
  p += 4;
  const uint32_t uv = b0 | (b1 << 8U) | (b2 << 16U) | (b3 << 24U);
  return static_cast<int32_t>(uv);
}

long long ReadI64LE(const uint8_t*& p, const uint8_t* end) {
  if (end - p < 8) throw std::runtime_error("unexpected EOF (i64)");
  uint64_t uv = 0;
  for (uint32_t i = 0; i < 8; ++i) {
    uv |= static_cast<uint64_t>(p[i]) << (8U * i);
  }
  p += 8;
  return static_cast<long long>(uv);
}

}  // namespace polybench
