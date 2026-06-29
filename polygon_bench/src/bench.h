#pragma once

#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#include "encoders/encoder.h"
#include "generate.h"

namespace polybench {

// 单行基准测试结果。
//
// 约定：
// - poly(MB): 估算的 PolygonSet 内存占用
// - xxx(MB): 编码后的大小
// - xxx_enc/ms, xxx_dec/ms: 平均编解码耗时
struct BenchRow {
  std::string scenario;
  uint32_t polygons = 0;
  uint32_t points = 0;
  double poly_mb = 0.0;

  double pbuf_mb = 0.0;
  double pbuf_enc_ms = 0.0;
  double pbuf_dec_ms = 0.0;

  double bin_fixed_mb = 0.0;
  double bin_fixed_enc_ms = 0.0;
  double bin_fixed_dec_ms = 0.0;

  double bin_offset_varint_mb = 0.0;
  double bin_offset_varint_enc_ms = 0.0;
  double bin_offset_varint_dec_ms = 0.0;

  double delta_varint_mb = 0.0;
  double delta_varint_enc_ms = 0.0;
  double delta_varint_dec_ms = 0.0;

#ifdef POLYBENCH_WITH_BOOST
  double boost_bin_mb = 0.0;
  double boost_bin_enc_ms = 0.0;
  double boost_bin_dec_ms = 0.0;
#endif

  double rss_mb = 0.0;
};

struct BenchConfig {
  uint32_t iterations = 3;
  bool warmup = true;
};

BenchRow RunScenarioBench(const GenerateConfig& gen_cfg, const BenchConfig& bench_cfg);

std::string RenderMarkdownTable(const std::vector<BenchRow>& rows, bool include_avg);

}  // namespace polybench
