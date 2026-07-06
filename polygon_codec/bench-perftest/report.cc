#include "report.h"

#include <algorithm>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>

namespace polygon_codec::perftest {

std::string AlgorithmName(Algorithm algo) {
  switch (algo) {
    case Algorithm::kBinFixed:
      return "bin_fixed";
    case Algorithm::kBinOffsetVarint:
      return "bin_offset_varint";
    case Algorithm::kDeltaVarint:
      return "delta_varint";
    case Algorithm::kDeltaWindowMinVarint:
      return "delta_windowmin_varint";
    case Algorithm::kPbuf:
      return "pbuf";
    case Algorithm::kBoostBin:
      return "boost_bin";
    case Algorithm::kButt:
      return "butt";
  }
  return "unknown";
}

namespace {

static std::string F3(double v) {
  std::ostringstream s;
  s << std::fixed << std::setprecision(3) << v;
  return s.str();
}

static std::string U32Range(const U32Stat3& s) {
  return std::to_string(s.avg) + " (" + std::to_string(s.min) + "~" + std::to_string(s.max) + ")";
}

static std::string DRange(const Stat3& s) { return F3(s.avg) + " (" + F3(s.min) + "~" + F3(s.max) + ")"; }

static ReportRow AverageRow(const std::vector<ReportRow>& rows) {
  ReportRow avg;
  avg.scenario = "avg";
  if (rows.empty()) return avg;

  avg.algos = rows.front().algos;
  avg.size_mb.resize(avg.algos.size());
  avg.bytes_per_polygon.resize(avg.algos.size());
  avg.enc_ms.resize(avg.algos.size());
  avg.dec_ms.resize(avg.algos.size());

  {
    double sum = 0.0;
    avg.polygons.min = rows.front().polygons.min;
    avg.polygons.max = rows.front().polygons.max;
    for (const auto& r : rows) {
      sum += static_cast<double>(r.polygons.avg);
      avg.polygons.min = std::min(avg.polygons.min, r.polygons.min);
      avg.polygons.max = std::max(avg.polygons.max, r.polygons.max);
    }
    avg.polygons.avg = static_cast<uint32_t>(std::llround(sum / static_cast<double>(rows.size())));
  }

  {
    double sum = 0.0;
    avg.points.min = rows.front().points.min;
    avg.points.max = rows.front().points.max;
    for (const auto& r : rows) {
      sum += static_cast<double>(r.points.avg);
      avg.points.min = std::min(avg.points.min, r.points.min);
      avg.points.max = std::max(avg.points.max, r.points.max);
    }
    avg.points.avg = static_cast<uint32_t>(std::llround(sum / static_cast<double>(rows.size())));
  }

  auto avg_stat = [&](auto getvec, size_t idx) {
    Stat3 s;
    double sum = 0.0;
    s.min = std::numeric_limits<double>::infinity();
    s.max = -std::numeric_limits<double>::infinity();
    for (const auto& r : rows) {
      const Stat3 v = getvec(r)[idx];
      sum += v.avg;
      s.min = std::min(s.min, v.min);
      s.max = std::max(s.max, v.max);
    }
    s.avg = sum / static_cast<double>(rows.size());
    return s;
  };

  {
    Stat3 s;
    double sum = 0.0;
    s.min = std::numeric_limits<double>::infinity();
    s.max = -std::numeric_limits<double>::infinity();
    for (const auto& r : rows) {
      sum += r.poly_mb.avg;
      s.min = std::min(s.min, r.poly_mb.min);
      s.max = std::max(s.max, r.poly_mb.max);
    }
    s.avg = sum / static_cast<double>(rows.size());
    avg.poly_mb = s;
  }

  for (size_t i = 0; i < avg.algos.size(); ++i) {
    avg.size_mb[i] = avg_stat([](const ReportRow& r) { return r.size_mb; }, i);
    avg.bytes_per_polygon[i] = avg_stat([](const ReportRow& r) { return r.bytes_per_polygon; }, i);
    avg.enc_ms[i] = avg_stat([](const ReportRow& r) { return r.enc_ms; }, i);
    avg.dec_ms[i] = avg_stat([](const ReportRow& r) { return r.dec_ms; }, i);
  }

  return avg;
}

static std::string Highlight(const Stat3& v, double best, double worst) {
  const std::string formatted = DRange(v);
  constexpr double kEps = 1e-9;
  const bool is_best = std::abs(v.avg - best) <= kEps;
  const bool is_worst = std::abs(v.avg - worst) <= kEps;
  if (is_best && is_worst) return formatted;
  if (is_best) return "<span style=\"color: green; font-weight: bold;\">" + formatted + "</span>";
  if (is_worst) return "<span style=\"color: red; font-weight: bold;\">" + formatted + "</span>";
  return formatted;
}

static void AppendDatasetOverview(std::ostringstream& out, const std::vector<ReportRow>& rows, bool include_avg) {
  out << "## Dataset Overview\n\n";
  out << "| scenario | polygons | points | poly(MB) |\n";
  out << "|---|---:|---:|---:|\n";
  for (const auto& r : rows) {
    out << "| " << r.scenario << " | " << U32Range(r.polygons) << " | " << U32Range(r.points) << " | " << DRange(r.poly_mb) << " |\n";
  }
  if (include_avg) {
    const auto avg = AverageRow(rows);
    out << "| " << avg.scenario << " | " << U32Range(avg.polygons) << " | " << U32Range(avg.points) << " | " << DRange(avg.poly_mb) << " |\n";
  }
  out << "\n";
}

static void AppendMetricTable(std::ostringstream& out, const std::string& title, const std::vector<ReportRow>& rows,
                              bool include_avg, const std::vector<Algorithm>& algos,
                              const std::function<const std::vector<Stat3>&(const ReportRow&)>& getter) {
  out << "## " << title << "\n\n";
  out << "| scenario |";
  for (const auto a : algos) out << " " << AlgorithmName(a) << " |";
  out << "\n|---|";
  for (size_t i = 0; i < algos.size(); ++i) out << "---:|";
  out << "\n";

  auto dump_row = [&](const ReportRow& r) {
    double best = std::numeric_limits<double>::infinity();
    double worst = -std::numeric_limits<double>::infinity();
    const auto& v = getter(r);
    for (const auto& x : v) {
      best = std::min(best, x.avg);
      worst = std::max(worst, x.avg);
    }

    out << "| " << r.scenario << " |";
    for (const auto& x : v) out << " " << Highlight(x, best, worst) << " |";
    out << "\n";
  };

  for (const auto& r : rows) dump_row(r);
  if (include_avg) dump_row(AverageRow(rows));
  out << "\n";
}

}  // namespace

std::string RenderMarkdown(const std::string& title, const std::vector<ReportRow>& rows, bool include_avg) {
  std::ostringstream out;
  out << "## " << title << "\n\n";
  if (rows.empty()) return out.str();
  const auto algos = rows.front().algos;

  AppendDatasetOverview(out, rows, include_avg);
  AppendMetricTable(out, "Size Comparison (MB)", rows, include_avg, algos,
                    [](const ReportRow& r) -> const std::vector<Stat3>& { return r.size_mb; });
  AppendMetricTable(out, "Average Encoded Length Per Object (bytes/object)", rows, include_avg, algos,
                    [](const ReportRow& r) -> const std::vector<Stat3>& { return r.bytes_per_polygon; });
  AppendMetricTable(out, "Encode Latency Comparison (ms)", rows, include_avg, algos,
                    [](const ReportRow& r) -> const std::vector<Stat3>& { return r.enc_ms; });
  AppendMetricTable(out, "Decode Latency Comparison (ms)", rows, include_avg, algos,
                    [](const ReportRow& r) -> const std::vector<Stat3>& { return r.dec_ms; });
  return out.str();
}

}  // namespace polygon_codec::perftest
