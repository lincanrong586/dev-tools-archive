#pragma once

#include <cstdint>
#include <string>

#include "../types.h"

namespace polygon_codec::perftest {

// benchmark 默认参数集中定义在这里，便于：
// 1) 统一命令行帮助文本与默认行为；
// 2) 避免同一默认值在多个文件重复硬编码。
inline constexpr uint32_t kDefaultSeed = 1;
inline constexpr int32_t kDefaultRegionSizeUm = 100000;
inline constexpr int32_t kDefaultCutWidthUm = 1024 * 72;
inline constexpr int32_t kDefaultCutHeightUm = 2048 * 72;
inline constexpr uint32_t kDefaultMinTotalPoints = 250000;
inline constexpr uint32_t kDefaultMaxTotalPoints = 350000;
inline constexpr uint32_t kDefaultPolygons = 50000;
inline constexpr uint32_t kDefaultPointsPerPolygon = 6;
inline constexpr uint32_t kDefaultMinPolygonPoints = 3;
inline constexpr uint32_t kDefaultMaxPolygonPoints = 10000;

// 场景用于模拟“窗口位置不同但窗口大小一致”的三类区域。
// 注意：这只影响窗口在大区域中的平移位置，不改变窗口尺寸。
enum class ScenarioKind {
  NearOrigin,
  Middle,
  Far,
};

// 两类数据集：
// 1) RandomPoints：历史随机点集生成（点无序、polygon 点数较少时更像压力测试点集）
// 2) MaskLike：更贴近 mask 版图风格（nm 栅格、复杂轮廓、矩形裁剪补点形成闭环）
enum class GenerateKind {
  RandomPoints,
  MaskLike,
};

struct GenerateConfig {
  GenerateKind kind = GenerateKind::RandomPoints;
  ScenarioKind scenario = ScenarioKind::NearOrigin;

  // seed 用于可复现随机生成；benchmark 中通常会做 seed+i 来生成多轮不同数据。
  uint32_t seed = kDefaultSeed;

  // 大区域边长（单位：um）。
  // 例如 100mm=100000um，可设置 region_size_um=100000。
  int32_t region_size_um = kDefaultRegionSizeUm;

  // 截取窗口：支持旧式正方形参数 cut_size_um，也支持新式矩形 cut_width_um/cut_height_um。
  int32_t cut_size_um = 0;
  int32_t cut_width_um = kDefaultCutWidthUm;
  int32_t cut_height_um = kDefaultCutHeightUm;

  // 每轮总点数范围（随机选取）。
  uint32_t min_total_points = kDefaultMinTotalPoints;
  uint32_t max_total_points = kDefaultMaxTotalPoints;

  // polygon 数与点数分布控制：
  // - polygons>0：作为“期望 polygon 数”的中心值，内部会加一定抖动
  // - points_per_polygon：作为“期望平均点数”用于反推 polygon 数
  uint32_t polygons = kDefaultPolygons;
  uint32_t points_per_polygon = kDefaultPointsPerPolygon;

  // 每 polygon 点数范围；RandomPoints 会用该范围分配点数；
  // MaskLike 会将该范围自动抬高到更适合复杂轮廓的区间。
  uint32_t min_polygon_points = kDefaultMinPolygonPoints;
  uint32_t max_polygon_points = kDefaultMaxPolygonPoints;
};

std::string ScenarioName(ScenarioKind scenario);

// 生成一份 PolygonSet 数据。
// - RandomPoints：在窗口内均匀采样点，不额外保证闭环或边界有序
// - MaskLike：先在 nm 栅格生成复杂轮廓，再做矩形裁剪，裁剪产生的交点会自动补到边界上（满足“切割面补点形成闭环”）
PolygonSet GeneratePolygonSet(const GenerateConfig& cfg);

}  // namespace polygon_codec::perftest
