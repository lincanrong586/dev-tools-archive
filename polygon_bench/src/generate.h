#pragma once

#include <cstdint>
#include <string>

#include "polygon_types.h"

namespace polybench {

// 三个采样场景：
// - NearOrigin: 截取窗口靠近 (0,0)
// - Middle:     截取窗口靠近区域中心
// - Far:        截取窗口靠近最远角 (region_size_um-cut_size_um, region_size_um-cut_size_um)
enum class ScenarioKind {
  NearOrigin,
  Middle,
  Far,
};

// 生成 PolygonSet 的参数。
//
// 说明：
// - region_size_um 对应 100mm×100mm 的区域边长（默认 100000um）
// - cut_size_um 为截取的正方形窗口边长（单位 um）
// - polygons 与 points_per_polygon 对应业务假设（默认 5万 polygon、每个 6 点）
struct GenerateConfig {
  ScenarioKind scenario = ScenarioKind::NearOrigin;
  uint32_t polygons = 50000;
  uint32_t points_per_polygon = 6;
  int32_t region_size_um = 100000;
  int32_t cut_size_um = 1000;
  uint32_t seed = 1;
};

std::string ScenarioName(ScenarioKind scenario);

PolygonSet GeneratePolygonSet(const GenerateConfig& cfg);

}  // namespace polybench
