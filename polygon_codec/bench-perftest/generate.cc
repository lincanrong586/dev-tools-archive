#include "generate.h"

#include <algorithm>
#include <cmath>
#include <limits>
#include <random>
#include <vector>

namespace polygon_codec::perftest {

namespace {

constexpr uint32_t kMinPolygonPoints = 1;
constexpr uint32_t kPreferredPolygonJitterDivisor = 10;
constexpr int32_t kNearOriginCoordinate = 0;
constexpr long long kRegionUpperBoundBias = 1;
constexpr uint32_t kMaskMinControlPoints = 8;
constexpr uint32_t kMaskMaxControlPoints = 32;
constexpr uint32_t kMaskControlPointDivisor = 10;
constexpr double kOrthogonalSnapRatio = 0.45;
constexpr double kDiagonalSnapRatio = 0.75;
constexpr double kRadiusBaseRatio = 0.65;
constexpr double kRadiusJitterRatio = 0.35;
constexpr long long kNmPerUm = 1000;
constexpr uint32_t kMaskMinPointsPerPolygon = 300;
constexpr uint32_t kMaskMinPolygonPoints = 200;
constexpr uint32_t kMaskMaxPolygonPoints = 2000;

int32_t EffectiveCutWidth(const GenerateConfig& cfg) { return cfg.cut_size_um > 0 ? cfg.cut_size_um : cfg.cut_width_um; }
int32_t EffectiveCutHeight(const GenerateConfig& cfg) { return cfg.cut_size_um > 0 ? cfg.cut_size_um : cfg.cut_height_um; }

uint32_t ClampU32(uint32_t value, uint32_t low, uint32_t high) { return std::min(std::max(value, low), high); }

uint32_t ChooseTotalPoints(const GenerateConfig& cfg, std::mt19937& rng) {
  const uint32_t low = std::min(cfg.min_total_points, cfg.max_total_points);
  const uint32_t high = std::max(cfg.min_total_points, cfg.max_total_points);
  std::uniform_int_distribution<uint32_t> dist(low, high);
  return dist(rng);
}

uint32_t ChoosePolygonCount(const GenerateConfig& cfg, uint32_t total_points, std::mt19937& rng) {
  const uint32_t min_polygon_points = std::max(kMinPolygonPoints, cfg.min_polygon_points);
  const uint32_t max_polygon_points = std::max(min_polygon_points, cfg.max_polygon_points);

  const uint32_t min_polygons = std::max(kMinPolygonPoints, (total_points + max_polygon_points - 1) / max_polygon_points);
  const uint32_t max_polygons = std::max(kMinPolygonPoints, total_points / min_polygon_points);

  const uint32_t preferred_by_avg = std::max(kMinPolygonPoints, total_points / std::max(kMinPolygonPoints, cfg.points_per_polygon));
  const uint32_t preferred_polygons = cfg.polygons > 0 ? cfg.polygons : preferred_by_avg;
  const uint32_t clamped_preferred = ClampU32(preferred_polygons, min_polygons, max_polygons);

  const uint32_t jitter = std::max(kMinPolygonPoints, clamped_preferred / kPreferredPolygonJitterDivisor);
  const uint32_t low =
      ClampU32(clamped_preferred > jitter ? clamped_preferred - jitter : kMinPolygonPoints, min_polygons, max_polygons);
  const uint32_t high = ClampU32(clamped_preferred + jitter, min_polygons, max_polygons);

  if (low >= high) return low;
  std::uniform_int_distribution<uint32_t> dist(low, high);
  return dist(rng);
}

std::vector<uint32_t> GeneratePolygonPointCounts(const GenerateConfig& cfg, uint32_t polygon_count, uint32_t total_points,
                                                 std::mt19937& rng) {
  const uint32_t min_polygon_points = std::max(kMinPolygonPoints, cfg.min_polygon_points);
  const uint32_t max_polygon_points = std::max(min_polygon_points, cfg.max_polygon_points);
  const uint32_t max_extra_per_polygon = max_polygon_points - min_polygon_points;

  std::vector<uint32_t> counts(polygon_count, min_polygon_points);
  uint32_t remaining = total_points - polygon_count * min_polygon_points;

  for (uint32_t i = 0; i < polygon_count; ++i) {
    if (remaining == 0) break;

    const uint32_t rest = polygon_count - i - 1;
    const uint32_t min_extra_here = remaining > rest * max_extra_per_polygon ? remaining - rest * max_extra_per_polygon : 0;
    const uint32_t max_extra_here = std::min(max_extra_per_polygon, remaining);

    uint32_t extra = max_extra_here;
    if (i + 1 < polygon_count && min_extra_here < max_extra_here) {
      std::uniform_int_distribution<uint32_t> dist(min_extra_here, max_extra_here);
      extra = dist(rng);
    }

    counts[i] += extra;
    remaining -= extra;
  }

  std::shuffle(counts.begin(), counts.end(), rng);
  return counts;
}

static std::pair<int32_t, int32_t> ScenarioOrigin(const GenerateConfig& cfg) {
  const int32_t cut_width_um = EffectiveCutWidth(cfg);
  const int32_t cut_height_um = EffectiveCutHeight(cfg);
  const int32_t max_x0 = std::max(0, cfg.region_size_um - cut_width_um);
  const int32_t max_y0 = std::max(0, cfg.region_size_um - cut_height_um);
  switch (cfg.scenario) {
    case ScenarioKind::NearOrigin:
      return {kNearOriginCoordinate, kNearOriginCoordinate};
    case ScenarioKind::Middle:
      return {max_x0 / 2, max_y0 / 2};
    case ScenarioKind::Far:
      return {max_x0, max_y0};
  }
  return {kNearOriginCoordinate, kNearOriginCoordinate};
}

struct RectLL {
  long long x0 = 0;
  long long y0 = 0;
  long long x1 = 0;
  long long y1 = 0;
};

static long long ClampLL(long long v, long long low, long long high) { return std::min(std::max(v, low), high); }

static bool InsideLeft(const Point& p, const RectLL& r) { return p.x >= r.x0; }
static bool InsideRight(const Point& p, const RectLL& r) { return p.x <= r.x1; }
static bool InsideBottom(const Point& p, const RectLL& r) { return p.y >= r.y0; }
static bool InsideTop(const Point& p, const RectLL& r) { return p.y <= r.y1; }

static Point IntersectAtX(const Point& a, const Point& b, long long x) {
  if (a.x == b.x) return Point{x, a.y};
  const long double t = static_cast<long double>(x - a.x) / static_cast<long double>(b.x - a.x);
  const long double y = static_cast<long double>(a.y) + t * static_cast<long double>(b.y - a.y);
  return Point{x, static_cast<long long>(std::llround(y))};
}

static Point IntersectAtY(const Point& a, const Point& b, long long y) {
  if (a.y == b.y) return Point{a.x, y};
  const long double t = static_cast<long double>(y - a.y) / static_cast<long double>(b.y - a.y);
  const long double x = static_cast<long double>(a.x) + t * static_cast<long double>(b.x - a.x);
  return Point{static_cast<long long>(std::llround(x)), y};
}

template <typename InsideFn, typename IntersectFn>
static Polygon ClipAgainst(const Polygon& in, InsideFn inside, IntersectFn intersect) {
  Polygon out;
  if (in.empty()) return out;

  Point prev = in.back();
  bool prev_in = inside(prev);
  for (const auto& cur : in) {
    const bool cur_in = inside(cur);
    if (prev_in && cur_in) {
      out.push_back(cur);
    } else if (prev_in && !cur_in) {
      out.push_back(intersect(prev, cur));
    } else if (!prev_in && cur_in) {
      out.push_back(intersect(prev, cur));
      out.push_back(cur);
    }
    prev = cur;
    prev_in = cur_in;
  }
  return out;
}

static void DedupPolygon(Polygon& poly) {
  if (poly.empty()) return;
  Polygon out;
  out.reserve(poly.size());
  out.push_back(poly.front());
  for (size_t i = 1; i < poly.size(); ++i) {
    if (!(poly[i] == out.back())) out.push_back(poly[i]);
  }
  if (out.size() > 1 && out.front() == out.back()) out.pop_back();
  poly.swap(out);
}

static Polygon ClipPolygonToRect(const Polygon& poly, const RectLL& r) {
  Polygon p = poly;
  p = ClipAgainst(p, [&](const Point& pt) { return InsideLeft(pt, r); },
                  [&](const Point& a, const Point& b) { return IntersectAtX(a, b, r.x0); });
  p = ClipAgainst(p, [&](const Point& pt) { return InsideRight(pt, r); },
                  [&](const Point& a, const Point& b) { return IntersectAtX(a, b, r.x1); });
  p = ClipAgainst(p, [&](const Point& pt) { return InsideBottom(pt, r); },
                  [&](const Point& a, const Point& b) { return IntersectAtY(a, b, r.y0); });
  p = ClipAgainst(p, [&](const Point& pt) { return InsideTop(pt, r); },
                  [&](const Point& a, const Point& b) { return IntersectAtY(a, b, r.y1); });
  DedupPolygon(p);
  return p;
}

static Polygon GenerateMaskLikePolygon(long long cx, long long cy, long long radius_nm, uint32_t points, std::mt19937& rng) {
  const uint32_t control = std::max(kMaskMinControlPoints, std::min(kMaskMaxControlPoints, points / kMaskControlPointDivisor));
  std::uniform_real_distribution<double> jitter01(0.0, 1.0);
  constexpr double kPi = 3.14159265358979323846;
  std::uniform_real_distribution<double> angle_dist(0.0, 2.0 * kPi);

  std::vector<std::pair<long long, long long>> control_pts;
  control_pts.reserve(control);

  for (uint32_t i = 0; i < control; ++i) {
    double angle = angle_dist(rng);
    const double mode = jitter01(rng);

    if (mode < kOrthogonalSnapRatio) {
      angle = std::round(angle / (kPi / 2.0)) * (kPi / 2.0);
    } else if (mode < kDiagonalSnapRatio) {
      angle = std::round(angle / (kPi / 4.0)) * (kPi / 4.0);
    }

    const double rr = static_cast<double>(radius_nm) * (kRadiusBaseRatio + kRadiusJitterRatio * jitter01(rng));
    const long long x = cx + static_cast<long long>(std::llround(rr * std::cos(angle)));
    const long long y = cy + static_cast<long long>(std::llround(rr * std::sin(angle)));
    control_pts.push_back({x, y});
  }

  Polygon poly;
  poly.reserve(points);

  for (uint32_t i = 0; i < control; ++i) {
    const auto [x0, y0] = control_pts[i];
    const auto [x1, y1] = control_pts[(i + 1) % control];
    const uint32_t seg_points = std::max(1u, points / control);

    for (uint32_t j = 0; j < seg_points; ++j) {
      const double t = static_cast<double>(j) / static_cast<double>(seg_points);
      const long long x = static_cast<long long>(std::llround(static_cast<double>(x0) + t * static_cast<double>(x1 - x0)));
      const long long y = static_cast<long long>(std::llround(static_cast<double>(y0) + t * static_cast<double>(y1 - y0)));
      poly.push_back(Point{x, y});
      if (poly.size() >= points) break;
    }
    if (poly.size() >= points) break;
  }

  DedupPolygon(poly);
  return poly;
}

static PolygonSet GenerateMaskLikePolygonSet(const GenerateConfig& cfg, std::mt19937& rng) {
  // 将 um 坐标转成 nm 栅格：1um=1000nm。
  constexpr long long kScale = kNmPerUm;
  const long long region_nm = static_cast<long long>(cfg.region_size_um) * kScale;
  const long long cut_w_nm = static_cast<long long>(EffectiveCutWidth(cfg)) * kScale;
  const long long cut_h_nm = static_cast<long long>(EffectiveCutHeight(cfg)) * kScale;

  const auto [x0_um, y0_um] = ScenarioOrigin(cfg);
  const long long x0 = static_cast<long long>(x0_um) * kScale;
  const long long y0 = static_cast<long long>(y0_um) * kScale;

  RectLL window;
  window.x0 = x0;
  window.y0 = y0;
  window.x1 = x0 + std::max(0LL, cut_w_nm - kRegionUpperBoundBias);
  window.y1 = y0 + std::max(0LL, cut_h_nm - kRegionUpperBoundBias);

  const uint32_t total_points = ChooseTotalPoints(cfg, rng);

  // MaskLike 的设计目标是让单 polygon 点数偏大（更接近复杂轮廓/OPC 锯齿边）。
  const uint32_t desired_pp = std::max(kMaskMinPointsPerPolygon, cfg.points_per_polygon);
  const uint32_t rough_polygons = std::max(kMinPolygonPoints, total_points / desired_pp);
  const uint32_t jitter = std::max(kMinPolygonPoints, rough_polygons / kPreferredPolygonJitterDivisor);
  std::uniform_int_distribution<uint32_t> poly_count_dist(rough_polygons > jitter ? rough_polygons - jitter : kMinPolygonPoints,
                                                          rough_polygons + jitter);
  const uint32_t polygon_count = poly_count_dist(rng);

  GenerateConfig local = cfg;
  local.polygons = 0;
  local.points_per_polygon = desired_pp;
  local.min_polygon_points = std::max(kMaskMinPolygonPoints, cfg.min_polygon_points);
  local.max_polygon_points = std::max(local.min_polygon_points, std::max(kMaskMaxPolygonPoints, cfg.max_polygon_points));
  const std::vector<uint32_t> point_counts = GeneratePolygonPointCounts(local, polygon_count, total_points, rng);

  const long long min_dim = std::max(1LL, std::min(cut_w_nm, cut_h_nm));
  const long long radius_low = std::max(10LL, (min_dim * 2) / 3);
  const long long radius_high = std::max(radius_low, min_dim * 2);
  std::uniform_int_distribution<long long> radius_dist(radius_low, radius_high);

  const long long margin = min_dim / 2;
  const long long cx_low = ClampLL(window.x0 - margin, 0, std::max(0LL, region_nm - kRegionUpperBoundBias));
  const long long cx_high = ClampLL(window.x1 + margin, 0, std::max(0LL, region_nm - kRegionUpperBoundBias));
  const long long cy_low = ClampLL(window.y0 - margin, 0, std::max(0LL, region_nm - kRegionUpperBoundBias));
  const long long cy_high = ClampLL(window.y1 + margin, 0, std::max(0LL, region_nm - kRegionUpperBoundBias));

  const long long edge = std::max(1LL, min_dim / 10);
  std::uniform_real_distribution<double> pick01(0.0, 1.0);
  std::uniform_int_distribution<long long> cx_any(cx_low, cx_high);
  std::uniform_int_distribution<long long> cy_any(cy_low, cy_high);
  std::uniform_int_distribution<long long> cx_left(cx_low, ClampLL(window.x0 + edge, cx_low, cx_high));
  std::uniform_int_distribution<long long> cx_right(ClampLL(window.x1 - edge, cx_low, cx_high), cx_high);
  std::uniform_int_distribution<long long> cy_bottom(cy_low, ClampLL(window.y0 + edge, cy_low, cy_high));
  std::uniform_int_distribution<long long> cy_top(ClampLL(window.y1 - edge, cy_low, cy_high), cy_high);

  PolygonSet ps;
  ps.reserve(point_counts.size());

  size_t accumulated = 0;
  for (const uint32_t pc : point_counts) {
    if (accumulated >= total_points) break;

    // 刻意提高“靠近裁剪边界”的概率，让裁剪产生更多交点补点，更贴近“切割面补点闭环”的特征。
    const double kEdgeBias = 0.70;
    const long long cx = pick01(rng) < kEdgeBias ? (pick01(rng) < 0.5 ? cx_left(rng) : cx_right(rng)) : cx_any(rng);
    const long long cy = pick01(rng) < kEdgeBias ? (pick01(rng) < 0.5 ? cy_bottom(rng) : cy_top(rng)) : cy_any(rng);

    const long long radius = radius_dist(rng);
    const Polygon raw = GenerateMaskLikePolygon(cx, cy, radius, pc, rng);
    Polygon clipped = ClipPolygonToRect(raw, window);
    if (clipped.size() < 3) continue;

    accumulated += clipped.size();
    ps.push_back(std::move(clipped));
  }

  return ps;
}

}  // namespace

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

PolygonSet GeneratePolygonSet(const GenerateConfig& cfg) {
  std::mt19937 rng(cfg.seed);

  if (cfg.kind == GenerateKind::MaskLike) {
    return GenerateMaskLikePolygonSet(cfg, rng);
  }

  const uint32_t total_points = ChooseTotalPoints(cfg, rng);
  const uint32_t polygon_count = ChoosePolygonCount(cfg, total_points, rng);
  const std::vector<uint32_t> point_counts = GeneratePolygonPointCounts(cfg, polygon_count, total_points, rng);

  const auto [x0, y0] = ScenarioOrigin(cfg);
  const int32_t cut_width_um = EffectiveCutWidth(cfg);
  const int32_t cut_height_um = EffectiveCutHeight(cfg);

  // RandomPoints 只要求点在窗口范围内；不额外保证点序沿边界连续。
  std::uniform_int_distribution<int32_t> dx(0, std::max(0, cut_width_um - 1));
  std::uniform_int_distribution<int32_t> dy(0, std::max(0, cut_height_um - 1));

  PolygonSet ps;
  ps.reserve(point_counts.size());
  for (const uint32_t count : point_counts) {
    Polygon poly;
    poly.reserve(count);
    for (uint32_t i = 0; i < count; ++i) {
      poly.push_back(Point{static_cast<long long>(x0 + dx(rng)), static_cast<long long>(y0 + dy(rng))});
    }
    ps.push_back(std::move(poly));
  }

  return ps;
}

}  // namespace polygon_codec::perftest
