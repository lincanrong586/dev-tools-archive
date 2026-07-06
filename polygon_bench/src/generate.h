#pragma once

#include <cstdint>
#include <string>

#include "polygon_types.h"

namespace polybench {

// 三个采样场景。
//
// 这里的“场景”只决定截取窗口左下角的放置位置：
// - NearOrigin: 截取窗口靠近 (0, 0)
// - Middle:     截取窗口靠近整块区域中心
// - Far:        截取窗口靠近最远角
//
// 注意：Far 场景下 x 与 y 的最大起点需要分别按窗口宽度/高度计算，
// 因此当窗口不是正方形时，最远角坐标将是：
//   (region_size_um - cut_width_um, region_size_um - cut_height_um)
enum class ScenarioKind {
  NearOrigin,
  Middle,
  Far,
};

enum class GenerateKind {
  RandomPoints,
  MaskLike,
};

// 生成 PolygonSet 的参数。
//
// 设计目标：
// 1. 默认参数尽量贴近当前基准场景；
// 2. 允许用户直接设置矩形窗口宽高；
// 3. 保留旧版 cut_size_um 作为兼容入口，便于已有脚本继续工作。
//
// 字段说明：
// - region_size_um:
//     对应整块区域边长，单位 um。当前业务背景中默认是 100mm = 100000um。
// - min_total_points / max_total_points:
//     单次生成的 PolygonSet 总点数范围。默认 25w ~ 35w，目标中心约 30w。
// - polygons:
//     目标 polygon 个数。实际生成时会在该值附近做随机扰动，而不是固定不变。
// - points_per_polygon:
//     目标平均每 polygon 点数。实际每个 polygon 的点数会随机变化，但会尽量围绕该平均值。
// - min_polygon_points / max_polygon_points:
//     单个 polygon 允许的点数上下界，默认 3 ~ 10000。
// - cut_width_um / cut_height_um:
//     截取窗口的宽和高，单位 um。未显式指定时默认 (1024*72) x (2048*72)。
// - cut_size_um:
//     兼容旧参数。若 > 0，则视为“正方形窗口边长”，优先级高于宽/高字段。
struct GenerateConfig {
  GenerateKind kind = GenerateKind::RandomPoints;
  ScenarioKind scenario = ScenarioKind::NearOrigin;
  uint32_t min_total_points = 250000;
  uint32_t max_total_points = 350000;
  uint32_t polygons = 50000;
  uint32_t points_per_polygon = 6;
  uint32_t min_polygon_points = 3;
  uint32_t max_polygon_points = 10000;
  int32_t region_size_um = 100000;
  int32_t cut_width_um = 1024 * 72;
  int32_t cut_height_um = 2048 * 72;
  int32_t cut_size_um = 0;
  uint32_t seed = 1;
};

// 返回场景名称，用于表格输出与日志显示。
std::string ScenarioName(ScenarioKind scenario);

// 根据 GenerateConfig 生成测试用 PolygonSet。
//
// 说明：
// - 这里只关注“数据分布是否符合窗口范围”，不校验几何闭环、自交、互不重叠等真实版图约束；
// - 生成的数据主要服务于编码体积和编解码耗时对比。
PolygonSet GeneratePolygonSet(const GenerateConfig& cfg);

}  // namespace polybench
