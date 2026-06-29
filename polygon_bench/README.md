# polygon_bench

用于对比 PolygonSet（`std::vector<std::vector<Point>>`，Point(x,y) 为 `int32`，单位 um）的多种编码传输格式的大小与编解码时延。

## 支持的编码格式

本工具内置实现（均为自包含，无第三方依赖）：

- `pbuf`：最小化 protobuf 兼容实现（对应 [polygon.proto](schemas/polygon.proto) 的 wire-format）
- `bin_fixed`：自定义二进制（varint 写入数量 + little-endian int32 点坐标）
- `bin_offset_varint`：自定义二进制（先写 min_x/min_y，再写相对坐标的 varint），用于消除“离原点远导致编码变大”的问题
- `delta_varint`：自定义二进制（每个 polygon 内做 delta + ZigZag varint），用于利用点序列的局部连续性
- `boost_bin`：Boost.Serialization 的 `binary_oarchive/binary_iarchive`（可选，检测到 Boost 后自动启用）

## 场景生成方式（与业务场景对齐）

区域：100mm×100mm（默认 `region_size_um=100000`）。

截取：一个正方形窗口（默认 `cut_size_um=1000`），并生成 N 个 polygon，每个 polygon 固定 K 个点（默认 6 点）。

场景：

- `near_origin`：窗口左下角在 (0,0) 附近
- `middle`：窗口在整区域中心附近
- `far`：窗口在 (region_size_um-cut_size_um, region_size_um-cut_size_um) 附近

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
./build_make/polygon_bench --polygons 50000 --points-per-polygon 6 --cut-size-um 1000 --iterations 3
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
./build/polygon_bench --polygons 50000 --points-per-polygon 6 --cut-size-um 1000 --iterations 3
# 或
./build_make/polygon_bench --polygons 50000 --points-per-polygon 6 --cut-size-um 1000 --iterations 3
```

输出为 Markdown 表格，字段含义：

- `poly(MB)`：估算的 PolygonSet 内存占用（包含外层/内层 vector 的对象数组与点坐标数组，不含 allocator 元数据）
- `xxx(MB)`：编码后的字节数（MB）
- `xxx_enc(ms)` / `xxx_dec(ms)`：单次平均编码/解码耗时（ms）
- `rss(MB)`：当前进程 VmRSS（MB）

## 运行单元测试（验证编解码一致性）

```bash
./build/polygon_bench_tests
# 或
./build_make/polygon_bench_tests
```
