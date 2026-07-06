#pragma once

#include <cstdint>
#include <vector>

#include "codec.h"
#include "status.h"

namespace polygon_codec {

// FrameHeader 为统一包头解析结果。
// 包头用于：
// - 快速校验数据是否属于本库（magic）
// - 校验版本兼容性（version）
// - 标识 payload 使用的算法（algorithm）
struct FrameHeader {
  Algorithm algorithm = Algorithm::kBinFixed;
};

// PayloadView 表示 frame 中 payload 的零拷贝视图。
// data 直接指向原始字节缓冲区内部，不拥有内存。
struct PayloadView {
  const uint8_t* data = nullptr;
  size_t size = 0;
};

// WrapFrame 会将 payload 包上一层统一包头，形成可自描述字节流。
std::vector<uint8_t> WrapFrame(Algorithm algo, const std::vector<uint8_t>& payload);

// ParseHeader 解析包头（不复制 payload）。
StatusOr<FrameHeader> ParseHeader(const std::vector<uint8_t>& bytes);

// ExtractPayloadView 返回 payload 的零拷贝视图（从第 8 字节开始）。
StatusOr<PayloadView> ExtractPayloadView(const std::vector<uint8_t>& bytes);

// ExtractPayload 返回 payload 的拷贝（从第 8 字节开始）。
// 仅用于兼容旧路径；性能敏感场景建议优先使用 ExtractPayloadView。
StatusOr<std::vector<uint8_t>> ExtractPayload(const std::vector<uint8_t>& bytes);

}  // namespace polygon_codec
