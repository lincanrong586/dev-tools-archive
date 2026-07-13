#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <string>
#include <vector>

#include "../codec.h"
#include "../codec_metrics.h"
#include "generate.h"
#include "report.h"

namespace polygon_codec::perftest {

namespace {

constexpr uint32_t kBenchmarkWarmupIteration = 0;
constexpr uint32_t kDefaultIterations = 100;
constexpr double kBytesPerMiB = 1024.0 * 1024.0;

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

static void Add(StatD& s, double v) {
  s.sum += v;
  s.min = std::min(s.min, v);
  s.max = std::max(s.max, v);
}

static void Add(StatU32& s, uint32_t v) {
  s.sum += v;
  s.min = std::min(s.min, v);
  s.max = std::max(s.max, v);
}

static Stat3 ToStat3(const StatD& s, uint32_t n) {
  Stat3 r;
  if (n == 0) return r;
  r.avg = s.sum / static_cast<double>(n);
  r.min = std::isfinite(s.min) ? s.min : 0.0;
  r.max = std::isfinite(s.max) ? s.max : 0.0;
  return r;
}

static U32Stat3 ToU32Stat3(const StatU32& s, uint32_t n) {
  U32Stat3 r;
  if (n == 0) return r;
  r.avg = static_cast<uint32_t>(std::llround(static_cast<double>(s.sum) / static_cast<double>(n)));
  r.min = s.min == std::numeric_limits<uint32_t>::max() ? 0 : s.min;
  r.max = s.max;
  return r;
}

static double ToMB(size_t bytes) { return static_cast<double>(bytes) / kBytesPerMiB; }

static std::vector<Algorithm> AlgoList() {
  std::vector<Algorithm> algos = {
      Algorithm::kPbuf,
      Algorithm::kBinFixed,
      Algorithm::kBinOffsetVarint,
      Algorithm::kDeltaVarint,
      Algorithm::kDeltaWindowMinVarint,
  };
#ifdef POLYGON_CODEC_WITH_BOOST
  algos.push_back(Algorithm::kBoostBin);
#endif
  return algos;
}

static bool ParseU32(const std::string& s, uint32_t& out) {
  char* end = nullptr;
  const unsigned long v = std::strtoul(s.c_str(), &end, 10);
  if (!end || *end != '\0') return false;
  out = static_cast<uint32_t>(v);
  return true;
}

static bool ParseI32(const std::string& s, int32_t& out) {
  char* end = nullptr;
  const long v = std::strtol(s.c_str(), &end, 10);
  if (!end || *end != '\0') return false;
  out = static_cast<int32_t>(v);
  return true;
}

static std::string CoordinateScaleName(CoordinateScale scale) {
  switch (scale) {
    case CoordinateScale::kNm0p1:
      return "0.1nm";
    case CoordinateScale::kNm1:
      return "1nm";
    case CoordinateScale::kNm10:
      return "10nm";
    case CoordinateScale::kNm100:
      return "100nm";
    case CoordinateScale::kUm1:
      return "1um";
  }
  return "unknown";
}

static bool ParseCoordinateScale(const std::string& s, CoordinateScale& out) {
  if (s == "0.1nm") {
    out = CoordinateScale::kNm0p1;
    return true;
  }
  if (s == "1nm") {
    out = CoordinateScale::kNm1;
    return true;
  }
  if (s == "10nm") {
    out = CoordinateScale::kNm10;
    return true;
  }
  if (s == "100nm") {
    out = CoordinateScale::kNm100;
    return true;
  }
  if (s == "1um") {
    out = CoordinateScale::kUm1;
    return true;
  }
  return false;
}

static void PrintUsage() {
  std::cout << "polygon_codec_bench usage:\n"
            << "  ./build/polygon_codec_bench --iterations N [options]\n\n"
            << "options:\n"
            << "  --iterations N           每组数据生成/测试轮数（默认 " << kDefaultIterations << "）\n"
            << "  --seed N                 基础随机种子（默认 " << kDefaultSeed << "，内部使用 seed+i 生成多轮）\n"
            << "  --min-total-points N     每轮总点数下限（默认 " << kDefaultMinTotalPoints << "）\n"
            << "  --max-total-points N     每轮总点数上限（默认 " << kDefaultMaxTotalPoints << "）\n"
            << "  --polygons N             期望 polygon 数中心值（默认 " << kDefaultPolygons << "）\n"
            << "  --points-per-polygon N   期望平均点数（默认 " << kDefaultPointsPerPolygon
            << "；mask_like 会自动抬高）\n"
            << "  --scale S                point(x,y) 的取值单位（默认 0.1nm；可选：0.1nm/1nm/10nm/100nm/1um）\n"
            << "  --region-size-um N       大区域边长（um，默认 " << kDefaultRegionSizeUm << "）\n"
            << "  --cut-size-um N          旧式正方形窗口（um，>0 时覆盖宽高）\n"
            << "  --cut-width-um N         矩形窗口宽（um）\n"
            << "  --cut-height-um N        矩形窗口高（um）\n";
}

static ReportRow RunScenario(GenerateConfig gen_cfg, uint32_t iterations, const std::vector<Algorithm>& algos) {
  ReportRow row;
  row.scenario = ScenarioName(gen_cfg.scenario);
  row.algos = algos;
  row.size_mb.resize(algos.size());
  row.bytes_per_polygon.resize(algos.size());
  row.enc_ms.resize(algos.size());
  row.dec_ms.resize(algos.size());

  StatU32 polygons_s;
  StatU32 points_s;
  StatD poly_mb_s;

  std::vector<StatD> size_mb_s(algos.size());
  std::vector<StatD> bpp_s(algos.size());
  std::vector<StatD> enc_s(algos.size());
  std::vector<StatD> dec_s(algos.size());

  Options opt;
  opt.enable_metrics = false;
  opt.print_metrics = false;
  opt.coordinate_scale = gen_cfg.coordinate_scale;

  for (uint32_t i = 0; i < iterations; ++i) {
    gen_cfg.seed = gen_cfg.seed + i;
    const PolygonSet ps = GeneratePolygonSet(gen_cfg);
    const uint32_t polygons = static_cast<uint32_t>(ps.size());
    uint32_t points = 0;
    for (const auto& poly : ps) points += static_cast<uint32_t>(poly.size());

    Add(polygons_s, polygons);
    Add(points_s, points);
    Add(poly_mb_s, ToMB(EstimateRawBytes(ps)));

    for (size_t ai = 0; ai < algos.size(); ++ai) {
      const Algorithm algo = algos[ai];

      const auto t0 = std::chrono::steady_clock::now();
      const auto enc_or = Codec::Encode(ps, algo, opt);
      const auto t1 = std::chrono::steady_clock::now();
      if (!enc_or.ok()) {
        throw std::runtime_error("encode failed: " + enc_or.status().message());
      }

      const auto& bytes = enc_or.value().bytes;
      Add(size_mb_s[ai], ToMB(bytes.size()));
      Add(bpp_s[ai], polygons == 0 ? 0.0 : static_cast<double>(bytes.size()) / static_cast<double>(polygons));
      Add(enc_s[ai], std::chrono::duration<double, std::milli>(t1 - t0).count());

      const auto t2 = std::chrono::steady_clock::now();
      const auto dec_or = Codec::Decode(bytes, opt);
      const auto t3 = std::chrono::steady_clock::now();
      if (!dec_or.ok()) {
        throw std::runtime_error("decode failed: " + dec_or.status().message());
      }
      Add(dec_s[ai], std::chrono::duration<double, std::milli>(t3 - t2).count());

      if (i == kBenchmarkWarmupIteration) {
        if (!(dec_or.value().polygon_set == ps)) {
          throw std::runtime_error("roundtrip mismatch on first iteration");
        }
      }
    }
  }

  row.polygons = ToU32Stat3(polygons_s, iterations);
  row.points = ToU32Stat3(points_s, iterations);
  row.poly_mb = ToStat3(poly_mb_s, iterations);
  for (size_t ai = 0; ai < algos.size(); ++ai) {
    row.size_mb[ai] = ToStat3(size_mb_s[ai], iterations);
    row.bytes_per_polygon[ai] = ToStat3(bpp_s[ai], iterations);
    row.enc_ms[ai] = ToStat3(enc_s[ai], iterations);
    row.dec_ms[ai] = ToStat3(dec_s[ai], iterations);
  }

  return row;
}

static std::vector<ReportRow> RunAllScenarios(GenerateConfig cfg, uint32_t iterations, const std::vector<Algorithm>& algos) {
  std::vector<ReportRow> rows;
  for (auto s : {ScenarioKind::NearOrigin, ScenarioKind::Middle, ScenarioKind::Far}) {
    cfg.scenario = s;
    rows.push_back(RunScenario(cfg, iterations, algos));
  }
  return rows;
}

}  // namespace

}  // namespace polygon_codec::perftest

int main(int argc, char** argv) {
  using namespace polygon_codec::perftest;

  GenerateConfig cfg;
  uint32_t iterations = kDefaultIterations;

  for (int i = 1; i < argc; ++i) {
    const std::string a = argv[i];
    auto need = [&](const char* name) -> std::string {
      if (i + 1 >= argc) throw std::runtime_error(std::string("missing value for ") + name);
      return argv[++i];
    };

    if (a == "--help" || a == "-h") {
      PrintUsage();
      return 0;
    } else if (a == "--iterations") {
      ParseU32(need("--iterations"), iterations);
    } else if (a == "--seed") {
      ParseU32(need("--seed"), cfg.seed);
    } else if (a == "--min-total-points") {
      ParseU32(need("--min-total-points"), cfg.min_total_points);
    } else if (a == "--max-total-points") {
      ParseU32(need("--max-total-points"), cfg.max_total_points);
    } else if (a == "--polygons") {
      ParseU32(need("--polygons"), cfg.polygons);
    } else if (a == "--points-per-polygon") {
      ParseU32(need("--points-per-polygon"), cfg.points_per_polygon);
    } else if (a == "--region-size-um") {
      ParseI32(need("--region-size-um"), cfg.region_size_um);
    } else if (a == "--cut-size-um") {
      ParseI32(need("--cut-size-um"), cfg.cut_size_um);
    } else if (a == "--cut-width-um") {
      ParseI32(need("--cut-width-um"), cfg.cut_width_um);
    } else if (a == "--cut-height-um") {
      ParseI32(need("--cut-height-um"), cfg.cut_height_um);
    } else if (a == "--scale") {
      if (!ParseCoordinateScale(need("--scale"), cfg.coordinate_scale)) {
        throw std::runtime_error("invalid --scale value");
      }
    } else {
      throw std::runtime_error("unknown arg: " + a);
    }
  }

  const auto algos = AlgoList();

  GenerateConfig random_cfg = cfg;
  random_cfg.kind = GenerateKind::RandomPoints;
  std::cout << RenderMarkdown("random_points (scale=" + CoordinateScaleName(cfg.coordinate_scale) + ")",
                              RunAllScenarios(random_cfg, iterations, algos), true);

  GenerateConfig mask_cfg = cfg;
  mask_cfg.kind = GenerateKind::MaskLike;
  std::cout << RenderMarkdown("mask_like (scale=" + CoordinateScaleName(cfg.coordinate_scale) + ")",
                              RunAllScenarios(mask_cfg, iterations, algos), true);

  return 0;
}
