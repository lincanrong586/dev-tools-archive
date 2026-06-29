#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bench.h"

namespace {

uint32_t ParseU32(const char* s) {
  char* end = nullptr;
  const unsigned long v = std::strtoul(s, &end, 10);
  if (!end || *end != '\0') throw std::runtime_error("invalid integer: " + std::string(s));
  if (v > 0xFFFFFFFFUL) throw std::runtime_error("integer out of range: " + std::string(s));
  return static_cast<uint32_t>(v);
}

int32_t ParseI32(const char* s) {
  char* end = nullptr;
  const long v = std::strtol(s, &end, 10);
  if (!end || *end != '\0') throw std::runtime_error("invalid integer: " + std::string(s));
  if (v < INT32_MIN || v > INT32_MAX) throw std::runtime_error("integer out of range: " + std::string(s));
  return static_cast<int32_t>(v);
}

void PrintUsage() {
  std::cout
      << "polygon_bench usage:\n"
      << "  polygon_bench [--polygons N] [--points-per-polygon N] [--cut-size-um N] [--region-size-um N] [--iterations N]\n"
      << "examples:\n"
      << "  ./polygon_bench --polygons 50000 --points-per-polygon 6 --cut-size-um 1000 --iterations 3\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    polybench::GenerateConfig gen_cfg;
    polybench::BenchConfig bench_cfg;

    for (int i = 1; i < argc; ++i) {
      const std::string arg = argv[i];
      if (arg == "--help" || arg == "-h") {
        PrintUsage();
        return 0;
      }
      if (i + 1 >= argc) throw std::runtime_error("missing value for: " + arg);
      const std::string val = argv[++i];

      if (arg == "--polygons") gen_cfg.polygons = ParseU32(val.c_str());
      else if (arg == "--points-per-polygon") gen_cfg.points_per_polygon = ParseU32(val.c_str());
      else if (arg == "--cut-size-um") gen_cfg.cut_size_um = ParseI32(val.c_str());
      else if (arg == "--region-size-um") gen_cfg.region_size_um = ParseI32(val.c_str());
      else if (arg == "--iterations") bench_cfg.iterations = ParseU32(val.c_str());
      else throw std::runtime_error("unknown arg: " + arg);
    }

    std::vector<polybench::BenchRow> rows;
    for (auto scenario : {polybench::ScenarioKind::NearOrigin, polybench::ScenarioKind::Middle, polybench::ScenarioKind::Far}) {
      gen_cfg.scenario = scenario;
      rows.push_back(polybench::RunScenarioBench(gen_cfg, bench_cfg));
    }

    std::cout << polybench::RenderMarkdownTable(rows, true);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}

