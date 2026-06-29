#include <exception>
#include <iostream>
#include <string>

#include "encoders/bin_fixed.h"
#include "encoders/bin_offset_varint.h"
#include "encoders/delta_varint.h"
#include "encoders/json_arr.h"
#include "encoders/json_obj.h"
#include "encoders/mini_pbuf.h"
#include "generate.h"

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

}  // namespace

int main() {
  try {
    polybench::GenerateConfig cfg;
    cfg.polygons = 3;
    cfg.points_per_polygon = 4;
    cfg.cut_size_um = 10;
    cfg.region_size_um = 100000;
    cfg.seed = 123;

    for (auto scenario : {polybench::ScenarioKind::NearOrigin, polybench::ScenarioKind::Middle, polybench::ScenarioKind::Far}) {
      cfg.scenario = scenario;
      const auto ps = polybench::GeneratePolygonSet(cfg);

      RoundTripTest<polybench::MiniPbufEncoder>(ps);
      RoundTripTest<polybench::JsonObjEncoder>(ps);
      RoundTripTest<polybench::JsonArrEncoder>(ps);
      RoundTripTest<polybench::BinFixedEncoder>(ps);
      RoundTripTest<polybench::BinOffsetVarintEncoder>(ps);
      RoundTripTest<polybench::DeltaVarintEncoder>(ps);
    }

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
