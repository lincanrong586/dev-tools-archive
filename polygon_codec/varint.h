#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace polygon_codec {

// 使用 protobuf 风格的 base-128 varint 编码无符号整数。
// 说明：该函数仅负责“按字节写入”，不做边界检查。
void AppendUVarint(uint32_t value, std::vector<uint8_t>& out);
void AppendUVarint64(uint64_t value, std::vector<uint8_t>& out);

// 从 [p, end) 读取 varint，并推进指针 p。
// 若数据非法/越界，会抛出 std::runtime_error。
uint32_t ReadUVarint(const uint8_t*& p, const uint8_t* end);
uint64_t ReadUVarint64(const uint8_t*& p, const uint8_t* end);

// ZigZag 编码：将有符号整数映射到无符号整数，使得“小幅正负数”更容易被 varint 压缩。
uint32_t ZigZagEncode32(int32_t v);
int32_t ZigZagDecode32(uint32_t v);
uint64_t ZigZagEncode64(long long v);
long long ZigZagDecode64(uint64_t v);

// 以 little-endian 写入/读取定长整数。
void AppendI32LE(int32_t v, std::vector<uint8_t>& out);
void AppendI64LE(long long v, std::vector<uint8_t>& out);
int32_t ReadI32LE(const uint8_t*& p, const uint8_t* end);
long long ReadI64LE(const uint8_t*& p, const uint8_t* end);

}  // namespace polygon_codec

