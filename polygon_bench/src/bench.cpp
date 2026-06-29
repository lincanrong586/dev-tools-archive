#include "bench.h"

#include <chrono>
#include <iomanip>
#include <numeric>
#include <sstream>

#include "encoders/boost_bin.h"
#include "encoders/bin_fixed.h"
#include "encoders/bin_offset_varint.h"
#include "encoders/delta_varint.h"
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

struct MeasureResult {
  double enc_ms = 0.0;
  double dec_ms = 0.0;
  size_t bytes = 0;
};

static MeasureResult Measure(const Encoder& enc, const PolygonSet& ps, const BenchConfig& cfg) {
  using Clock = std::chrono::steady_clock;

  auto do_encode = [&]() { return enc.Encode(ps).bytes; };
  auto do_decode = [&](const std::vector<uint8_t>& bytes) { return enc.Decode(bytes).polygon_set; };

  if (cfg.warmup) {
    auto bytes = do_encode();
    (void)do_decode(bytes);
  }

  std::vector<double> enc_ms;
  std::vector<double> dec_ms;
  enc_ms.reserve(cfg.iterations);
  dec_ms.reserve(cfg.iterations);

  std::vector<uint8_t> last_bytes;
  for (uint32_t i = 0; i < cfg.iterations; ++i) {
    auto t0 = Clock::now();
    auto bytes = do_encode();
    auto t1 = Clock::now();
    auto ps2 = do_decode(bytes);
    auto t2 = Clock::now();

    if (ps2 != ps) {
      throw std::runtime_error("decode result mismatch for encoder: " + enc.Name());
    }

    last_bytes = std::move(bytes);
    enc_ms.push_back(std::chrono::duration<double, std::milli>(t1 - t0).count());
    dec_ms.push_back(std::chrono::duration<double, std::milli>(t2 - t1).count());
  }

  auto avg = [](const std::vector<double>& v) {
    if (v.empty()) return 0.0;
    return std::accumulate(v.begin(), v.end(), 0.0) / static_cast<double>(v.size());
  };

  MeasureResult r;
  r.enc_ms = avg(enc_ms);
  r.dec_ms = avg(dec_ms);
  r.bytes = last_bytes.size();
  return r;
}

BenchRow RunScenarioBench(const GenerateConfig& gen_cfg, const BenchConfig& bench_cfg) {
  auto ps = GeneratePolygonSet(gen_cfg);

  BenchRow row;
  row.scenario = ScenarioName(gen_cfg.scenario);
  row.polygons = static_cast<uint32_t>(ps.size());
  row.points = static_cast<uint32_t>(PointCount(ps));
  row.poly_mb = PolyDataMB(ps);

  MiniPbufEncoder pbuf;
  BinFixedEncoder bin_fixed;
  BinOffsetVarintEncoder bin_offset_varint;
  DeltaVarintEncoder delta_varint;
#ifdef POLYBENCH_WITH_BOOST
  BoostBinEncoder boost_bin;
#endif

  {
    const auto r = Measure(pbuf, ps, bench_cfg);
    row.pbuf_mb = ToMB(r.bytes);
    row.pbuf_enc_ms = r.enc_ms;
    row.pbuf_dec_ms = r.dec_ms;
  }

  {
    const auto r = Measure(bin_fixed, ps, bench_cfg);
    row.bin_fixed_mb = ToMB(r.bytes);
    row.bin_fixed_enc_ms = r.enc_ms;
    row.bin_fixed_dec_ms = r.dec_ms;
  }

  {
    const auto r = Measure(bin_offset_varint, ps, bench_cfg);
    row.bin_offset_varint_mb = ToMB(r.bytes);
    row.bin_offset_varint_enc_ms = r.enc_ms;
    row.bin_offset_varint_dec_ms = r.dec_ms;
  }

  {
    const auto r = Measure(delta_varint, ps, bench_cfg);
    row.delta_varint_mb = ToMB(r.bytes);
    row.delta_varint_enc_ms = r.enc_ms;
    row.delta_varint_dec_ms = r.dec_ms;
  }

#ifdef POLYBENCH_WITH_BOOST
  {
    const auto r = Measure(boost_bin, ps, bench_cfg);
    row.boost_bin_mb = ToMB(r.bytes);
    row.boost_bin_enc_ms = r.enc_ms;
    row.boost_bin_dec_ms = r.dec_ms;
  }
#endif

  row.rss_mb = CurrentRSSMB();
  return row;
}

static void AppendHeader(std::ostringstream& out) {
  out << "| scenario | polygons | points | poly(MB) | pbuf(MB) | pbuf_enc(ms) | pbuf_dec(ms) | bin_fixed(MB) | bin_fixed_enc(ms) | bin_fixed_dec(ms) | bin_offset_varint(MB) | bin_offset_varint_enc(ms) | bin_offset_varint_dec(ms) | delta_varint(MB) | delta_varint_enc(ms) | delta_varint_dec(ms) |";
#ifdef POLYBENCH_WITH_BOOST
  out << " boost_bin(MB) | boost_bin_enc(ms) | boost_bin_dec(ms) |";
#endif
  out << " rss(MB) |\n";

  out << "|---|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|---:|";
#ifdef POLYBENCH_WITH_BOOST
  out << "---:|---:|---:|";
#endif
  out << "---:|\n";
}

static void AppendRow(std::ostringstream& out, const BenchRow& r) {
  auto f3 = [](double v) {
    std::ostringstream s;
    s << std::fixed << std::setprecision(3) << v;
    return s.str();
  };
  out << "| " << r.scenario << " | " << r.polygons << " | " << r.points << " | " << f3(r.poly_mb) << " | " << f3(r.pbuf_mb)
      << " | " << f3(r.pbuf_enc_ms) << " | " << f3(r.pbuf_dec_ms) << " | " << f3(r.bin_fixed_mb) << " | "
      << f3(r.bin_fixed_enc_ms) << " | " << f3(r.bin_fixed_dec_ms) << " | " << f3(r.bin_offset_varint_mb) << " | "
      << f3(r.bin_offset_varint_enc_ms) << " | " << f3(r.bin_offset_varint_dec_ms) << " | " << f3(r.delta_varint_mb) << " | "
      << f3(r.delta_varint_enc_ms) << " | " << f3(r.delta_varint_dec_ms) << " |";
#ifdef POLYBENCH_WITH_BOOST
  out << " " << f3(r.boost_bin_mb) << " | " << f3(r.boost_bin_enc_ms) << " | " << f3(r.boost_bin_dec_ms) << " |";
#endif
  out << " " << f3(r.rss_mb) << " |\n";
}

static BenchRow AverageRow(const std::vector<BenchRow>& rows) {
  BenchRow avg;
  if (rows.empty()) return avg;
  avg.scenario = "avg";
  avg.polygons = rows.front().polygons;
  avg.points = rows.front().points;

  auto avg1 = [&](auto getter) {
    double sum = 0.0;
    for (const auto& r : rows) sum += getter(r);
    return sum / static_cast<double>(rows.size());
  };

  avg.poly_mb = avg1([](const BenchRow& r) { return r.poly_mb; });
  avg.pbuf_mb = avg1([](const BenchRow& r) { return r.pbuf_mb; });
  avg.pbuf_enc_ms = avg1([](const BenchRow& r) { return r.pbuf_enc_ms; });
  avg.pbuf_dec_ms = avg1([](const BenchRow& r) { return r.pbuf_dec_ms; });
  avg.bin_fixed_mb = avg1([](const BenchRow& r) { return r.bin_fixed_mb; });
  avg.bin_fixed_enc_ms = avg1([](const BenchRow& r) { return r.bin_fixed_enc_ms; });
  avg.bin_fixed_dec_ms = avg1([](const BenchRow& r) { return r.bin_fixed_dec_ms; });
  avg.bin_offset_varint_mb = avg1([](const BenchRow& r) { return r.bin_offset_varint_mb; });
  avg.bin_offset_varint_enc_ms = avg1([](const BenchRow& r) { return r.bin_offset_varint_enc_ms; });
  avg.bin_offset_varint_dec_ms = avg1([](const BenchRow& r) { return r.bin_offset_varint_dec_ms; });
  avg.delta_varint_mb = avg1([](const BenchRow& r) { return r.delta_varint_mb; });
  avg.delta_varint_enc_ms = avg1([](const BenchRow& r) { return r.delta_varint_enc_ms; });
  avg.delta_varint_dec_ms = avg1([](const BenchRow& r) { return r.delta_varint_dec_ms; });
#ifdef POLYBENCH_WITH_BOOST
  avg.boost_bin_mb = avg1([](const BenchRow& r) { return r.boost_bin_mb; });
  avg.boost_bin_enc_ms = avg1([](const BenchRow& r) { return r.boost_bin_enc_ms; });
  avg.boost_bin_dec_ms = avg1([](const BenchRow& r) { return r.boost_bin_dec_ms; });
#endif
  avg.rss_mb = avg1([](const BenchRow& r) { return r.rss_mb; });
  return avg;
}

std::string RenderMarkdownTable(const std::vector<BenchRow>& rows, bool include_avg) {
  std::ostringstream out;
  AppendHeader(out);
  for (const auto& r : rows) AppendRow(out, r);
  if (include_avg) AppendRow(out, AverageRow(rows));
  return out.str();
}

}  // namespace polybench
