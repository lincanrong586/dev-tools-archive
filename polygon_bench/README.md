# polygon_bench

用于对比 PolygonSet（`std::vector<std::vector<Point>>`，Point(x,y) 为 `long long`，单位 um）的多种编码传输格式的大小与编解码时延。

## 支持的编码格式

本工具内置实现（均为自包含，无第三方依赖）：

- `pbuf`：最小化 protobuf 兼容实现（对应 [polygon.proto](schemas/polygon.proto) 的 wire-format）
- `bin_fixed`：自定义二进制（varint 写入数量 + little-endian int64 点坐标）
- `bin_offset_varint`：自定义二进制（先写 min_x/min_y，再写相对坐标的 varint），用于消除“离原点远导致编码变大”的问题
- `delta_varint`：自定义二进制（每个 polygon 内做 delta + ZigZag varint），用于利用点序列的局部连续性
- `boost_bin`：Boost.Serialization 的 `binary_oarchive/binary_iarchive`（可选，检测到 Boost 后自动启用），并对 `Point` 启用按位可序列化优化

## 场景生成方式（与业务场景对齐）

区域：100mm×100mm（默认 `region_size_um=100000`）。

截取：一个矩形窗口，默认宽高为 `(1024 * 72) x (2048 * 72) um`（即 `73728 x 147456 um`）。

随机生成规则（默认）：

- 单次生成的总点数在 `250000 ~ 350000` 之间随机波动，目标中心约 `300000`
- `polygons` 参数表示“目标 polygon 个数”，默认 `50000`，每次生成都会在该值附近随机扰动
- `points_per_polygon` 参数表示“目标平均点数”，默认 `6`
- 实际每个 polygon 的点数不是固定值，而是在以下约束下随机分配：
  - 最少 `3` 个点
  - 最多 `10000` 个点
  - 所有 polygon 的点数总和仍严格落在 `250000 ~ 350000` 之间

兼容性说明：

- 新参数：`cut_width_um`、`cut_height_um`
- 兼容旧参数：`cut_size_um`
- 当传入 `cut_size_um` 时，会覆盖宽/高并退化为正方形窗口

场景：

- `near_origin`：窗口左下角在 (0,0) 附近
- `middle`：窗口在整区域中心附近
- `far`：窗口在 `(region_size_um-cut_width_um, region_size_um-cut_height_um)` 附近

注：生成的 polygon 仅用于编码性能测试（不校验几何“闭环/不相交”性质），以便把测试重点放在“数据格式化选型”的编解码与大小对比。

## 构建

需要：Linux + g++(C++17)。

### 方式 1：使用 CMake

需要：`cmake(>=3.16)`。

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### 方式 2：使用 Makefile（适合无 cmake 环境）

仓库已附带 `Makefile`，只要本机有 `g++` 和 `make` 即可：

```bash
make -j
```

生成文件位于：

- `build_make/polygon_bench`
- `build_make/polygon_bench_tests`

### 方式 3：使用 build.sh（推荐）

仓库已附带 `build.sh`，会自动检测本机是否存在 `cmake`：

- 有 `cmake`：走 `./build`
- 无 `cmake`：自动回退到 `Makefile`，走 `./build_make`

```bash
chmod +x build.sh
./build.sh
./build.sh --test
./build.sh --clean --test
```

### openEuler 22.03 环境说明

你提供的本地环境为：

- 有：`gcc/g++ 10.3.1`、`make 4.3`
- 缺：`cmake`

因此优先建议直接使用 `Makefile`：

```bash
make -j
./build_make/polygon_bench_tests
./build_make/polygon_bench --polygons 50000 --points-per-polygon 6 --cut-width-um 73728 --cut-height-um 147456 --iterations 100
```

或者直接使用自动脚本：

```bash
chmod +x build.sh
./build.sh --test
./build.sh
```

如果你仍想安装 `cmake`，可尝试：

```bash
sudo yum install -y cmake
```

或：

```bash
sudo dnf install -y cmake
```

安装后再执行 CMake 方式构建。

### Boost 安装（用于启用 boost_bin）

如果你希望对比 `boost_bin`，需要安装 Boost 的 Serialization 组件（头文件 + `libboost_serialization`）：

```bash
sudo yum install -y boost-devel
```

安装完成后：

- CMake 构建会自动检测并启用 `boost_bin`
- Makefile 构建会自动检测 `/usr/include/boost/...` 并启用 `boost_bin`

如果你的 Boost 安装在非默认路径，可用：

```bash
make WITH_BOOST=1 BOOST_INC=/path/to/boost/include BOOST_LIB=/path/to/boost/lib -j
```

## 运行基准测试

```bash
./build/polygon_bench --polygons 50000 --points-per-polygon 6 --cut-width-um 73728 --cut-height-um 147456 --iterations 100
# 或
./build_make/polygon_bench --polygons 50000 --points-per-polygon 6 --cut-width-um 73728 --cut-height-um 147456 --iterations 100
```

如需沿用旧版正方形窗口参数，也可以继续使用：

```bash
./build_make/polygon_bench --polygons 50000 --points-per-polygon 6 --cut-size-um 1000 --iterations 100
```

参数说明：

- `--polygons`：目标 polygon 个数，不再表示“严格固定值”
- `--points-per-polygon`：目标平均点数，不再表示“每个 polygon 固定 N 个点”
- `--iterations`：默认 `100`，且每轮都会重新生成一份规模随机不同的 PolygonSet

输出为 2 组报告（每组各 5 个 Markdown 表）：

- `random_points`：历史随机点集生成方式（旧版逻辑）
- `mask_like`：nm 栅格 + 复杂轮廓 + 矩形裁剪补点闭环（更贴近 mask 版图轮廓点序列）

每组报告包含 5 个 Markdown 表：

- `Dataset Overview`：打印每个场景的 `polygons`、`points`、`poly(MB)`
- `Size Comparison (MB)`：比较不同编码格式的结果大小
- `Average Encoded Length Per Object (bytes/object)`：比较平均每个 polygon 的编码长度
- `Encode Latency Comparison (ms)`：比较编码时延
- `Decode Latency Comparison (ms)`：比较解码时延
- 每一行场景内，最优值用绿色加粗标记，最差值用红色加粗标记

说明：

- 输出场景包含 `near_origin`、`middle`、`far`，以及汇总行 `avg`
- `Average Encoded Length Per Object` 目前按“每个 polygon 平均编码字节数”计算
- 每个数值以 `avg (min~max)` 形式输出，min/max 来自 `--iterations` 轮数据的统计
- 若检测到 Boost，则编码对比表中会自动增加 `boost_bin` 列
- Markdown 中使用 HTML `span` 做颜色标记，便于在支持富文本的查看器中直接区分最优/最差

## 运行单元测试（验证编解码一致性）

```bash
./build/polygon_bench_tests
# 或
./build_make/polygon_bench_tests
```
