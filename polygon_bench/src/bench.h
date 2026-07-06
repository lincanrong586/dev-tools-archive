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
// - polygons / points: 当前场景下多次随机迭代后的平均 polygon 数与平均总点数
// - poly(MB): 估算的 PolygonSet 内存占用
// - xxx(MB): 编码后的大小
// - xxx_enc/ms, xxx_dec/ms: 平均编解码耗时
struct BenchRow {
  std::string scenario;
  uint32_t polygons = 0;
  uint32_t polygons_min = 0;
  uint32_t polygons_max = 0;
  uint32_t points = 0;
  uint32_t points_min = 0;
  uint32_t points_max = 0;
  double poly_mb = 0.0;
  double poly_mb_min = 0.0;
  double poly_mb_max = 0.0;

  double pbuf_mb = 0.0;
  double pbuf_mb_min = 0.0;
  double pbuf_mb_max = 0.0;
  double pbuf_enc_ms = 0.0;
  double pbuf_enc_ms_min = 0.0;
  double pbuf_enc_ms_max = 0.0;
  double pbuf_dec_ms = 0.0;
  double pbuf_dec_ms_min = 0.0;
  double pbuf_dec_ms_max = 0.0;
  double pbuf_bpp = 0.0;
  double pbuf_bpp_min = 0.0;
  double pbuf_bpp_max = 0.0;

  double bin_fixed_mb = 0.0;
  double bin_fixed_mb_min = 0.0;
  double bin_fixed_mb_max = 0.0;
  double bin_fixed_enc_ms = 0.0;
  double bin_fixed_enc_ms_min = 0.0;
  double bin_fixed_enc_ms_max = 0.0;
  double bin_fixed_dec_ms = 0.0;
  double bin_fixed_dec_ms_min = 0.0;
  double bin_fixed_dec_ms_max = 0.0;
  double bin_fixed_bpp = 0.0;
  double bin_fixed_bpp_min = 0.0;
  double bin_fixed_bpp_max = 0.0;

  double bin_offset_varint_mb = 0.0;
  double bin_offset_varint_mb_min = 0.0;
  double bin_offset_varint_mb_max = 0.0;
  double bin_offset_varint_enc_ms = 0.0;
  double bin_offset_varint_enc_ms_min = 0.0;
  double bin_offset_varint_enc_ms_max = 0.0;
  double bin_offset_varint_dec_ms = 0.0;
  double bin_offset_varint_dec_ms_min = 0.0;
  double bin_offset_varint_dec_ms_max = 0.0;
  double bin_offset_varint_bpp = 0.0;
  double bin_offset_varint_bpp_min = 0.0;
  double bin_offset_varint_bpp_max = 0.0;

  double delta_varint_mb = 0.0;
  double delta_varint_mb_min = 0.0;
  double delta_varint_mb_max = 0.0;
  double delta_varint_enc_ms = 0.0;
  double delta_varint_enc_ms_min = 0.0;
  double delta_varint_enc_ms_max = 0.0;
  double delta_varint_dec_ms = 0.0;
  double delta_varint_dec_ms_min = 0.0;
  double delta_varint_dec_ms_max = 0.0;
  double delta_varint_bpp = 0.0;
  double delta_varint_bpp_min = 0.0;
  double delta_varint_bpp_max = 0.0;

  double delta_windowmin_varint_mb = 0.0;
  double delta_windowmin_varint_mb_min = 0.0;
  double delta_windowmin_varint_mb_max = 0.0;
  double delta_windowmin_varint_enc_ms = 0.0;
  double delta_windowmin_varint_enc_ms_min = 0.0;
  double delta_windowmin_varint_enc_ms_max = 0.0;
  double delta_windowmin_varint_dec_ms = 0.0;
  double delta_windowmin_varint_dec_ms_min = 0.0;
  double delta_windowmin_varint_dec_ms_max = 0.0;
  double delta_windowmin_varint_bpp = 0.0;
  double delta_windowmin_varint_bpp_min = 0.0;
  double delta_windowmin_varint_bpp_max = 0.0;

#ifdef POLYBENCH_WITH_BOOST
  double boost_bin_mb = 0.0;
  double boost_bin_mb_min = 0.0;
  double boost_bin_mb_max = 0.0;
  double boost_bin_enc_ms = 0.0;
  double boost_bin_enc_ms_min = 0.0;
  double boost_bin_enc_ms_max = 0.0;
  double boost_bin_dec_ms = 0.0;
  double boost_bin_dec_ms_min = 0.0;
  double boost_bin_dec_ms_max = 0.0;
  double boost_bin_bpp = 0.0;
  double boost_bin_bpp_min = 0.0;
  double boost_bin_bpp_max = 0.0;
#endif

  double rss_mb = 0.0;
};

struct BenchConfig {
  uint32_t iterations = 100;
  bool warmup = true;
};

BenchRow RunScenarioBench(const GenerateConfig& gen_cfg, const BenchConfig& bench_cfg);

std::string RenderMarkdownTable(const std::vector<BenchRow>& rows, bool include_avg);

}  // namespace polybench
