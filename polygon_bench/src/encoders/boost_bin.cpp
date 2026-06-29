#include "encoders/boost_bin.h"

#include <stdexcept>
#include <string>
#include <vector>

#ifdef POLYBENCH_WITH_BOOST
#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>
#include <sstream>
#endif

namespace polybench {

std::string BoostBinEncoder::Name() const { return "boost_bin"; }

#ifdef POLYBENCH_WITH_BOOST
namespace boost::serialization {

template <class Archive>
void serialize(Archive& ar, polybench::Point& p, const unsigned int) {
  ar& p.x;
  ar& p.y;
}

}  // namespace boost::serialization
#endif

EncodeResult BoostBinEncoder::Encode(const PolygonSet& polygon_set) const {
#ifndef POLYBENCH_WITH_BOOST
  (void)polygon_set;
  throw std::runtime_error("boost_bin: not enabled (rebuild with Boost.Serialization)");
#else
  std::ostringstream oss(std::ios::binary);
  boost::archive::binary_oarchive oa(oss);
  oa << polygon_set;
  const std::string s = oss.str();
  EncodeResult r;
  r.bytes.assign(s.begin(), s.end());
  return r;
#endif
}

DecodeResult BoostBinEncoder::Decode(const std::vector<uint8_t>& bytes) const {
#ifndef POLYBENCH_WITH_BOOST
  (void)bytes;
  throw std::runtime_error("boost_bin: not enabled (rebuild with Boost.Serialization)");
#else
  const std::string s(bytes.begin(), bytes.end());
  std::istringstream iss(s, std::ios::binary);
  boost::archive::binary_iarchive ia(iss);
  DecodeResult r;
  ia >> r.polygon_set;
  return r;
#endif
}

}  // namespace polybench

