#include "generate.h"

#include <algorithm>
#include <random>

namespace polybench {

std::string ScenarioName(ScenarioKind scenario) {
  switch (scenario) {
    case ScenarioKind::NearOrigin:
      return "near_origin";
    case ScenarioKind::Middle:
      return "middle";
    case ScenarioKind::Far:
      return "far";
  }
  return "unknown";
}

static std::pair<int32_t, int32_t> ScenarioOrigin(const GenerateConfig& cfg) {
  const int32_t max0 = std::max(0, cfg.region_size_um - cfg.cut_size_um);
  switch (cfg.scenario) {
    case ScenarioKind::NearOrigin:
      return {0, 0};
    case ScenarioKind::Middle:
      return {max0 / 2, max0 / 2};
    case ScenarioKind::Far:
      return {max0, max0};
  }
  return {0, 0};
}

PolygonSet GeneratePolygonSet(const GenerateConfig& cfg) {
  const auto [x0, y0] = ScenarioOrigin(cfg);
  std::mt19937 rng(cfg.seed);
  std::uniform_int_distribution<int32_t> dx(0, std::max(0, cfg.cut_size_um - 1));
  std::uniform_int_distribution<int32_t> dy(0, std::max(0, cfg.cut_size_um - 1));

  PolygonSet ps;
  ps.reserve(cfg.polygons);
  for (uint32_t i = 0; i < cfg.polygons; ++i) {
    Polygon poly;
    poly.reserve(cfg.points_per_polygon);
    for (uint32_t j = 0; j < cfg.points_per_polygon; ++j) {
      Point p;
      p.x = x0 + dx(rng);
      p.y = y0 + dy(rng);
      poly.push_back(p);
    }
    ps.push_back(std::move(poly));
  }
  return ps;
}

}  // namespace polybench

