#include "bench.h"

#include <cmath>
#include <chrono>
#include <functional>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>

#include "encoders/boost_bin.h"
#include "encoders/bin_fixed.h"
#include "encoders/bin_offset_varint.h"
#include "encoders/delta_varint.h"
#include "encoders/delta_windowmin_varint.h"
#include "encoders/mini_pbuf.h"
#include "rss.h"

namespace polybench {

static double ToMB(size_t bytes) { return static_cast<double>(bytes) / (1024.0 * 1024.0); }

static size_t EstimatePolygonSetBytes(const PolygonSet& ps) {
  size_t bytes = sizeof(PolygonSet);
  bytes += ps.capacity() * sizeof(Polygon);
  for (const auto& poly : ps) {
    bytes += poly.capacity() * sizeof(Point);
  }
  return bytes;
}

static double PolyDataMB(const PolygonSet& ps) { return ToMB(EstimatePolygonSetBytes(ps)); }

struct MetricStat {
  double avg = 0.0;
  double min = 0.0;
  double max = 0.0;
};

using MetricGetter = std::function<MetricStat(const BenchRow&)>;

struct MetricColumn {
  const char* name;
  MetricGetter getter;
};

struct MeasureResult {
  double enc_ms = 0.0;
  double dec_ms = 0.0;
  size_t bytes = 0;
};

static MeasureResult MeasureOnce(const Encoder& enc, const PolygonSet& ps, bool do_warmup) {
  using Clock = std::chrono::steady_clock;

  auto do_encode = [&]() { return enc.Encode(ps).bytes; };
  auto do_decode = [&](const std::vector<uint8_t>& bytes) { return enc.Decode(bytes).polygon_set; };

  if (do_warmup) {
    auto bytes = do_encode();
    (void)do_decode(bytes);
  }

  auto t0 = Clock::now();
  auto bytes = do_encode();
  auto t1 = Clock::now();
  auto ps2 = do_decode(bytes);
  auto t2 = Clock::now();

  if (ps2 != ps) {
    throw std::runtime_error("decode result mismatch for encoder: " + enc.Name());
  }

  MeasureResult r;
  r.enc_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
  r.dec_ms = std::chrono::duration<double, std::milli>(t2 - t1).count();
  r.bytes = bytes.size();
  return r;
}

BenchRow RunScenarioBench(const GenerateConfig& gen_cfg, const BenchConfig& bench_cfg) {
  BenchRow row;
  row.scenario = ScenarioName(gen_cfg.scenario);

  struct StatD {
    double sum = 0.0;
    double min = std::numeric_limits<double>::infinity();
    double max = -std::numeric_limits<double>::infinity();
  };

  struct StatU32 {
    uint64_t sum = 0;
    uint32_t min = std::numeric_limits<uint32_t>::max();
    uint32_t max = 0;
  };

  auto add_d = [](StatD& s, double v) {
    s.sum += v;
    s.min = std::min(s.min, v);
    s.max = std::max(s.max, v);
  };

  auto add_u32 = [](StatU32& s, uint32_t v) {
    s.sum += v;
    s.min = std::min(s.min, v);
    s.max = std::max(s.max, v);
  };

  MiniPbufEncoder pbuf;
  BinFixedEncoder bin_fixed;
  BinOffsetVarintEncoder bin_offset_varint;
  DeltaVarintEncoder delta_varint;
  DeltaWindowMinVarintEncoder delta_windowmin_varint;
#ifdef POLYBENCH_WITH_BOOST
  BoostBinEncoder boost_bin;
#endif

  StatU32 polygons_s;
  StatU32 points_s;
  StatD poly_mb_s;

  StatD pbuf_mb_s;
  StatD pbuf_enc_s;
  StatD pbuf_dec_s;
  StatD pbuf_bpp_s;

  StatD bin_fixed_mb_s;
  StatD bin_fixed_enc_s;
  StatD bin_fixed_dec_s;
  StatD bin_fixed_bpp_s;

  StatD bin_offset_varint_mb_s;
  StatD bin_offset_varint_enc_s;
  StatD bin_offset_varint_dec_s;
  StatD bin_offset_varint_bpp_s;

  StatD delta_varint_mb_s;
  StatD delta_varint_enc_s;
  StatD delta_varint_dec_s;
  StatD delta_varint_bpp_s;

  StatD delta_windowmin_varint_mb_s;
  StatD delta_windowmin_varint_enc_s;
  StatD delta_windowmin_varint_dec_s;
  StatD delta_windowmin_varint_bpp_s;

#ifdef POLYBENCH_WITH_BOOST
  StatD boost_bin_mb_s;
  StatD boost_bin_enc_s;
  StatD boost_bin_dec_s;
  StatD boost_bin_bpp_s;
#endif

  // 每轮迭代都重新生成一份随机 PolygonSet：
  // - 总点数在 25w~35w（默认）之间波动
  // - polygon 数和每 polygon 点数也会随 seed 变化
  //
  // 这样测出来的是“随机业务负载下的平均编解码表现”，
  // 而不是只对同一份固定数据重复编码 100 次。
  for (uint32_t i = 0; i < bench_cfg.iterations; ++i) {
    GenerateConfig iter_cfg = gen_cfg;
    iter_cfg.seed = gen_cfg.seed + i;
    const auto ps = GeneratePolygonSet(iter_cfg);

    const uint32_t polygons = static_cast<uint32_t>(ps.size());
    const uint32_t points = static_cast<uint32_t>(PointCount(ps));
    add_u32(polygons_s, polygons);
    add_u32(points_s, points);
    add_d(poly_mb_s, PolyDataMB(ps));

    {
      const auto r = MeasureOnce(pbuf, ps, bench_cfg.warmup);
      add_d(pbuf_mb_s, ToMB(r.bytes));
      add_d(pbuf_enc_s, r.enc_ms);
      add_d(pbuf_dec_s, r.dec_ms);
      add_d(pbuf_bpp_s, polygons == 0 ? 0.0 : static_cast<double>(r.bytes) / static_cast<double>(polygons));
    }

    {
      const auto r = MeasureOnce(bin_fixed, ps, bench_cfg.warmup);
      add_d(bin_fixed_mb_s, ToMB(r.bytes));
      add_d(bin_fixed_enc_s, r.enc_ms);
      add_d(bin_fixed_dec_s, r.dec_ms);
      add_d(bin_fixed_bpp_s, polygons == 0 ? 0.0 : static_cast<double>(r.bytes) / static_cast<double>(polygons));
    }

    {
      const auto r = MeasureOnce(bin_offset_varint, ps, bench_cfg.warmup);
      add_d(bin_offset_varint_mb_s, ToMB(r.bytes));
      add_d(bin_offset_varint_enc_s, r.enc_ms);
      add_d(bin_offset_varint_dec_s, r.dec_ms);
      add_d(bin_offset_varint_bpp_s, polygons == 0 ? 0.0 : static_cast<double>(r.bytes) / static_cast<double>(polygons));
    }

    {
      const auto r = MeasureOnce(delta_varint, ps, bench_cfg.warmup);
      add_d(delta_varint_mb_s, ToMB(r.bytes));
      add_d(delta_varint_enc_s, r.enc_ms);
      add_d(delta_varint_dec_s, r.dec_ms);
      add_d(delta_varint_bpp_s, polygons == 0 ? 0.0 : static_cast<double>(r.bytes) / static_cast<double>(polygons));
    }

    {
      const auto r = MeasureOnce(delta_windowmin_varint, ps, bench_cfg.warmup);
      add_d(delta_windowmin_varint_mb_s, ToMB(r.bytes));
      add_d(delta_windowmin_varint_enc_s, r.enc_ms);
      add_d(delta_windowmin_varint_dec_s, r.dec_ms);
      add_d(delta_windowmin_varint_bpp_s, polygons == 0 ? 0.0 : static_cast<double>(r.bytes) / static_cast<double>(polygons));
    }

#ifdef POLYBENCH_WITH_BOOST
    {
      const auto r = MeasureOnce(boost_bin, ps, bench_cfg.warmup);
      add_d(boost_bin_mb_s, ToMB(r.bytes));
      add_d(boost_bin_enc_s, r.enc_ms);
      add_d(boost_bin_dec_s, r.dec_ms);
      add_d(boost_bin_bpp_s, polygons == 0 ? 0.0 : static_cast<double>(r.bytes) / static_cast<double>(polygons));
    }
#endif

    row.rss_mb += CurrentRSSMB();
  }

  // 表格里保留的是“多轮随机测试的平均值”。
  const double iterations = static_cast<double>(std::max(1u, bench_cfg.iterations));

  row.polygons = static_cast<uint32_t>(std::lround(static_cast<double>(polygons_s.sum) / iterations));
  row.polygons_min = polygons_s.min == std::numeric_limits<uint32_t>::max() ? 0 : polygons_s.min;
  row.polygons_max = polygons_s.max;

  row.points = static_cast<uint32_t>(std::lround(static_cast<double>(points_s.sum) / iterations));
  row.points_min = points_s.min == std::numeric_limits<uint32_t>::max() ? 0 : points_s.min;
  row.points_max = points_s.max;

  row.poly_mb = poly_mb_s.sum / iterations;
  row.poly_mb_min = poly_mb_s.min;
  row.poly_mb_max = poly_mb_s.max;

  row.pbuf_mb = pbuf_mb_s.sum / iterations;
  row.pbuf_mb_min = pbuf_mb_s.min;
  row.pbuf_mb_max = pbuf_mb_s.max;
  row.pbuf_enc_ms = pbuf_enc_s.sum / iterations;
  row.pbuf_enc_ms_min = pbuf_enc_s.min;
  row.pbuf_enc_ms_max = pbuf_enc_s.max;
  row.pbuf_dec_ms = pbuf_dec_s.sum / iterations;
  row.pbuf_dec_ms_min = pbuf_dec_s.min;
  row.pbuf_dec_ms_max = pbuf_dec_s.max;
  row.pbuf_bpp = pbuf_bpp_s.sum / iterations;
  row.pbuf_bpp_min = pbuf_bpp_s.min;
  row.pbuf_bpp_max = pbuf_bpp_s.max;

  row.bin_fixed_mb = bin_fixed_mb_s.sum / iterations;
  row.bin_fixed_mb_min = bin_fixed_mb_s.min;
  row.bin_fixed_mb_max = bin_fixed_mb_s.max;
  row.bin_fixed_enc_ms = bin_fixed_enc_s.sum / iterations;
  row.bin_fixed_enc_ms_min = bin_fixed_enc_s.min;
  row.bin_fixed_enc_ms_max = bin_fixed_enc_s.max;
  row.bin_fixed_dec_ms = bin_fixed_dec_s.sum / iterations;
  row.bin_fixed_dec_ms_min = bin_fixed_dec_s.min;
  row.bin_fixed_dec_ms_max = bin_fixed_dec_s.max;
  row.bin_fixed_bpp = bin_fixed_bpp_s.sum / iterations;
  row.bin_fixed_bpp_min = bin_fixed_bpp_s.min;
  row.bin_fixed_bpp_max = bin_fixed_bpp_s.max;

  row.bin_offset_varint_mb = bin_offset_varint_mb_s.sum / iterations;
  row.bin_offset_varint_mb_min = bin_offset_varint_mb_s.min;
  row.bin_offset_varint_mb_max = bin_offset_varint_mb_s.max;
  row.bin_offset_varint_enc_ms = bin_offset_varint_enc_s.sum / iterations;
  row.bin_offset_varint_enc_ms_min = bin_offset_varint_enc_s.min;
  row.bin_offset_varint_enc_ms_max = bin_offset_varint_enc_s.max;
  row.bin_offset_varint_dec_ms = bin_offset_varint_dec_s.sum / iterations;
  row.bin_offset_varint_dec_ms_min = bin_offset_varint_dec_s.min;
  row.bin_offset_varint_dec_ms_max = bin_offset_varint_dec_s.max;
  row.bin_offset_varint_bpp = bin_offset_varint_bpp_s.sum / iterations;
  row.bin_offset_varint_bpp_min = bin_offset_varint_bpp_s.min;
  row.bin_offset_varint_bpp_max = bin_offset_varint_bpp_s.max;

  row.delta_varint_mb = delta_varint_mb_s.sum / iterations;
  row.delta_varint_mb_min = delta_varint_mb_s.min;
  row.delta_varint_mb_max = delta_varint_mb_s.max;
  row.delta_varint_enc_ms = delta_varint_enc_s.sum / iterations;
  row.delta_varint_enc_ms_min = delta_varint_enc_s.min;
  row.delta_varint_enc_ms_max = delta_varint_enc_s.max;
  row.delta_varint_dec_ms = delta_varint_dec_s.sum / iterations;
  row.delta_varint_dec_ms_min = delta_varint_dec_s.min;
  row.delta_varint_dec_ms_max = delta_varint_dec_s.max;
  row.delta_varint_bpp = delta_varint_bpp_s.sum / iterations;
  row.delta_varint_bpp_min = delta_varint_bpp_s.min;
  row.delta_varint_bpp_max = delta_varint_bpp_s.max;

  row.delta_windowmin_varint_mb = delta_windowmin_varint_mb_s.sum / iterations;
  row.delta_windowmin_varint_mb_min = delta_windowmin_varint_mb_s.min;
  row.delta_windowmin_varint_mb_max = delta_windowmin_varint_mb_s.max;
  row.delta_windowmin_varint_enc_ms = delta_windowmin_varint_enc_s.sum / iterations;
  row.delta_windowmin_varint_enc_ms_min = delta_windowmin_varint_enc_s.min;
  row.delta_windowmin_varint_enc_ms_max = delta_windowmin_varint_enc_s.max;
  row.delta_windowmin_varint_dec_ms = delta_windowmin_varint_dec_s.sum / iterations;
  row.delta_windowmin_varint_dec_ms_min = delta_windowmin_varint_dec_s.min;
  row.delta_windowmin_varint_dec_ms_max = delta_windowmin_varint_dec_s.max;
  row.delta_windowmin_varint_bpp = delta_windowmin_varint_bpp_s.sum / iterations;
  row.delta_windowmin_varint_bpp_min = delta_windowmin_varint_bpp_s.min;
  row.delta_windowmin_varint_bpp_max = delta_windowmin_varint_bpp_s.max;

#ifdef POLYBENCH_WITH_BOOST
  row.boost_bin_mb = boost_bin_mb_s.sum / iterations;
  row.boost_bin_mb_min = boost_bin_mb_s.min;
  row.boost_bin_mb_max = boost_bin_mb_s.max;
  row.boost_bin_enc_ms = boost_bin_enc_s.sum / iterations;
  row.boost_bin_enc_ms_min = boost_bin_enc_s.min;
  row.boost_bin_enc_ms_max = boost_bin_enc_s.max;
  row.boost_bin_dec_ms = boost_bin_dec_s.sum / iterations;
  row.boost_bin_dec_ms_min = boost_bin_dec_s.min;
  row.boost_bin_dec_ms_max = boost_bin_dec_s.max;
  row.boost_bin_bpp = boost_bin_bpp_s.sum / iterations;
  row.boost_bin_bpp_min = boost_bin_bpp_s.min;
  row.boost_bin_bpp_max = boost_bin_bpp_s.max;
#endif
  row.rss_mb /= iterations;
  return row;
}

static BenchRow AverageRow(const std::vector<BenchRow>& rows) {
  BenchRow avg;
  if (rows.empty()) return avg;
  avg.scenario = "avg";

  auto avg1 = [&](auto getter) {
    double sum = 0.0;
    for (const auto& r : rows) sum += getter(r);
    return sum / static_cast<double>(rows.size());
  };

  auto avg_u32 = [&](auto getter) {
    return static_cast<uint32_t>(std::lround(avg1([&](const BenchRow& r) { return static_cast<double>(getter(r)); })));
  };

  avg.polygons = avg_u32([](const BenchRow& r) { return r.polygons; });
  avg.polygons_min = avg_u32([](const BenchRow& r) { return r.polygons_min; });
  avg.polygons_max = avg_u32([](const BenchRow& r) { return r.polygons_max; });
  avg.points = avg_u32([](const BenchRow& r) { return r.points; });
  avg.points_min = avg_u32([](const BenchRow& r) { return r.points_min; });
  avg.points_max = avg_u32([](const BenchRow& r) { return r.points_max; });
  avg.poly_mb = avg1([](const BenchRow& r) { return r.poly_mb; });
  avg.poly_mb_min = avg1([](const BenchRow& r) { return r.poly_mb_min; });
  avg.poly_mb_max = avg1([](const BenchRow& r) { return r.poly_mb_max; });
  avg.pbuf_mb = avg1([](const BenchRow& r) { return r.pbuf_mb; });
  avg.pbuf_mb_min = avg1([](const BenchRow& r) { return r.pbuf_mb_min; });
  avg.pbuf_mb_max = avg1([](const BenchRow& r) { return r.pbuf_mb_max; });
  avg.pbuf_enc_ms = avg1([](const BenchRow& r) { return r.pbuf_enc_ms; });
  avg.pbuf_enc_ms_min = avg1([](const BenchRow& r) { return r.pbuf_enc_ms_min; });
  avg.pbuf_enc_ms_max = avg1([](const BenchRow& r) { return r.pbuf_enc_ms_max; });
  avg.pbuf_dec_ms = avg1([](const BenchRow& r) { return r.pbuf_dec_ms; });
  avg.pbuf_dec_ms_min = avg1([](const BenchRow& r) { return r.pbuf_dec_ms_min; });
  avg.pbuf_dec_ms_max = avg1([](const BenchRow& r) { return r.pbuf_dec_ms_max; });
  avg.pbuf_bpp = avg1([](const BenchRow& r) { return r.pbuf_bpp; });
  avg.pbuf_bpp_min = avg1([](const BenchRow& r) { return r.pbuf_bpp_min; });
  avg.pbuf_bpp_max = avg1([](const BenchRow& r) { return r.pbuf_bpp_max; });
  avg.bin_fixed_mb = avg1([](const BenchRow& r) { return r.bin_fixed_mb; });
  avg.bin_fixed_mb_min = avg1([](const BenchRow& r) { return r.bin_fixed_mb_min; });
  avg.bin_fixed_mb_max = avg1([](const BenchRow& r) { return r.bin_fixed_mb_max; });
  avg.bin_fixed_enc_ms = avg1([](const BenchRow& r) { return r.bin_fixed_enc_ms; });
  avg.bin_fixed_enc_ms_min = avg1([](const BenchRow& r) { return r.bin_fixed_enc_ms_min; });
  avg.bin_fixed_enc_ms_max = avg1([](const BenchRow& r) { return r.bin_fixed_enc_ms_max; });
  avg.bin_fixed_dec_ms = avg1([](const BenchRow& r) { return r.bin_fixed_dec_ms; });
  avg.bin_fixed_dec_ms_min = avg1([](const BenchRow& r) { return r.bin_fixed_dec_ms_min; });
  avg.bin_fixed_dec_ms_max = avg1([](const BenchRow& r) { return r.bin_fixed_dec_ms_max; });
  avg.bin_fixed_bpp = avg1([](const BenchRow& r) { return r.bin_fixed_bpp; });
  avg.bin_fixed_bpp_min = avg1([](const BenchRow& r) { return r.bin_fixed_bpp_min; });
  avg.bin_fixed_bpp_max = avg1([](const BenchRow& r) { return r.bin_fixed_bpp_max; });
  avg.bin_offset_varint_mb = avg1([](const BenchRow& r) { return r.bin_offset_varint_mb; });
  avg.bin_offset_varint_mb_min = avg1([](const BenchRow& r) { return r.bin_offset_varint_mb_min; });
  avg.bin_offset_varint_mb_max = avg1([](const BenchRow& r) { return r.bin_offset_varint_mb_max; });
  avg.bin_offset_varint_enc_ms = avg1([](const BenchRow& r) { return r.bin_offset_varint_enc_ms; });
  avg.bin_offset_varint_enc_ms_min = avg1([](const BenchRow& r) { return r.bin_offset_varint_enc_ms_min; });
  avg.bin_offset_varint_enc_ms_max = avg1([](const BenchRow& r) { return r.bin_offset_varint_enc_ms_max; });
  avg.bin_offset_varint_dec_ms = avg1([](const BenchRow& r) { return r.bin_offset_varint_dec_ms; });
  avg.bin_offset_varint_dec_ms_min = avg1([](const BenchRow& r) { return r.bin_offset_varint_dec_ms_min; });
  avg.bin_offset_varint_dec_ms_max = avg1([](const BenchRow& r) { return r.bin_offset_varint_dec_ms_max; });
  avg.bin_offset_varint_bpp = avg1([](const BenchRow& r) { return r.bin_offset_varint_bpp; });
  avg.bin_offset_varint_bpp_min = avg1([](const BenchRow& r) { return r.bin_offset_varint_bpp_min; });
  avg.bin_offset_varint_bpp_max = avg1([](const BenchRow& r) { return r.bin_offset_varint_bpp_max; });
  avg.delta_varint_mb = avg1([](const BenchRow& r) { return r.delta_varint_mb; });
  avg.delta_varint_mb_min = avg1([](const BenchRow& r) { return r.delta_varint_mb_min; });
  avg.delta_varint_mb_max = avg1([](const BenchRow& r) { return r.delta_varint_mb_max; });
  avg.delta_varint_enc_ms = avg1([](const BenchRow& r) { return r.delta_varint_enc_ms; });
  avg.delta_varint_enc_ms_min = avg1([](const BenchRow& r) { return r.delta_varint_enc_ms_min; });
  avg.delta_varint_enc_ms_max = avg1([](const BenchRow& r) { return r.delta_varint_enc_ms_max; });
  avg.delta_varint_dec_ms = avg1([](const BenchRow& r) { return r.delta_varint_dec_ms; });
  avg.delta_varint_dec_ms_min = avg1([](const BenchRow& r) { return r.delta_varint_dec_ms_min; });
  avg.delta_varint_dec_ms_max = avg1([](const BenchRow& r) { return r.delta_varint_dec_ms_max; });
  avg.delta_varint_bpp = avg1([](const BenchRow& r) { return r.delta_varint_bpp; });
  avg.delta_varint_bpp_min = avg1([](const BenchRow& r) { return r.delta_varint_bpp_min; });
  avg.delta_varint_bpp_max = avg1([](const BenchRow& r) { return r.delta_varint_bpp_max; });
  avg.delta_windowmin_varint_mb = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_mb; });
  avg.delta_windowmin_varint_mb_min = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_mb_min; });
  avg.delta_windowmin_varint_mb_max = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_mb_max; });
  avg.delta_windowmin_varint_enc_ms = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_enc_ms; });
  avg.delta_windowmin_varint_enc_ms_min = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_enc_ms_min; });
  avg.delta_windowmin_varint_enc_ms_max = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_enc_ms_max; });
  avg.delta_windowmin_varint_dec_ms = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_dec_ms; });
  avg.delta_windowmin_varint_dec_ms_min = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_dec_ms_min; });
  avg.delta_windowmin_varint_dec_ms_max = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_dec_ms_max; });
  avg.delta_windowmin_varint_bpp = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_bpp; });
  avg.delta_windowmin_varint_bpp_min = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_bpp_min; });
  avg.delta_windowmin_varint_bpp_max = avg1([](const BenchRow& r) { return r.delta_windowmin_varint_bpp_max; });
#ifdef POLYBENCH_WITH_BOOST
  avg.boost_bin_mb = avg1([](const BenchRow& r) { return r.boost_bin_mb; });
  avg.boost_bin_mb_min = avg1([](const BenchRow& r) { return r.boost_bin_mb_min; });
  avg.boost_bin_mb_max = avg1([](const BenchRow& r) { return r.boost_bin_mb_max; });
  avg.boost_bin_enc_ms = avg1([](const BenchRow& r) { return r.boost_bin_enc_ms; });
  avg.boost_bin_enc_ms_min = avg1([](const BenchRow& r) { return r.boost_bin_enc_ms_min; });
  avg.boost_bin_enc_ms_max = avg1([](const BenchRow& r) { return r.boost_bin_enc_ms_max; });
  avg.boost_bin_dec_ms = avg1([](const BenchRow& r) { return r.boost_bin_dec_ms; });
  avg.boost_bin_dec_ms_min = avg1([](const BenchRow& r) { return r.boost_bin_dec_ms_min; });
  avg.boost_bin_dec_ms_max = avg1([](const BenchRow& r) { return r.boost_bin_dec_ms_max; });
  avg.boost_bin_bpp = avg1([](const BenchRow& r) { return r.boost_bin_bpp; });
  avg.boost_bin_bpp_min = avg1([](const BenchRow& r) { return r.boost_bin_bpp_min; });
  avg.boost_bin_bpp_max = avg1([](const BenchRow& r) { return r.boost_bin_bpp_max; });
#endif
  avg.rss_mb = avg1([](const BenchRow& r) { return r.rss_mb; });
  return avg;
}

static std::string FormatValue(double v) {
  std::ostringstream s;
  s << std::fixed << std::setprecision(3) << v;
  return s.str();
}

static std::string FormatValue(uint32_t v) { return std::to_string(v); }

static std::string FormatValueRange(uint32_t avg, uint32_t min, uint32_t max) {
  return FormatValue(avg) + " (" + FormatValue(min) + "~" + FormatValue(max) + ")";
}

static std::string FormatValueRange(double avg, double min, double max) {
  return FormatValue(avg) + " (" + FormatValue(min) + "~" + FormatValue(max) + ")";
}

static std::vector<MetricColumn> BuildMetricColumns(const std::string& metric) {
  std::vector<MetricColumn> columns = {
      {"pbuf", [metric](const BenchRow& r) {
         return metric == "size"         ? MetricStat{r.pbuf_mb, r.pbuf_mb_min, r.pbuf_mb_max}
                : metric == "avg_bytes" ? MetricStat{r.pbuf_bpp, r.pbuf_bpp_min, r.pbuf_bpp_max}
                : metric == "encode"    ? MetricStat{r.pbuf_enc_ms, r.pbuf_enc_ms_min, r.pbuf_enc_ms_max}
                                        : MetricStat{r.pbuf_dec_ms, r.pbuf_dec_ms_min, r.pbuf_dec_ms_max};
       }},
      {"bin_fixed",
       [metric](const BenchRow& r) {
         return metric == "size"         ? MetricStat{r.bin_fixed_mb, r.bin_fixed_mb_min, r.bin_fixed_mb_max}
                : metric == "avg_bytes" ? MetricStat{r.bin_fixed_bpp, r.bin_fixed_bpp_min, r.bin_fixed_bpp_max}
                : metric == "encode"    ? MetricStat{r.bin_fixed_enc_ms, r.bin_fixed_enc_ms_min, r.bin_fixed_enc_ms_max}
                                        : MetricStat{r.bin_fixed_dec_ms, r.bin_fixed_dec_ms_min, r.bin_fixed_dec_ms_max};
       }},
      {"bin_offset_varint", [metric](const BenchRow& r) {
         return metric == "size"         ? MetricStat{r.bin_offset_varint_mb, r.bin_offset_varint_mb_min, r.bin_offset_varint_mb_max}
                : metric == "avg_bytes" ? MetricStat{r.bin_offset_varint_bpp, r.bin_offset_varint_bpp_min, r.bin_offset_varint_bpp_max}
                : metric == "encode"    ? MetricStat{r.bin_offset_varint_enc_ms, r.bin_offset_varint_enc_ms_min, r.bin_offset_varint_enc_ms_max}
                                        : MetricStat{r.bin_offset_varint_dec_ms, r.bin_offset_varint_dec_ms_min, r.bin_offset_varint_dec_ms_max};
       }},
      {"delta_varint",
       [metric](const BenchRow& r) {
         return metric == "size"         ? MetricStat{r.delta_varint_mb, r.delta_varint_mb_min, r.delta_varint_mb_max}
                : metric == "avg_bytes" ? MetricStat{r.delta_varint_bpp, r.delta_varint_bpp_min, r.delta_varint_bpp_max}
                : metric == "encode"    ? MetricStat{r.delta_varint_enc_ms, r.delta_varint_enc_ms_min, r.delta_varint_enc_ms_max}
                                        : MetricStat{r.delta_varint_dec_ms, r.delta_varint_dec_ms_min, r.delta_varint_dec_ms_max};
       }},
      {"delta_windowmin_varint",
       [metric](const BenchRow& r) {
         return metric == "size"         ? MetricStat{r.delta_windowmin_varint_mb, r.delta_windowmin_varint_mb_min, r.delta_windowmin_varint_mb_max}
                : metric == "avg_bytes" ? MetricStat{r.delta_windowmin_varint_bpp, r.delta_windowmin_varint_bpp_min, r.delta_windowmin_varint_bpp_max}
                : metric == "encode"    ? MetricStat{r.delta_windowmin_varint_enc_ms, r.delta_windowmin_varint_enc_ms_min, r.delta_windowmin_varint_enc_ms_max}
                                        : MetricStat{r.delta_windowmin_varint_dec_ms, r.delta_windowmin_varint_dec_ms_min, r.delta_windowmin_varint_dec_ms_max};
       }},
  };
#ifdef POLYBENCH_WITH_BOOST
  columns.push_back({"boost_bin", [metric](const BenchRow& r) {
                       return metric == "size"         ? MetricStat{r.boost_bin_mb, r.boost_bin_mb_min, r.boost_bin_mb_max}
                              : metric == "avg_bytes" ? MetricStat{r.boost_bin_bpp, r.boost_bin_bpp_min, r.boost_bin_bpp_max}
                              : metric == "encode"    ? MetricStat{r.boost_bin_enc_ms, r.boost_bin_enc_ms_min, r.boost_bin_enc_ms_max}
                                                      : MetricStat{r.boost_bin_dec_ms, r.boost_bin_dec_ms_min, r.boost_bin_dec_ms_max};
                     }});
#endif
  return columns;
}

static std::string HighlightValue(const MetricStat& value, double best, double worst) {
  const std::string formatted = FormatValueRange(value.avg, value.min, value.max);
  constexpr double kEpsilon = 1e-9;
  const bool is_best = std::abs(value.avg - best) <= kEpsilon;
  const bool is_worst = std::abs(value.avg - worst) <= kEpsilon;
  if (is_best && is_worst) return formatted;
  if (is_best) return "<span style=\"color: green; font-weight: bold;\">" + formatted + "</span>";
  if (is_worst) return "<span style=\"color: red; font-weight: bold;\">" + formatted + "</span>";
  return formatted;
}

static void AppendMetricTable(std::ostringstream& out, const char* title, const char* unit, const std::vector<BenchRow>& rows,
                              const std::vector<MetricColumn>& columns, bool include_avg) {
  out << "## " << title;
  if (unit && *unit) out << " (" << unit << ")";
  out << "\n\n";
  out << "| scenario |";
  for (const auto& column : columns) out << " " << column.name << " |";
  out << "\n";

  out << "|---|";
  for (size_t i = 0; i < columns.size(); ++i) out << "---:|";
  out << "\n";

  std::vector<BenchRow> display_rows = rows;
  if (include_avg) display_rows.push_back(AverageRow(rows));

  for (const auto& row : display_rows) {
    double best = std::numeric_limits<double>::infinity();
    double worst = -std::numeric_limits<double>::infinity();
    for (const auto& column : columns) {
      const double value = column.getter(row).avg;
      best = std::min(best, value);
      worst = std::max(worst, value);
    }

    out << "| " << row.scenario << " |";
    for (const auto& column : columns) out << " " << HighlightValue(column.getter(row), best, worst) << " |";
    out << "\n";
  }

  out << "\n";
}

static void AppendDatasetOverviewTable(std::ostringstream& out, const std::vector<BenchRow>& rows, bool include_avg) {
  out << "## Dataset Overview\n\n";
  out << "| scenario | polygons | points | poly(MB) |\n";
  out << "|---|---:|---:|---:|\n";

  std::vector<BenchRow> display_rows = rows;
  if (include_avg) display_rows.push_back(AverageRow(rows));

  for (const auto& row : display_rows) {
    out << "| " << row.scenario << " | " << FormatValueRange(row.polygons, row.polygons_min, row.polygons_max) << " | "
        << FormatValueRange(row.points, row.points_min, row.points_max) << " | " << FormatValueRange(row.poly_mb, row.poly_mb_min, row.poly_mb_max)
        << " |\n";
  }

  out << "\n";
}

std::string RenderMarkdownTable(const std::vector<BenchRow>& rows, bool include_avg) {
  std::ostringstream out;
  AppendDatasetOverviewTable(out, rows, include_avg);
  const auto size_columns = BuildMetricColumns("size");
  const auto avg_bytes_columns = BuildMetricColumns("avg_bytes");
  const auto encode_columns = BuildMetricColumns("encode");
  const auto decode_columns = BuildMetricColumns("decode");

  AppendMetricTable(out, "Size Comparison", "MB", rows, size_columns, include_avg);
  AppendMetricTable(out, "Average Encoded Length Per Object", "bytes/object", rows, avg_bytes_columns, include_avg);
  AppendMetricTable(out, "Encode Latency Comparison", "ms", rows, encode_columns, include_avg);
  AppendMetricTable(out, "Decode Latency Comparison", "ms", rows, decode_columns, include_avg);
  return out.str();
}

}  // namespace polybench
