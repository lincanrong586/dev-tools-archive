#pragma once

#include <cctype>
#include <cstdint>
#include <stdexcept>

namespace polybench {

inline void SkipWS(const char*& p, const char* end) {
  while (p < end && std::isspace(static_cast<unsigned char>(*p))) ++p;
}

inline void ExpectChar(const char*& p, const char* end, char c) {
  SkipWS(p, end);
  if (p >= end || *p != c) throw std::runtime_error(std::string("json parse: expected '") + c + "'");
  ++p;
}

inline bool ConsumeChar(const char*& p, const char* end, char c) {
  SkipWS(p, end);
  if (p < end && *p == c) {
    ++p;
    return true;
  }
  return false;
}

inline int32_t ParseI32(const char*& p, const char* end) {
  SkipWS(p, end);
  if (p >= end) throw std::runtime_error("json parse: unexpected EOF (int)");

  bool neg = false;
  if (*p == '-') {
    neg = true;
    ++p;
  }
  if (p >= end || !std::isdigit(static_cast<unsigned char>(*p))) throw std::runtime_error("json parse: invalid int");

  int64_t v = 0;
  while (p < end && std::isdigit(static_cast<unsigned char>(*p))) {
    v = v * 10 + (*p - '0');
    if (v > static_cast<int64_t>(INT32_MAX) + 1) throw std::runtime_error("json parse: int out of range");
    ++p;
  }
  if (neg) v = -v;
  if (v < INT32_MIN || v > INT32_MAX) throw std::runtime_error("json parse: int out of range");
  return static_cast<int32_t>(v);
}

inline void ExpectLiteral(const char*& p, const char* end, const char* lit) {
  SkipWS(p, end);
  for (const char* q = lit; *q; ++q) {
    if (p >= end || *p != *q) throw std::runtime_error("json parse: unexpected token");
    ++p;
  }
}

}  // namespace polybench

