#pragma once

#include <cstddef>
#include <vector>

#ifdef POLYGON_CODEC_WITH_BOOST
#include <boost/mpl/bool.hpp>
#include <boost/serialization/is_bitwise_serializable.hpp>
#endif

namespace polygon_codec {

// Point 表示一个二维整数坐标点。
//
// 约定：
// - x/y 均使用 long long，满足 nm/um 等高精度整数坐标需求
// - 编解码算法不对坐标单位做假设，单位由业务侧约定
struct Point {
  long long x = 0;
  long long y = 0;
};

// Polygon 表示一个闭环轮廓（点序列）。
// 注意：本库只关心点序列本身用于编解码；不在库内校验“是否自交/是否闭环/是否有孔洞”等几何约束。
using Polygon = std::vector<Point>;

// PolygonSet 表示一个窗口/区域内的多个 polygon。
using PolygonSet = std::vector<Polygon>;

inline bool operator==(const Point& a, const Point& b) { return a.x == b.x && a.y == b.y; }

inline bool operator==(const Polygon& a, const Polygon& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (!(a[i] == b[i])) return false;
  }
  return true;
}

inline bool operator==(const PolygonSet& a, const PolygonSet& b) {
  if (a.size() != b.size()) return false;
  for (size_t i = 0; i < a.size(); ++i) {
    if (!(a[i] == b[i])) return false;
  }
  return true;
}

}  // namespace polygon_codec

#ifdef POLYGON_CODEC_WITH_BOOST
namespace boost::serialization {

template <>
struct is_bitwise_serializable<polygon_codec::Point> {
  static const bool value = true;
  using type = boost::mpl::bool_<true>;
};

}  // namespace boost::serialization
#endif
