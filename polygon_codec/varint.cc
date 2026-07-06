#include "varint.h"

namespace polygon_codec {

namespace {

// 定长整数（LE）读写常量，用于减少魔鬼数字并集中管理。
constexpr size_t kI32Bytes = 4;
constexpr size_t kI64Bytes = 8;
constexpr uint32_t kBitsPerByte = 8;
constexpr uint64_t kByteMask = 0xFFULL;
constexpr uint8_t kVarintContinueBit = 0x80U;
constexpr uint8_t kVarintPayloadMask = 0x7FU;
constexpr uint32_t kVarintShiftStep = 7;
constexpr uint32_t kZigZag32SignShift = 31;
constexpr uint32_t kZigZag64SignShift = 63;

}  // namespace

void AppendUVarint(uint32_t value, std::vector<uint8_t>& out) {
  if (value < kVarintContinueBit) {
    out.push_back(static_cast<uint8_t>(value));
    return;
  }
  while (value >= kVarintContinueBit) {
    out.push_back(static_cast<uint8_t>(value) | kVarintContinueBit);
    value >>= kVarintShiftStep;
  }
  out.push_back(static_cast<uint8_t>(value));
}

void AppendUVarint64(uint64_t value, std::vector<uint8_t>& out) {
  if (value < kVarintContinueBit) {
    out.push_back(static_cast<uint8_t>(value));
    return;
  }
  while (value >= kVarintContinueBit) {
    out.push_back(static_cast<uint8_t>(value) | kVarintContinueBit);
    value >>= kVarintShiftStep;
  }
  out.push_back(static_cast<uint8_t>(value));
}

uint32_t ReadUVarint(const uint8_t*& p, const uint8_t* end) {
  if (p >= end) throw std::runtime_error("invalid varint");

  const uint8_t b0 = *p++;
  if ((b0 & kVarintContinueBit) == 0) return b0;
  uint32_t value = static_cast<uint32_t>(b0 & kVarintPayloadMask);

  if (p >= end) throw std::runtime_error("invalid varint");
  const uint8_t b1 = *p++;
  value |= static_cast<uint32_t>(b1 & kVarintPayloadMask) << kVarintShiftStep;
  if ((b1 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint");
  const uint8_t b2 = *p++;
  value |= static_cast<uint32_t>(b2 & kVarintPayloadMask) << (kVarintShiftStep * 2U);
  if ((b2 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint");
  const uint8_t b3 = *p++;
  value |= static_cast<uint32_t>(b3 & kVarintPayloadMask) << (kVarintShiftStep * 3U);
  if ((b3 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint");
  const uint8_t b4 = *p++;
  value |= static_cast<uint32_t>(b4 & kVarintPayloadMask) << (kVarintShiftStep * 4U);
  if ((b4 & kVarintContinueBit) == 0) return value;

  throw std::runtime_error("invalid varint");
}

uint64_t ReadUVarint64(const uint8_t*& p, const uint8_t* end) {
  if (p >= end) throw std::runtime_error("invalid varint64");

  const uint8_t b0 = *p++;
  if ((b0 & kVarintContinueBit) == 0) return b0;
  uint64_t value = static_cast<uint64_t>(b0 & kVarintPayloadMask);

  if (p >= end) throw std::runtime_error("invalid varint64");
  const uint8_t b1 = *p++;
  value |= static_cast<uint64_t>(b1 & kVarintPayloadMask) << kVarintShiftStep;
  if ((b1 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint64");
  const uint8_t b2 = *p++;
  value |= static_cast<uint64_t>(b2 & kVarintPayloadMask) << (kVarintShiftStep * 2U);
  if ((b2 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint64");
  const uint8_t b3 = *p++;
  value |= static_cast<uint64_t>(b3 & kVarintPayloadMask) << (kVarintShiftStep * 3U);
  if ((b3 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint64");
  const uint8_t b4 = *p++;
  value |= static_cast<uint64_t>(b4 & kVarintPayloadMask) << (kVarintShiftStep * 4U);
  if ((b4 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint64");
  const uint8_t b5 = *p++;
  value |= static_cast<uint64_t>(b5 & kVarintPayloadMask) << (kVarintShiftStep * 5U);
  if ((b5 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint64");
  const uint8_t b6 = *p++;
  value |= static_cast<uint64_t>(b6 & kVarintPayloadMask) << (kVarintShiftStep * 6U);
  if ((b6 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint64");
  const uint8_t b7 = *p++;
  value |= static_cast<uint64_t>(b7 & kVarintPayloadMask) << (kVarintShiftStep * 7U);
  if ((b7 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint64");
  const uint8_t b8 = *p++;
  value |= static_cast<uint64_t>(b8 & kVarintPayloadMask) << (kVarintShiftStep * 8U);
  if ((b8 & kVarintContinueBit) == 0) return value;

  if (p >= end) throw std::runtime_error("invalid varint64");
  const uint8_t b9 = *p++;
  value |= static_cast<uint64_t>(b9 & 0x01U) << kZigZag64SignShift;
  if ((b9 & kVarintContinueBit) == 0) return value;

  throw std::runtime_error("invalid varint64");
}

uint32_t ZigZagEncode32(int32_t v) {
  return (static_cast<uint32_t>(v) << 1U) ^ static_cast<uint32_t>(v >> kZigZag32SignShift);
}

int32_t ZigZagDecode32(uint32_t v) {
  return static_cast<int32_t>((v >> 1U) ^ static_cast<uint32_t>(-static_cast<int32_t>(v & 1U)));
}

uint64_t ZigZagEncode64(long long v) {
  return (static_cast<uint64_t>(v) << 1U) ^ static_cast<uint64_t>(v >> kZigZag64SignShift);
}

long long ZigZagDecode64(uint64_t v) {
  return static_cast<long long>((v >> 1U) ^ static_cast<uint64_t>(-static_cast<long long>(v & 1U)));
}

void AppendI32LE(int32_t v, std::vector<uint8_t>& out) {
  const uint32_t uv = static_cast<uint32_t>(v);
  const size_t n = out.size();
  out.resize(n + kI32Bytes);
  for (size_t i = 0; i < kI32Bytes; ++i) {
    out[n + i] = static_cast<uint8_t>((static_cast<uint64_t>(uv) >> (kBitsPerByte * i)) & kByteMask);
  }
}

void AppendI64LE(long long v, std::vector<uint8_t>& out) {
  const uint64_t uv = static_cast<uint64_t>(v);
  const size_t n = out.size();
  out.resize(n + kI64Bytes);
  for (size_t i = 0; i < kI64Bytes; ++i) {
    out[n + i] = static_cast<uint8_t>((uv >> (kBitsPerByte * i)) & kByteMask);
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
  if (end - p < static_cast<ptrdiff_t>(kI64Bytes)) throw std::runtime_error("unexpected EOF (i64)");
  uint64_t uv = 0;
  for (size_t i = 0; i < kI64Bytes; ++i) {
    uv |= static_cast<uint64_t>(p[i]) << (kBitsPerByte * i);
  }
  p += kI64Bytes;
  return static_cast<long long>(uv);
}

}  // namespace polygon_codec
