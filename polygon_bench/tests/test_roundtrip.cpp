#include <exception>
#include <cmath>
#include <cstddef>
#include <iostream>
#include <numeric>
#include <string>
#include <type_traits>

#include "bench.h"
#include "encoders/boost_bin.h"
#include "encoders/bin_fixed.h"
#include "encoders/bin_offset_varint.h"
#include "encoders/delta_varint.h"
#include "encoders/delta_windowmin_varint.h"
#include "encoders/mini_pbuf.h"
#include "generate.h"

static_assert(std::is_same_v<decltype(polybench::Point{}.x), long long>,
              "Point.x should be long long");
static_assert(std::is_same_v<decltype(polybench::Point{}.y), long long>,
              "Point.y should be long long");

#ifdef POLYBENCH_WITH_BOOST
#include <boost/serialization/is_bitwise_serializable.hpp>
static_assert(boost::serialization::is_bitwise_serializable<polybench::Point>::value,
              "polybench::Point should be marked BOOST_IS_BITWISE_SERIALIZABLE");
#endif

namespace {

int g_failed = 0;

void Assert(bool ok, const std::string& msg) {
  if (!ok) {
    ++g_failed;
    std::cerr << "ASSERT FAIL: " << msg << "\n";
  }
}

template <typename EncoderT>
void RoundTripTest(const polybench::PolygonSet& ps) {
  EncoderT enc;
  const auto bytes = enc.Encode(ps).bytes;
  Assert(!bytes.empty(), enc.Name() + ": encoded bytes should not be empty");
  const auto ps2 = enc.Decode(bytes).polygon_set;
  Assert(ps2 == ps, enc.Name() + ": decode should roundtrip");
}

void LongLongCoordinateRoundTripTest() {
  polybench::PolygonSet ps = {
      {
          {static_cast<long long>(1) << 40, -((static_cast<long long>(1) << 39) + 17)},
          {-((static_cast<long long>(1) << 41) - 33), (static_cast<long long>(1) << 42) + 9},
          {(static_cast<long long>(1) << 45) + 1234, -((static_cast<long long>(1) << 44) + 5678)},
      },
  };

  RoundTripTest<polybench::MiniPbufEncoder>(ps);
  RoundTripTest<polybench::BinFixedEncoder>(ps);
  RoundTripTest<polybench::BinOffsetVarintEncoder>(ps);
  RoundTripTest<polybench::DeltaVarintEncoder>(ps);
  RoundTripTest<polybench::DeltaWindowMinVarintEncoder>(ps);
#ifdef POLYBENCH_WITH_BOOST
  RoundTripTest<polybench::BoostBinEncoder>(ps);
#endif
}

void DefaultRectWindowTest() {
  // 不显式设置宽高时，默认应使用 (1024 * 72) x (2048 * 72) 的矩形窗口。
  polybench::GenerateConfig cfg;
  Assert(cfg.cut_width_um == 73728, "default cut_width_um should be 1024*72");
  Assert(cfg.cut_height_um == 147456, "default cut_height_um should be 2048*72");
  cfg.min_total_points = 40;
  cfg.max_total_points = 40;
  cfg.polygons = 8;
  cfg.points_per_polygon = 5;
  cfg.region_size_um = 100000;
  cfg.seed = 7;

  const auto ps = polybench::GeneratePolygonSet(cfg);
  for (const auto& poly : ps) {
    for (const auto& p : poly) {
      Assert(p.x >= 0 && p.x < 73728, "default window width should be 1024*72 um");
      Assert(p.y >= 0 && p.y < 147456, "default window height should be 2048*72 um");
    }
  }
}

void ExplicitRectWindowTest() {
  // 显式设置矩形窗口宽高后，Far 场景下的点应严格落在最远角窗口范围内。
  polybench::GenerateConfig cfg;
  cfg.scenario = polybench::ScenarioKind::Far;
  cfg.min_total_points = 40;
  cfg.max_total_points = 40;
  cfg.polygons = 8;
  cfg.points_per_polygon = 5;
  cfg.region_size_um = 100000;
  cfg.cut_width_um = 321;
  cfg.cut_height_um = 654;
  cfg.seed = 9;

  const auto ps = polybench::GeneratePolygonSet(cfg);
  const int32_t x0 = cfg.region_size_um - cfg.cut_width_um;
  const int32_t y0 = cfg.region_size_um - cfg.cut_height_um;
  for (const auto& poly : ps) {
    for (const auto& p : poly) {
      Assert(p.x >= x0 && p.x < x0 + cfg.cut_width_um, "explicit width should constrain x range");
      Assert(p.y >= y0 && p.y < y0 + cfg.cut_height_um, "explicit height should constrain y range");
    }
  }
}

void RandomScaleGenerationTest() {
  polybench::GenerateConfig cfg;
  cfg.seed = 21;
  const auto ps = polybench::GeneratePolygonSet(cfg);
  const size_t total_points = polybench::PointCount(ps);
  Assert(total_points >= 250000 && total_points <= 350000, "default total points should be within 25w~35w");
  for (const auto& poly : ps) {
    Assert(poly.size() >= 3, "each polygon should have at least 3 points");
    Assert(poly.size() <= 10000, "each polygon should have at most 10000 points");
  }
}

void RandomizedStructureSeedVariationTest() {
  polybench::GenerateConfig cfg;
  cfg.seed = 11;
  const auto ps1 = polybench::GeneratePolygonSet(cfg);
  cfg.seed = 12;
  const auto ps2 = polybench::GeneratePolygonSet(cfg);

  Assert(polybench::PointCount(ps1) >= 250000 && polybench::PointCount(ps1) <= 350000,
         "seed 11 total points should stay within range");
  Assert(polybench::PointCount(ps2) >= 250000 && polybench::PointCount(ps2) <= 350000,
         "seed 12 total points should stay within range");

  bool different = ps1.size() != ps2.size() || polybench::PointCount(ps1) != polybench::PointCount(ps2);
  if (!different) {
    for (size_t i = 0; i < ps1.size(); ++i) {
      if (ps1[i].size() != ps2[i].size()) {
        different = true;
        break;
      }
    }
  }
  Assert(different, "different seeds should generate different polygon or point distributions");
}

void BenchDefaultsTest() {
  polybench::BenchConfig cfg;
  Assert(cfg.iterations == 100, "default benchmark iterations should be 100");
}

void MaskLikeGenerationClipTest() {
  polybench::GenerateConfig cfg;
  cfg.kind = polybench::GenerateKind::MaskLike;
  cfg.scenario = polybench::ScenarioKind::NearOrigin;
  cfg.min_total_points = 400;
  cfg.max_total_points = 400;
  cfg.region_size_um = 1000;
  cfg.cut_size_um = 50;
  cfg.points_per_polygon = 300;
  cfg.seed = 1234;

  const auto ps = polybench::GeneratePolygonSet(cfg);
  Assert(!ps.empty(), "mask-like generation should produce non-empty polygon set");

  const long long x0 = 0;
  const long long y0 = 0;
  const long long x1 = static_cast<long long>(cfg.cut_size_um) * 1000 - 1;
  const long long y1 = static_cast<long long>(cfg.cut_size_um) * 1000 - 1;

  bool has_boundary = false;
  for (const auto& poly : ps) {
    Assert(poly.size() >= 3, "mask-like clipped polygon should have at least 3 points");
    for (const auto& p : poly) {
      Assert(p.x >= x0 && p.x <= x1, "mask-like point.x should be inside cut window");
      Assert(p.y >= y0 && p.y <= y1, "mask-like point.y should be inside cut window");
      if (p.x == x0 || p.x == x1 || p.y == y0 || p.y == y1) has_boundary = true;
    }
  }
  Assert(has_boundary, "mask-like generation should insert boundary points after clipping");
}

void DeltaWindowMinWireFormatTest() {
  polybench::PolygonSet ps = {
      {
          {10, 20},
          {13, 18},
      },
  };

  polybench::DeltaWindowMinVarintEncoder enc;
  const auto bytes = enc.Encode(ps).bytes;
  const std::vector<uint8_t> expected = {0x14, 0x24, 0x01, 0x02, 0x00, 0x02, 0x06, 0x03};
  Assert(bytes == expected, "delta_windowmin_varint: wire format should be global_min + first_offset + deltas");
  const auto ps2 = enc.Decode(bytes).polygon_set;
  Assert(ps2 == ps, "delta_windowmin_varint: decode should roundtrip for wire-format test");
}

void RenderTableFormatTest() {
  std::vector<polybench::BenchRow> rows;

  polybench::BenchRow near;
  near.scenario = "near_origin";
  near.polygons = 10;
  near.polygons_min = 9;
  near.polygons_max = 11;
  near.points = 60;
  near.points_min = 50;
  near.points_max = 70;
  near.poly_mb = 0.005;
  near.poly_mb_min = 0.004;
  near.poly_mb_max = 0.006;
  near.pbuf_mb = 3.000;
  near.pbuf_mb_min = 2.500;
  near.pbuf_mb_max = 3.500;
  near.bin_fixed_mb = 2.000;
  near.bin_fixed_mb_min = 1.900;
  near.bin_fixed_mb_max = 2.100;
  near.bin_offset_varint_mb = 1.000;
  near.bin_offset_varint_mb_min = 0.900;
  near.bin_offset_varint_mb_max = 1.100;
  near.delta_varint_mb = 4.000;
  near.delta_varint_mb_min = 3.900;
  near.delta_varint_mb_max = 4.100;
  near.delta_windowmin_varint_mb = 1.250;
  near.delta_windowmin_varint_mb_min = 1.200;
  near.delta_windowmin_varint_mb_max = 1.300;
  near.pbuf_enc_ms = 6.000;
  near.pbuf_enc_ms_min = 5.000;
  near.pbuf_enc_ms_max = 7.000;
  near.bin_fixed_enc_ms = 2.500;
  near.bin_fixed_enc_ms_min = 2.000;
  near.bin_fixed_enc_ms_max = 3.000;
  near.bin_offset_varint_enc_ms = 1.500;
  near.bin_offset_varint_enc_ms_min = 1.200;
  near.bin_offset_varint_enc_ms_max = 1.800;
  near.delta_varint_enc_ms = 8.000;
  near.delta_varint_enc_ms_min = 7.000;
  near.delta_varint_enc_ms_max = 9.000;
  near.delta_windowmin_varint_enc_ms = 1.750;
  near.delta_windowmin_varint_enc_ms_min = 1.500;
  near.delta_windowmin_varint_enc_ms_max = 2.000;
  near.pbuf_dec_ms = 5.500;
  near.pbuf_dec_ms_min = 5.000;
  near.pbuf_dec_ms_max = 6.000;
  near.bin_fixed_dec_ms = 1.500;
  near.bin_fixed_dec_ms_min = 1.200;
  near.bin_fixed_dec_ms_max = 1.800;
  near.bin_offset_varint_dec_ms = 3.000;
  near.bin_offset_varint_dec_ms_min = 2.500;
  near.bin_offset_varint_dec_ms_max = 3.500;
  near.delta_varint_dec_ms = 9.000;
  near.delta_varint_dec_ms_min = 8.000;
  near.delta_varint_dec_ms_max = 10.000;
  near.delta_windowmin_varint_dec_ms = 2.250;
  near.delta_windowmin_varint_dec_ms_min = 2.000;
  near.delta_windowmin_varint_dec_ms_max = 2.500;

  near.pbuf_bpp = 314572.800;
  near.pbuf_bpp_min = 300000.000;
  near.pbuf_bpp_max = 330000.000;
#ifdef POLYBENCH_WITH_BOOST
  near.boost_bin_mb = 2.500;
  near.boost_bin_enc_ms = 3.500;
  near.boost_bin_dec_ms = 4.500;
#endif
  rows.push_back(near);

  polybench::BenchRow far = near;
  far.scenario = "far";
  far.polygons = 20;
  far.points = 120;
  far.poly_mb = 0.010;
  far.pbuf_mb = 1.500;
  far.bin_fixed_mb = 1.000;
  far.bin_offset_varint_mb = 0.750;
  far.delta_varint_mb = 2.000;
  far.delta_windowmin_varint_mb = 0.875;
  far.pbuf_enc_ms = 4.000;
  far.bin_fixed_enc_ms = 2.000;
  far.bin_offset_varint_enc_ms = 5.000;
  far.delta_varint_enc_ms = 7.000;
  far.delta_windowmin_varint_enc_ms = 4.500;
  far.pbuf_dec_ms = 6.000;
  far.bin_fixed_dec_ms = 2.000;
  far.bin_offset_varint_dec_ms = 1.000;
  far.delta_varint_dec_ms = 8.000;
  far.delta_windowmin_varint_dec_ms = 1.500;
#ifdef POLYBENCH_WITH_BOOST
  far.boost_bin_mb = 1.250;
  far.boost_bin_enc_ms = 3.000;
  far.boost_bin_dec_ms = 4.000;
#endif
  rows.push_back(far);

  const std::string report = polybench::RenderMarkdownTable(rows, true);
  Assert(report.find("## Dataset Overview") != std::string::npos, "report should contain dataset overview table");
  Assert(report.find("| scenario | polygons | points | poly(MB) |") != std::string::npos,
         "overview table should contain polygons/points/poly(MB)");
  Assert(report.find("## Size Comparison") != std::string::npos, "report should contain size table title");
  Assert(report.find("## Average Encoded Length Per Object") != std::string::npos,
         "report should contain average encoded length table");
  Assert(report.find("## Encode Latency Comparison") != std::string::npos,
         "report should contain encode latency table title");
  Assert(report.find("## Decode Latency Comparison") != std::string::npos,
         "report should contain decode latency table title");
  Assert(report.find("| scenario | pbuf |") != std::string::npos, "table header should be encoder-oriented");
  Assert(report.find("delta_windowmin_varint") != std::string::npos, "report should contain delta_windowmin_varint column");
  Assert(report.find("3.000 (2.500~3.500)") != std::string::npos, "report should render avg/min/max ranges");
  Assert(report.find("style=\"color: green; font-weight: bold;\"") != std::string::npos,
         "best values should be highlighted in green");
  Assert(report.find("style=\"color: red; font-weight: bold;\"") != std::string::npos,
         "worst values should be highlighted in red");
  Assert(report.find("314572.800") != std::string::npos,
         "average bytes per object should be rendered from size and polygon count");
  Assert(report.find("rss(MB)") == std::string::npos, "report should focus on size/encode/decode comparison tables");
}

}  // namespace

int main() {
  try {
    polybench::GenerateConfig cfg;
    cfg.polygons = 3;
    cfg.points_per_polygon = 4;
    cfg.min_total_points = 12;
    cfg.max_total_points = 12;
    // 这里保留对旧参数 cut_size_um 的测试，确保正方形窗口兼容逻辑不回归。
    cfg.cut_size_um = 10;
    cfg.region_size_um = 100000;
    cfg.seed = 123;

    for (auto scenario : {polybench::ScenarioKind::NearOrigin, polybench::ScenarioKind::Middle, polybench::ScenarioKind::Far}) {
      cfg.scenario = scenario;
      const auto ps = polybench::GeneratePolygonSet(cfg);

      RoundTripTest<polybench::MiniPbufEncoder>(ps);
      RoundTripTest<polybench::BinFixedEncoder>(ps);
      RoundTripTest<polybench::BinOffsetVarintEncoder>(ps);
      RoundTripTest<polybench::DeltaVarintEncoder>(ps);
      RoundTripTest<polybench::DeltaWindowMinVarintEncoder>(ps);
#ifdef POLYBENCH_WITH_BOOST
      RoundTripTest<polybench::BoostBinEncoder>(ps);
#endif
    }

    DefaultRectWindowTest();
    ExplicitRectWindowTest();
    LongLongCoordinateRoundTripTest();
    RandomScaleGenerationTest();
    RandomizedStructureSeedVariationTest();
    BenchDefaultsTest();
    MaskLikeGenerationClipTest();
    DeltaWindowMinWireFormatTest();
    RenderTableFormatTest();

    if (g_failed == 0) {
      std::cout << "ALL TESTS PASSED\n";
      return 0;
    }
    std::cerr << g_failed << " TEST(S) FAILED\n";
    return 1;
  } catch (const std::exception& e) {
    std::cerr << "TEST ERROR: " << e.what() << "\n";
    return 2;
  }
}
