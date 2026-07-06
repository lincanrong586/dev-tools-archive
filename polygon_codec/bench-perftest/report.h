#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "../codec.h"

namespace polygon_codec::perftest {

struct Stat3 {
  double avg = 0.0;
  double min = 0.0;
  double max = 0.0;
};

struct U32Stat3 {
  uint32_t avg = 0;
  uint32_t min = 0;
  uint32_t max = 0;
};

struct ReportRow {
  std::string scenario;
  U32Stat3 polygons;
  U32Stat3 points;
  Stat3 poly_mb;

  std::vector<Algorithm> algos;
  std::vector<Stat3> size_mb;
  std::vector<Stat3> bytes_per_polygon;
  std::vector<Stat3> enc_ms;
  std::vector<Stat3> dec_ms;
};

std::string AlgorithmName(Algorithm algo);
std::string RenderMarkdown(const std::string& title, const std::vector<ReportRow>& rows, bool include_avg);

}  // namespace polygon_codec::perftest

