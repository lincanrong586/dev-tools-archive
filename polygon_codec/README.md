# polygon_codec

该目录提供 PolygonSet 的业务可复用编解码库（独立于 polygon_bench 工具），支持多算法选择、编码自描述、解码自动路由，以及可选统计开关。

## 目录结构

- `algorithms/`：算法实现（每个算法独立 `.h/.cc`）
- `tests/`：UT 测试
- 根目录：公共框架（`codec.*`、`codec_frame.*`、`codec_factory.*`、`varint.*` 等）

## 算法列表

- `bin_fixed`：定长 int64 little-endian
- `bin_offset_varint`：全局最小值偏移 + uvarint64
- `delta_varint`：polygon 内增量 + ZigZag + uvarint64
- `delta_windowmin_varint`：全局最小值 + 首点偏移 + 后续增量
- `pbuf`：最小 protobuf wire-format 兼容实现（不依赖 libprotobuf）
- `boost_bin`：Boost.Serialization binary archive（需要编译时启用 Boost）

## 业务使用方式

```cpp
#include "codec.h"
#include "types.h"

using namespace polygon_codec;

PolygonSet ps = {
    {
        {1, 2},
        {3, 4},
        {5, 6},
    },
};

Options opt;
opt.enable_metrics = false;
opt.print_metrics = false;

auto enc_or = Codec::Encode(ps, Algorithm::kBinOffsetVarint, opt);
if (!enc_or.ok()) {
  // 失败原因：enc_or.status().message()
}

auto dec_or = Codec::Decode(enc_or.value().bytes, opt);
if (!dec_or.ok()) {
  // 失败原因：dec_or.status().message()
}
```

## 构建与测试

```bash
./build.sh --clean --test
```

## 性能对比（random_points vs mask_like）

构建后运行：

```bash
./build/polygon_codec_bench --iterations 100
```

也可通过 Makefile：

```bash
make bench
./build_make/polygon_codec_bench --iterations 100
```

## Boost 说明

- CMake：若检测到 Boost.Serialization，会自动启用 `boost_bin`
- Makefile：默认自动探测系统 Boost 头文件；也可显式指定：

```bash
make WITH_BOOST=1
make WITH_BOOST=1 test
```
