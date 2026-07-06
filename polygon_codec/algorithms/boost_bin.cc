#include "boost_bin.h"

#ifdef POLYGON_CODEC_WITH_BOOST

#include <sstream>

#include <boost/archive/binary_iarchive.hpp>
#include <boost/archive/binary_oarchive.hpp>
#include <boost/serialization/vector.hpp>

namespace polygon_codec {

std::vector<uint8_t> BoostBinCodec::EncodePayload(const PolygonSet& ps) const {
  std::ostringstream oss(std::ios::binary);
  boost::archive::binary_oarchive oa(oss);
  oa << ps;
  const std::string s = oss.str();
  return std::vector<uint8_t>(s.begin(), s.end());
}

PolygonSet BoostBinCodec::DecodePayload(PayloadView payload) const {
  const std::string s(reinterpret_cast<const char*>(payload.data), payload.size);
  std::istringstream iss(s, std::ios::binary);
  boost::archive::binary_iarchive ia(iss);
  PolygonSet ps;
  ia >> ps;
  return ps;
}

}  // namespace polygon_codec

#endif
