#pragma once

#include <cstddef>
#include <cstdint>
#include <stdexcept>
#include <vector>

namespace polybench {

// Encodes an unsigned integer using protobuf-style base-128 varint.
void AppendUVarint(uint32_t value, std::vector<uint8_t>& out);

// Decodes a protobuf-style varint from [p, end). Advances p on success.
uint32_t ReadUVarint(const uint8_t*& p, const uint8_t* end);

// ZigZag encoding used by protobuf sint32.
uint32_t ZigZagEncode32(int32_t v);
int32_t ZigZagDecode32(uint32_t v);

// Writes a little-endian 32-bit integer.
void AppendI32LE(int32_t v, std::vector<uint8_t>& out);

// Reads a little-endian 32-bit integer from [p, end). Advances p on success.
int32_t ReadI32LE(const uint8_t*& p, const uint8_t* end);

}  // namespace polybench

