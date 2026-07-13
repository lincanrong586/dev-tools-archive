#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "../codec.h"
#include "../codec_frame.h"
#include "../algorithms/bin_offset_varint.h"
#include "../varint.h"

namespace {

int g_failed = 0;

void Assert(bool ok, const std::string& msg) {
  if (!ok) {
    ++g_failed;
    std::cerr << "ASSERT FAIL: " << msg << "\n";
  }
}

polygon_codec::PolygonSet BuildSamplePolygonSet() {
  using polygon_codec::Point;
  using polygon_codec::Polygon;
  using polygon_codec::PolygonSet;

  PolygonSet ps;
  Polygon p1;
  p1.push_back(Point{0, 0});
  p1.push_back(Point{10, 0});
  p1.push_back(Point{10, 5});
  p1.push_back(Point{0, 5});
  ps.push_back(p1);

  Polygon p2;
  p2.push_back(Point{(static_cast<long long>(1) << 40) + 7, -((static_cast<long long>(1) << 39) + 3)});
  p2.push_back(Point{(static_cast<long long>(1) << 40) + 9, -((static_cast<long long>(1) << 39) + 8)});
  p2.push_back(Point{(static_cast<long long>(1) << 40) + 13, -((static_cast<long long>(1) << 39) + 9)});
  ps.push_back(p2);

  return ps;
}

polygon_codec::PolygonSet BuildEdgeCasePolygonSet() {
  using polygon_codec::Point;
  using polygon_codec::Polygon;
  using polygon_codec::PolygonSet;

  PolygonSet ps;
  Polygon empty_poly;
  ps.push_back(empty_poly);

  Polygon single;
  single.push_back(Point{123, -456});
  ps.push_back(single);

  Polygon two_points;
  two_points.push_back(Point{0, 0});
  two_points.push_back(Point{1, 1});
  ps.push_back(two_points);

  return ps;
}

void RoundTripAllAlgorithmsTest() {
  using namespace polygon_codec;
  const PolygonSet ps = BuildSamplePolygonSet();

  for (uint8_t a = static_cast<uint8_t>(Algorithm::kBinFixed);
       a < static_cast<uint8_t>(Algorithm::kButt); ++a) {
    const Algorithm algo = static_cast<Algorithm>(a);

#ifndef POLYGON_CODEC_WITH_BOOST
    if (algo == Algorithm::kBoostBin) continue;
#endif

    const auto enc_or = Codec::Encode(ps, algo);
    Assert(enc_or.ok(), "encode should succeed");
    if (!enc_or.ok()) continue;

    const auto dec_or = Codec::Decode(enc_or.value().bytes);
    Assert(dec_or.ok(), "decode should succeed");
    if (!dec_or.ok()) continue;

    Assert(dec_or.value().algorithm == algo, "decode should detect algorithm from frame");
    Assert(dec_or.value().polygon_set == ps, "roundtrip should match");
  }
}

void BadMagicShouldFailTest() {
  using namespace polygon_codec;
  const PolygonSet ps = BuildSamplePolygonSet();
  const auto enc_or = Codec::Encode(ps, Algorithm::kBinFixed);
  Assert(enc_or.ok(), "encode should succeed");
  if (!enc_or.ok()) return;
  auto bytes = enc_or.value().bytes;
  bytes[0] ^= 0xFF;
  const auto dec_or = Codec::Decode(bytes);
  Assert(!dec_or.ok(), "decode should fail on bad magic");
}

void MetricsSwitchTest() {
  using namespace polygon_codec;
  Options opt;
  opt.enable_metrics = true;
  opt.print_metrics = false;

  const PolygonSet ps = BuildSamplePolygonSet();
  const auto enc_or = Codec::Encode(ps, Algorithm::kBinOffsetVarint, opt);
  Assert(enc_or.ok(), "encode should succeed");
  if (!enc_or.ok()) return;
  Assert(enc_or.value().metrics.enabled, "metrics should be enabled");
  Assert(enc_or.value().metrics.encoded_bytes == enc_or.value().bytes.size(), "encoded_bytes should match");

  const auto dec_or = Codec::Decode(enc_or.value().bytes, opt);
  Assert(dec_or.ok(), "decode should succeed");
  if (!dec_or.ok()) return;
  Assert(dec_or.value().metrics.enabled, "decode metrics should be enabled");
  Assert(dec_or.value().metrics.encoded_bytes == enc_or.value().bytes.size(), "decode encoded_bytes should match");
}

void PayloadViewShouldDecodeWithoutCopyTest() {
  using namespace polygon_codec;
  const PolygonSet ps = BuildSamplePolygonSet();
  const auto enc_or = Codec::Encode(ps, Algorithm::kBinOffsetVarint);
  Assert(enc_or.ok(), "encode should succeed");
  if (!enc_or.ok()) return;

  const auto view_or = ExtractPayloadView(enc_or.value().bytes);
  Assert(view_or.ok(), "payload view should be extracted");
  if (!view_or.ok()) return;

  const auto view = view_or.value();
  Assert(view.data == enc_or.value().bytes.data() + 8, "payload view should point to original frame bytes");
  Assert(view.size + 8 == enc_or.value().bytes.size(), "payload view size should exclude frame header");

  BinOffsetVarintCodec codec;
  const PolygonSet decoded = codec.DecodePayload(view);
  Assert(decoded == ps, "payload view decode should match source polygons");
}

void AlgorithmEnumShapeShouldNotContainExperimentalAlgoTest() {
  using namespace polygon_codec;
  Assert(static_cast<uint8_t>(Algorithm::kButt) == 7, "kButt should remain 7 after removing experimental algorithm");
}

void CoordinateScaleShouldRoundTripThroughFrameHeaderTest() {
  using namespace polygon_codec;
  const PolygonSet ps = BuildSamplePolygonSet();

  Options opt;
  opt.enable_metrics = false;
  opt.print_metrics = false;
  opt.coordinate_scale = CoordinateScale::kNm10;

  const auto enc_or = Codec::Encode(ps, Algorithm::kDeltaVarint, opt);
  Assert(enc_or.ok(), "encode should succeed");
  if (!enc_or.ok()) return;

  const auto header_or = ParseHeader(enc_or.value().bytes);
  Assert(header_or.ok(), "parse header should succeed");
  if (!header_or.ok()) return;

  Assert(header_or.value().coordinate_scale == CoordinateScale::kNm10, "frame header should contain coordinate scale");

  const auto dec_or = Codec::Decode(enc_or.value().bytes, opt);
  Assert(dec_or.ok(), "decode should succeed");
  if (!dec_or.ok()) return;

  Assert(dec_or.value().coordinate_scale == CoordinateScale::kNm10, "decode output should contain coordinate scale");
  Assert(dec_or.value().polygon_set == ps, "roundtrip should match");
}

void V1FrameShouldDefaultCoordinateScaleToNm1Test() {
  using namespace polygon_codec;
  const PolygonSet ps = BuildSamplePolygonSet();

  Options opt;
  opt.enable_metrics = false;
  opt.print_metrics = false;
  opt.coordinate_scale = CoordinateScale::kNm0p1;

  const auto enc_or = Codec::Encode(ps, Algorithm::kBinOffsetVarint, opt);
  Assert(enc_or.ok(), "encode should succeed");
  if (!enc_or.ok()) return;

  std::vector<uint8_t> bytes = enc_or.value().bytes;
  bytes[4] = 1;

  const auto header_or = ParseHeader(bytes);
  Assert(header_or.ok(), "parse v1 header should succeed");
  if (!header_or.ok()) return;
  Assert(header_or.value().coordinate_scale == CoordinateScale::kNm1, "v1 frame should default scale to 1nm");

  const auto dec_or = Codec::Decode(bytes, opt);
  Assert(dec_or.ok(), "decode v1 bytes should succeed");
  if (!dec_or.ok()) return;
  Assert(dec_or.value().coordinate_scale == CoordinateScale::kNm1, "decode output should default scale for v1 bytes");
  Assert(dec_or.value().polygon_set == ps, "v1 bytes roundtrip should match");
}

void TruncatedDataShouldFailDecodeTest() {
  using namespace polygon_codec;
  const PolygonSet ps = BuildSamplePolygonSet();
  for (uint8_t a = static_cast<uint8_t>(Algorithm::kBinFixed);
       a < static_cast<uint8_t>(Algorithm::kButt); ++a) {
    const Algorithm algo = static_cast<Algorithm>(a);
#ifndef POLYGON_CODEC_WITH_BOOST
    if (algo == Algorithm::kBoostBin) continue;
#endif
    const auto enc_or = Codec::Encode(ps, algo);
    Assert(enc_or.ok(), "encode should succeed for truncation test");
    if (!enc_or.ok()) continue;
    const auto& bytes = enc_or.value().bytes;
    if (bytes.size() <= 8) continue;

    for (size_t cut = 1; cut <= 8 && cut < bytes.size(); ++cut) {
      std::vector<uint8_t> truncated(bytes.begin(), bytes.end() - cut);
      const auto dec_or = Codec::Decode(truncated);
      Assert(!dec_or.ok(), "decode should fail on truncated bytes");
    }
  }
}

void VarintBoundaryAndInvalidDataTest() {
  using namespace polygon_codec;
  auto check_u64 = [&](uint64_t v) {
    std::vector<uint8_t> b;
    AppendUVarint64(v, b);
    const uint8_t* p = b.data();
    const uint8_t* end = b.data() + b.size();
    const uint64_t got = ReadUVarint64(p, end);
    Assert(got == v, "uvarint64 roundtrip should match");
    Assert(p == end, "uvarint64 should consume all bytes");
  };

  for (uint64_t v : {0ULL, 1ULL, 127ULL, 128ULL, 16383ULL, 16384ULL, (1ULL << 32) - 1ULL, (1ULL << 63) - 1ULL}) {
    check_u64(v);
  }

  {
    std::vector<uint8_t> bad(10, 0x80U);
    const uint8_t* p = bad.data();
    const uint8_t* end = bad.data() + bad.size();
    bool threw = false;
    try {
      (void)ReadUVarint64(p, end);
    } catch (...) {
      threw = true;
    }
    Assert(threw, "invalid uvarint64 should throw");
  }
}

void EmptyAndTinyObjectsRoundTripTest() {
  using namespace polygon_codec;

  const PolygonSet empty_ps;
  const PolygonSet tiny_ps = BuildEdgeCasePolygonSet();

  for (uint8_t a = static_cast<uint8_t>(Algorithm::kBinFixed);
       a < static_cast<uint8_t>(Algorithm::kButt); ++a) {
    const Algorithm algo = static_cast<Algorithm>(a);
#ifndef POLYGON_CODEC_WITH_BOOST
    if (algo == Algorithm::kBoostBin) continue;
#endif

    for (const auto* ps : {&empty_ps, &tiny_ps}) {
      const auto enc_or = Codec::Encode(*ps, algo);
      Assert(enc_or.ok(), "encode should succeed for empty/tiny");
      if (!enc_or.ok()) continue;
      const auto dec_or = Codec::Decode(enc_or.value().bytes);
      Assert(dec_or.ok(), "decode should succeed for empty/tiny");
      if (!dec_or.ok()) continue;
      Assert(dec_or.value().polygon_set == *ps, "empty/tiny roundtrip should match");
    }
  }
}

}  // namespace

int main() {
  try {
    RoundTripAllAlgorithmsTest();
    BadMagicShouldFailTest();
    MetricsSwitchTest();
    PayloadViewShouldDecodeWithoutCopyTest();
    AlgorithmEnumShapeShouldNotContainExperimentalAlgoTest();
    CoordinateScaleShouldRoundTripThroughFrameHeaderTest();
    V1FrameShouldDefaultCoordinateScaleToNm1Test();
    TruncatedDataShouldFailDecodeTest();
    VarintBoundaryAndInvalidDataTest();
    EmptyAndTinyObjectsRoundTripTest();

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
