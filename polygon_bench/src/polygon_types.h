#pragma once

#include <cstdint>
#include <vector>

#ifdef POLYBENCH_WITH_BOOST
#include <boost/mpl/bool_fwd.hpp>
#include <boost/serialization/is_bitwise_serializable.hpp>
#endif

namespace polybench {

struct Point {
  long long x = 0;
  long long y = 0;
};

using Polygon = std::vector<Point>;
using PolygonSet = std::vector<Polygon>;

bool operator==(const Point& a, const Point& b);
bool operator!=(const Point& a, const Point& b);

size_t PointCount(const PolygonSet& ps);

}  // namespace polybench

#ifdef POLYBENCH_WITH_BOOST
namespace boost::serialization {

template <>
struct is_bitwise_serializable<polybench::Point> {
  static const bool value = true;
  using type = boost::mpl::bool_<true>;
};

}  // namespace boost::serialization
#endif
