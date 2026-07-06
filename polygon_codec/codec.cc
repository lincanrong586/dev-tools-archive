#include "codec.h"

#include <chrono>
#include <exception>

#include "codec_factory.h"
#include "codec_frame.h"
#include "codec_metrics.h"

namespace polygon_codec {

StatusOr<EncodeOutput> Codec::Encode(const PolygonSet& ps, Algorithm algo, const Options& opt) {
  try {
    auto codec = CodecFactory::Create(algo);
    if (!codec) return Status::Error("codec: 不支持的算法，或算法未启用");

    EncodeOutput out;
    out.algorithm = algo;

    const bool enable_metrics = opt.enable_metrics;
    const auto t0 = std::chrono::steady_clock::now();
    const std::vector<uint8_t> payload = codec->EncodePayload(ps);
    out.bytes = WrapFrame(algo, payload);
    const auto t1 = std::chrono::steady_clock::now();

    if (enable_metrics) {
      out.metrics.enabled = true;
      out.metrics.encode_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      out.metrics.raw_bytes_estimated = EstimateRawBytes(ps);
      out.metrics.encoded_bytes = out.bytes.size();
      out.metrics.bytes_per_polygon =
          ps.empty() ? 0.0 : static_cast<double>(out.metrics.encoded_bytes) / static_cast<double>(ps.size());
      out.metrics.compression_ratio =
          out.metrics.raw_bytes_estimated == 0 ? 0.0 : static_cast<double>(out.metrics.encoded_bytes) / static_cast<double>(out.metrics.raw_bytes_estimated);
      if (opt.print_metrics) PrintMetricsLine("encode", algo, out.metrics);
    }

    return out;
  } catch (const std::exception& e) {
    return Status::Error(std::string("codec: encode 失败: ") + e.what());
  }
}

StatusOr<DecodeOutput> Codec::Decode(const std::vector<uint8_t>& bytes, const Options& opt) {
  try {
    const bool enable_metrics = opt.enable_metrics;
    const auto t0 = std::chrono::steady_clock::now();

    const auto header_or = ParseHeader(bytes);
    if (!header_or.ok()) return header_or.status();
    const Algorithm algo = header_or.value().algorithm;

    auto codec = CodecFactory::Create(algo);
    if (!codec) return Status::Error("codec: 该算法未启用或不受支持");

    const auto payload_view_or = ExtractPayloadView(bytes);
    if (!payload_view_or.ok()) return payload_view_or.status();

    DecodeOutput out;
    out.algorithm = algo;
    out.polygon_set = codec->DecodePayload(payload_view_or.value());

    const auto t1 = std::chrono::steady_clock::now();

    if (enable_metrics) {
      out.metrics.enabled = true;
      out.metrics.decode_ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
      out.metrics.raw_bytes_estimated = EstimateRawBytes(out.polygon_set);
      out.metrics.encoded_bytes = bytes.size();
      out.metrics.bytes_per_polygon =
          out.polygon_set.empty() ? 0.0 : static_cast<double>(out.metrics.encoded_bytes) / static_cast<double>(out.polygon_set.size());
      out.metrics.compression_ratio =
          out.metrics.raw_bytes_estimated == 0 ? 0.0 : static_cast<double>(out.metrics.encoded_bytes) / static_cast<double>(out.metrics.raw_bytes_estimated);
      if (opt.print_metrics) PrintMetricsLine("decode", algo, out.metrics);
    }

    return out;
  } catch (const std::exception& e) {
    return Status::Error(std::string("codec: decode 失败: ") + e.what());
  }
}

}  // namespace polygon_codec
