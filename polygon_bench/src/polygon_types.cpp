#include "polygon_types.h"

namespace polybench {

bool operator==(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }
bool operator!=(const Point& a, const Point& b) { return !(a == b); }

size_t PointCount(const PolygonSet& ps) {
  size_t n = 0;
  for (const auto& poly : ps) n += poly.size();
  return n;
}

}  // namespace polybench
