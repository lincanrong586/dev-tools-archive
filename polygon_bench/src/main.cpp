#include <cstdlib>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>

#include "bench.h"

namespace {

// 解析无符号整数命令行参数。
//
// 这里统一做格式校验，避免后面散落着重复的错误处理代码。
uint32_t ParseU32(const char* s) {
  char* end = nullptr;
  const unsigned long v = std::strtoul(s, &end, 10);
  if (!end || *end != '\0') throw std::runtime_error("invalid integer: " + std::string(s));
  if (v > 0xFFFFFFFFUL) throw std::runtime_error("integer out of range: " + std::string(s));
  return static_cast<uint32_t>(v);
}

// 解析有符号 32 位整数命令行参数。
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
      << "  polygon_bench [--polygons N] [--points-per-polygon N] [--cut-width-um N] [--cut-height-um N]\n"
      << "                [--cut-size-um N] [--region-size-um N] [--iterations N]\n"
      << "\n"
      << "output:\n"
      << "  Prints two reports:\n"
      << "    1) random_points (legacy)\n"
      << "    2) mask_like (nm grid + polygon clipping)\n"
      << "examples:\n"
      << "  ./polygon_bench --polygons 50000 --points-per-polygon 6 --cut-width-um 73728 --cut-height-um 147456 --iterations 100\n"
      << "  ./polygon_bench --polygons 50000 --points-per-polygon 6 --cut-size-um 1000 --iterations 100\n";
}

}  // namespace

int main(int argc, char** argv) {
  try {
    // 生成参数与基准参数分开存放：
    // - gen_cfg 控制数据规模、窗口位置和窗口尺寸；
    // - bench_cfg 控制编码/解码测试迭代次数。
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

      // 注意：
      // - polygons 现在表示“目标 polygon 个数”，每轮会围绕该值随机扰动；
      // - points-per-polygon 现在表示“目标平均点数”，而不是固定每个 polygon 都一样。
      if (arg == "--polygons") gen_cfg.polygons = ParseU32(val.c_str());
      else if (arg == "--points-per-polygon") gen_cfg.points_per_polygon = ParseU32(val.c_str());
      else if (arg == "--cut-width-um") gen_cfg.cut_width_um = ParseI32(val.c_str());
      else if (arg == "--cut-height-um") gen_cfg.cut_height_um = ParseI32(val.c_str());
      else if (arg == "--cut-size-um") gen_cfg.cut_size_um = ParseI32(val.c_str());
      else if (arg == "--region-size-um") gen_cfg.region_size_um = ParseI32(val.c_str());
      else if (arg == "--iterations") bench_cfg.iterations = ParseU32(val.c_str());
      else throw std::runtime_error("unknown arg: " + arg);
    }

    auto run_all_scenarios = [&](polybench::GenerateKind kind) {
      std::vector<polybench::BenchRow> rows;
      gen_cfg.kind = kind;
      for (auto scenario : {polybench::ScenarioKind::NearOrigin, polybench::ScenarioKind::Middle, polybench::ScenarioKind::Far}) {
        gen_cfg.scenario = scenario;
        rows.push_back(polybench::RunScenarioBench(gen_cfg, bench_cfg));
      }
      return rows;
    };

    std::cout << "## random_points\n\n";
    std::cout << polybench::RenderMarkdownTable(run_all_scenarios(polybench::GenerateKind::RandomPoints), true);
    std::cout << "## mask_like\n\n";
    std::cout << polybench::RenderMarkdownTable(run_all_scenarios(polybench::GenerateKind::MaskLike), true);
    return 0;
  } catch (const std::exception& e) {
    std::cerr << "error: " << e.what() << "\n";
    return 1;
  }
}
