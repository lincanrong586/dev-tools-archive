#include "codec_frame.h"

#include <array>

namespace polygon_codec {

namespace {

// 包头格式（固定 8 字节）：
// magic(4) + version(1) + algorithm(1) + flags(1) + reserved(1)
constexpr std::array<uint8_t, 4> kMagic = {'P', 'C', 'O', 'D'};
constexpr uint8_t kVersion = 1;
constexpr size_t kHeaderSize = 8;

static bool IsValidAlgorithmByte(uint8_t v) {
  const uint8_t low = static_cast<uint8_t>(Algorithm::kBinFixed);
  const uint8_t high = static_cast<uint8_t>(Algorithm::kButt);
  return v >= low && v < high;
}

}  // namespace

std::vector<uint8_t> WrapFrame(Algorithm algo, const std::vector<uint8_t>& payload) {
  std::vector<uint8_t> out;
  out.reserve(kHeaderSize + payload.size());
  out.insert(out.end(), kMagic.begin(), kMagic.end());
  out.push_back(kVersion);
  out.push_back(static_cast<uint8_t>(algo));
  out.push_back(0);
  out.push_back(0);
  out.insert(out.end(), payload.begin(), payload.end());
  return out;
}

StatusOr<FrameHeader> ParseHeader(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < kHeaderSize) return Status::Error("codec_frame: 数据长度不足，无法解析包头");
  for (size_t i = 0; i < kMagic.size(); ++i) {
    if (bytes[i] != kMagic[i]) return Status::Error("codec_frame: magic 不匹配，数据不是 polygon_codec 格式");
  }
  const uint8_t version = bytes[4];
  if (version != kVersion) return Status::Error("codec_frame: 版本不兼容");

  const uint8_t algo_v = bytes[5];
  if (!IsValidAlgorithmByte(algo_v)) return Status::Error("codec_frame: algorithm 字段非法");

  FrameHeader h;
  h.algorithm = static_cast<Algorithm>(algo_v);
  return h;
}

StatusOr<PayloadView> ExtractPayloadView(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < kHeaderSize) return Status::Error("codec_frame: 数据长度不足，无法提取 payload");
  return PayloadView{bytes.data() + kHeaderSize, bytes.size() - kHeaderSize};
}

StatusOr<std::vector<uint8_t>> ExtractPayload(const std::vector<uint8_t>& bytes) {
  if (bytes.size() < kHeaderSize) return Status::Error("codec_frame: 数据长度不足，无法提取 payload");
  return std::vector<uint8_t>(bytes.begin() + kHeaderSize, bytes.end());
}

}  // namespace polygon_codec
