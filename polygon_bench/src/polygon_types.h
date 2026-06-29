#pragma once

#include <cstdint>
#include <vector>

namespace polybench {

struct Point {
  int32_t x = 0;
  int32_t y = 0;
};

using Polygon = std::vector<Point>;
using PolygonSet = std::vector<Polygon>;

bool operator==(const Point& a, const Point& b);
bool operator!=(const Point& a, const Point& b);

size_t PointCount(const PolygonSet& ps);

}  // namespace polybench
