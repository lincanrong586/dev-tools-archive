# Polygon Codec Library Implementation Plan
 
> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.
 
**Goal:** 将选型后的 PolygonSet 编解码实现整理成可业务复用的独立 C++ 编解码库，支持算法工厂/自描述编码、可选统计（时延/压缩效果）、并提供 UT。
 
**Architecture:** 新建独立目录 `polygon_codec/` 作为库工程；目录层级保持精简，只保留根目录公共框架文件、`algorithms/` 算法目录、`tests/` UT 目录。对外暴露统一 `Codec` API（指定算法编码、解码自动识别算法）；内部按“算法实现(独立 .h/.cc) + 框架层(包头/工厂/统计)”分层，保证新增/删除算法时只需要改工厂函数与算法文件。
 
**Tech Stack:** C++17；无第三方依赖（UT 使用项目内最小断言框架）；同时提供 `CMakeLists.txt` + `Makefile` + `build.sh`。
 
---
 
## 0. 需求复述与约束确认
 
**需求点映射：**
1. 独立构建代码库：仅保留库代码 + UT；移除 benchmark/main 等测试工具代码
2. 工厂能力：编码可指定算法；编码结果带算法类型；解码根据算法类型自动选择解码器
3. 统计能力：时延统计、压缩效果统计；可开关控制是否统计、是否打印；默认关闭
4. 注释：库代码需要中文详细注释
 
**关键约束：**
- 不引入外部测试框架（如 gtest）除非仓库已存在依赖；默认用自带最小 UT 框架
- 既要能在有 CMake 的环境构建，也要能在无 CMake 的环境构建（Makefile/build.sh）
- 编码格式需要版本号与魔数，便于后续升级兼容
 
---
 
## 1. 目标工程结构（新建 `polygon_codec/`）
 
```
/workspace/polygon_codec/
  CMakeLists.txt
  Makefile
  build.sh
  README.md
  types.h
  status.h
  codec.h
  codec.cc
  codec_factory.h
  codec_factory.cc
  codec_frame.h
  codec_frame.cc
  codec_metrics.h
  codec_metrics.cc
  varint.h
  varint.cc
  algorithms/
    algorithm_codec.h
    bin_fixed.h
    bin_fixed.cc
    bin_offset_varint.h
    bin_offset_varint.cc
    delta_varint.h
    delta_varint.cc
    delta_windowmin_varint.h
    delta_windowmin_varint.cc
    mini_pbuf.h
    mini_pbuf.cc
    boost_bin.h
    boost_bin.cc
  tests/
    test_codec.cc
```
 
**说明：**
- 根目录：公共框架文件（类型、状态、Codec API、包头、工厂、统计、varint）
- `algorithms/`：各算法实现；每个算法一个 `.h/.cc`，只负责 payload 的编解码，不负责包头/统计
- `algorithms/algorithm_codec.h`：算法统一接口，工厂通过它创建具体算法
- `codec_factory.*`：唯一需要感知“有哪些算法”的框架文件；新增/删除算法时只改这里
- `Algorithm` 枚举增加 `kButt`，用于判断算法个数与遍历边界
- `tests/`：UT（roundtrip、包头路由、统计开关）
 
---
 
## 2. 对外 API 设计（业务使用方式）
 
### 2.1 数据类型
 
文件：`/workspace/polygon_codec/types.h`
 
```cpp
namespace polygon_codec {
 
struct Point {
  long long x = 0;
  long long y = 0;
};
 
using Polygon = std::vector<Point>;
using PolygonSet = std::vector<Polygon>;
 
}  // namespace polygon_codec
```
 
### 2.2 算法枚举与编码结果
 
文件：`/workspace/polygon_codec/codec.h`
 
```cpp
namespace polygon_codec {
 
enum class Algorithm : uint8_t {
  kBinFixed = 1,
  kBinOffsetVarint = 2,
  kDeltaVarint = 3,
  kDeltaWindowMinVarint = 4,
  kMiniPbuf = 5,
  kBoostBin = 6,
  kButt = 7,
};
 
struct Metrics {
  bool enabled = false;
  double encode_ms = 0.0;
  double decode_ms = 0.0;
  size_t raw_bytes_estimated = 0;
  size_t encoded_bytes = 0;
  double bytes_per_polygon = 0.0;
  double compression_ratio = 0.0;
};
 
struct Options {
  bool enable_metrics = false;
  bool print_metrics = false;
};
 
struct EncodeOutput {
  Algorithm algorithm;
  std::vector<uint8_t> bytes;   // 包含统一包头 + payload
  Metrics metrics;
};
 
struct DecodeOutput {
  Algorithm algorithm;
  PolygonSet polygon_set;
  Metrics metrics;
};
 
class Codec {
 public:
  static EncodeOutput Encode(const PolygonSet& ps, Algorithm algo, const Options& opt = {});
  static DecodeOutput Decode(const std::vector<uint8_t>& bytes, const Options& opt = {});
};
 
}  // namespace polygon_codec
```
 
**业务侧示例：**
```cpp
using namespace polygon_codec;
 
Options opt;
opt.enable_metrics = true;
opt.print_metrics = false;
 
auto enc = Codec::Encode(ps, Algorithm::kBinOffsetVarint, opt);
Send(enc.bytes);
 
auto dec = Codec::Decode(enc.bytes, opt);  // 自动从包头识别算法
```
 
---
 
## 3. 编码帧（包头）设计：自描述 + 可扩展
 
文件：`/workspace/polygon_codec/codec_frame.h`
 
### 3.1 包头格式（建议）
 
```
magic(4B) = 'P''C''O''D'
version(1B) = 1
algorithm(1B) = Algorithm enum value
flags(1B) = 0（预留）
reserved(1B) = 0（对齐/预留）
payload_bytes(...) = 算法自有 payload
```
 
**原因：**
- 4 字节 magic + 1 字节版本可以在解码端快速校验数据是否属于该库、是否版本兼容
- algorithm 字段保证“解码自动路由”
- flags/reserved 为后续兼容扩展预留（例如是否压缩、是否加密等）
 
### 3.2 需要实现的函数
 
文件：`/workspace/polygon_codec/codec_frame.cc`
 
```cpp
namespace polygon_codec {
 
struct FrameHeader {
  Algorithm algorithm;
};
 
std::vector<uint8_t> WrapFrame(Algorithm algo, const std::vector<uint8_t>& payload);
StatusOr<FrameHeader> ParseHeader(const std::vector<uint8_t>& bytes);
StatusOr<std::vector<uint8_t>> ExtractPayload(const std::vector<uint8_t>& bytes);
 
}  // namespace polygon_codec
```
 
---
 
## 4. 工厂与路由：Encode 指定，Decode 自动
 
文件：`/workspace/polygon_codec/codec_factory.h`
 
```cpp
namespace polygon_codec {
 
class IAlgorithmCodec {
 public:
  virtual ~IAlgorithmCodec() = default;
  virtual Algorithm algorithm() const = 0;
  virtual std::vector<uint8_t> EncodePayload(const PolygonSet& ps) const = 0;
  virtual PolygonSet DecodePayload(const std::vector<uint8_t>& payload) const = 0;
};
 
class CodecFactory {
 public:
  static std::unique_ptr<IAlgorithmCodec> Create(Algorithm algo);
};
 
}  // namespace polygon_codec
```
 
**Decode 自动路由流程：**
1. `ParseHeader(bytes)` → 得到 `algo`
2. `CodecFactory::Create(algo)` → 得到对应 codec
3. `ExtractPayload(bytes)` → 得到 payload
4. `DecodePayload(payload)` → 得到 PolygonSet

**扩展要求落实方式：**
- 新增算法时：
  1. 在 `algorithms/` 下新增 `xxx.h/.cc`
  2. 在 `Algorithm` 枚举中新增一个值，并保持 `kButt` 在末尾
  3. 在 `codec_factory.cc` 的 `switch` 中增加一条 `case`
- 删除算法时：
  1. 删除对应 `algorithms/xxx.h/.cc`
  2. 删除 `Algorithm` 枚举项
  3. 删除 `codec_factory.cc` 中对应 `case`
- 除上述两处外，不要求修改其他框架文件
 
---
 
## 5. 统计能力（可选）：时延 + 压缩效果
 
文件：`/workspace/polygon_codec/codec_metrics.h`
 
### 5.1 指标定义
- **时延**：`encode_ms` / `decode_ms`（仅在 `Options.enable_metrics=true` 时测量）
- **压缩效果**：
  - `raw_bytes_estimated`：估算 PolygonSet 内存占用（与 benchmark 中的 `EstimatePolygonSetBytes` 一致口径）
  - `encoded_bytes`：编码输出字节数（包含包头）
  - `bytes_per_polygon = encoded_bytes / polygons`
  - `compression_ratio = encoded_bytes / raw_bytes_estimated`
 
### 5.2 打印策略
- 默认 `print_metrics=false`：不打印任何统计
- 若开启打印：由库统一打印一行（算法/大小/比率/时延），避免业务重复实现
 
---
 
## 6. 算法集合（从现有 polygon_bench 迁移）
 
**迁移目标：** 将以下实现从 `polygon_bench/src/` 迁移到库内 `polygon_codec/algorithms/`，并改成独立命名空间 `polygon_codec`。
 
按本轮需求，算法清单固定为 6 个：
- `bin_fixed`
- `bin_offset_varint`
- `delta_varint`
- `delta_windowmin_varint`
- `mini_pbuf`
- `boost_bin`

说明：
- 你的原文里 `bin_offset_varint` 出现了两次，这里按现有上下文解释为：保留 `delta_varint`
- 若你确实希望去掉 `delta_varint`、保留两份 `bin_offset_varint`，则需要你再明确一次；当前文档按“6 个不同算法”设计
 
对应源文件（参考）：  
`/workspace/polygon_bench/src/encoders/*`、`/workspace/polygon_bench/src/varint.*`、`/workspace/polygon_bench/src/polygon_types.h`
 
---
 
## 7. 单元测试（UT）规划
 
文件：`/workspace/polygon_codec/tests/test_codec.cc`
 
覆盖点：
1. **Roundtrip**：每种算法 `Decode(Encode(ps)) == ps`
2. **包头路由**：对同一份 ps，用不同算法编码后，解码能自动识别正确算法并还原
3. **版本/魔数错误**：构造非法 bytes，返回可识别错误（不崩溃）
4. **统计开关**：`enable_metrics=false` 时 metrics 为默认值；打开后 metrics 字段应被填充
 
UT 框架：复用当前 `polygon_bench/tests/test_roundtrip.cpp` 的最小断言风格（无外部依赖）。
 
---
 
## 8. 构建与交付
 
### 8.1 构建产物
- `libpolygon_codec.a`（静态库）
- `polygon_codec_tests`（UT 可执行）
 
### 8.2 构建方式
- `CMakeLists.txt`：优先支持
- `Makefile`：无 cmake 环境可编译
- `build.sh`：自动选择 CMake/Makefile（沿用当前 polygon_bench 的策略）
 
---
 
## 9. 任务拆解（TDD 执行顺序）
 
### Task 1: 创建新库工程骨架
 
**Files:**
- Create: `/workspace/polygon_codec/CMakeLists.txt`
- Create: `/workspace/polygon_codec/Makefile`
- Create: `/workspace/polygon_codec/build.sh`
- Create: `/workspace/polygon_codec/README.md`
- Create: `/workspace/polygon_codec/types.h`
- Create: `/workspace/polygon_codec/status.h`
- Create: `/workspace/polygon_codec/algorithms/`
- Create: `/workspace/polygon_codec/tests/`
 
- [ ] **Step 1: 写最小 README + 构建脚本骨架（不含实现）**
 
```bash
cd /workspace/polygon_codec
./build.sh --clean --test
```
 
- [ ] **Step 2: 运行并确认失败（缺少源文件/目标）**
 
Expected: 构建失败，提示目标/文件缺失
 
- [ ] **Step 3: 补齐最小可构建空库与空测试（只返回 0）**
 
```cpp
// tests/test_codec.cc
int main() { return 0; }
```
 
- [ ] **Step 4: 再次运行构建测试**
 
Expected: `ALL TESTS PASSED`（或测试可执行正常返回）
 
---
 
### Task 2: 定义对外 API（Codec/Options/Metrics）并写失败测试
 
**Files:**
- Create: `/workspace/polygon_codec/codec.h`
- Create: `/workspace/polygon_codec/codec.cc`
- Modify: `/workspace/polygon_codec/tests/test_codec.cc`
 
- [ ] **Step 1: 写失败测试（能调用 Codec::Encode/Decode，且 Decode 能识别算法字段）**
 
```cpp
using namespace polygon_codec;
 
static void Assert(bool ok, const char* msg);
 
void ApiSmokeTest() {
  PolygonSet ps = {{{{1,2},{3,4},{5,6}}}};
  Options opt;
  auto enc = Codec::Encode(ps, Algorithm::kBinFixed, opt);
  auto dec = Codec::Decode(enc.bytes, opt);
  Assert(dec.polygon_set == ps, "roundtrip");
  Assert(dec.algorithm == Algorithm::kBinFixed, "decode should detect algorithm from frame");
}
```
 
- [ ] **Step 2: 运行测试确认失败（未实现符号）**
 
Expected: 链接失败或断言失败
 
- [ ] **Step 3: 写最小实现让测试进入下一阶段失败点（先仅支持 BinFixed）**
 
- [ ] **Step 4: 运行测试确认通过**
 
---
 
### Task 3: 实现帧包头（magic/version/algo）与错误处理
 
**Files:**
- Create: `/workspace/polygon_codec/codec_frame.h`
- Create: `/workspace/polygon_codec/codec_frame.cc`
- Modify: `/workspace/polygon_codec/codec.cc`
- Modify: `/workspace/polygon_codec/tests/test_codec.cc`
 
- [ ] **Step 1: 写失败测试（篡改 magic/version 时 Decode 返回错误码）**
- [ ] **Step 2: 运行确认失败**
- [ ] **Step 3: 实现 Wrap/Parse/Extract**
- [ ] **Step 4: 运行确认通过**
 
---
 
### Task 4: 工厂与算法接口（IAlgorithmCodec + Create）
 
**Files:**
- Create: `/workspace/polygon_codec/codec_factory.h`
- Create: `/workspace/polygon_codec/codec_factory.cc`
- Create: `/workspace/polygon_codec/algorithms/algorithm_codec.h`
- Modify: `/workspace/polygon_codec/codec.cc`
- Modify: `/workspace/polygon_codec/tests/test_codec.cc`
 
- [ ] **Step 1: 写失败测试（Create 返回对应实现；未知算法返回错误）**
- [ ] **Step 2: 运行确认失败**
- [ ] **Step 3: 实现最小工厂（先注册 BinFixed，并验证“只改工厂即可挂接算法”）**
- [ ] **Step 4: 运行确认通过**
 
---
 
### Task 5: 迁移 varint 与六种算法实现
 
**Files:**
- Create: `/workspace/polygon_codec/varint.h`
- Create: `/workspace/polygon_codec/varint.cc`
- Create: `/workspace/polygon_codec/algorithms/bin_fixed.h`
- Create: `/workspace/polygon_codec/algorithms/bin_fixed.cc`
- Create: `/workspace/polygon_codec/algorithms/bin_offset_varint.h`
- Create: `/workspace/polygon_codec/algorithms/bin_offset_varint.cc`
- Create: `/workspace/polygon_codec/algorithms/delta_varint.h`
- Create: `/workspace/polygon_codec/algorithms/delta_varint.cc`
- Create: `/workspace/polygon_codec/algorithms/delta_windowmin_varint.h`
- Create: `/workspace/polygon_codec/algorithms/delta_windowmin_varint.cc`
- Create: `/workspace/polygon_codec/algorithms/mini_pbuf.h`
- Create: `/workspace/polygon_codec/algorithms/mini_pbuf.cc`
- Create: `/workspace/polygon_codec/algorithms/boost_bin.h`
- Create: `/workspace/polygon_codec/algorithms/boost_bin.cc`
- Modify: `/workspace/polygon_codec/codec_factory.cc`
- Modify: `/workspace/polygon_codec/tests/test_codec.cc`
 
- [ ] **Step 1: 写失败测试（6 个算法逐个 roundtrip，并校验 `Algorithm::kButt` 边界）**
- [ ] **Step 2: 运行确认失败**
- [ ] **Step 3: 逐个迁移实现（每迁移一个算法就跑一次测试）**
- [ ] **Step 4: 全部算法通过 roundtrip**
 
---
 
### Task 6: 增加 Metrics 统计与打印开关
 
**Files:**
- Create: `/workspace/polygon_codec/codec_metrics.h`
- Create: `/workspace/polygon_codec/codec_metrics.cc`
- Modify: `/workspace/polygon_codec/codec.cc`
- Modify: `/workspace/polygon_codec/tests/test_codec.cc`
 
- [ ] **Step 1: 写失败测试（enable_metrics=true 时 encode_ms/encoded_bytes 被填充）**
- [ ] **Step 2: 运行确认失败**
- [ ] **Step 3: 实现统计（chrono 计时 + raw_bytes 估算 + ratio 计算）**
- [ ] **Step 4: 实现 print_metrics（默认关闭）**
- [ ] **Step 5: 运行确认通过**
 
---
 
### Task 7: 中文注释补齐与 README 使用文档
 
**Files:**
- Modify: `/workspace/polygon_codec/*.h`
- Modify: `/workspace/polygon_codec/*.cc`
- Modify: `/workspace/polygon_codec/algorithms/*.h`
- Modify: `/workspace/polygon_codec/algorithms/*.cc`
- Modify: `/workspace/polygon_codec/README.md`
 
- [ ] **Step 1: 为对外 API、包头格式、工厂扩展点、6 种算法实现加入中文详细注释**
- [ ] **Step 2: README 补齐使用示例、构建方式、算法说明、兼容性说明**
- [ ] **Step 3: 全量测试保持通过**
 
---
 
## 10. Spec 覆盖自检
 
- [x] 独立构建库工程结构与目录规划（Task 1）
- [x] 工厂能力 + 自描述编码 + 解码自动路由（Task 2/3/4）
- [x] 统计与打印开关（Task 6）
- [x] 代码中文注释（Task 7）
- [x] UT 覆盖（Task 2/3/4/5/6）
 
---
 
## Execution Handoff
 
Plan complete and saved to `/workspace/docs/superpowers/plans/2026-07-03-polygon-codec-lib.md`. Two execution options:
 
1. Subagent-Driven (recommended) - I dispatch a fresh subagent per task, review between tasks, fast iteration
2. Inline Execution - Execute tasks in this session, batch execution with checkpoints
 
Which approach?
