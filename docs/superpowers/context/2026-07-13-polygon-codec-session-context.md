## 目标

本文件用于将本阶段实现的关键上下文（需求、决策、落地结果）固化到仓库中，便于后续维护与交接。

## 当前工程形态

- /workspace 为 git 仓库根目录
- polygon_codec 为业务可复用编解码库
- polygon_codec/bench-perftest 为性能对比工具（random_points 与 mask_like 两类数据集）

## 本阶段新增需求：坐标刻度编码到数据中

用户明确要求：刻度精度是 point(x, y) 的取值单位，需要编码到数据中。

约束与预期：

- 允许的刻度集合：0.1nm/1nm/10nm/100nm/1um
- 默认：0.1nm
- 解码方无需外部上下文即可获知刻度
- 尽量不修改各算法 payload 的 wire format

## 关键实现

### 1) frame header 加入 coordinate_scale（version=2）

- header 长度仍为 8 字节
- version 升级为 2
- 新增 coordinate_scale 字段，编码写入，解码读取
- 兼容 version=1：默认将 v1 的 scale 视为 1nm

相关文件：

- polygon_codec/codec_frame.h
- polygon_codec/codec_frame.cc

### 2) Codec API 透传刻度

- Options 新增 coordinate_scale（默认 0.1nm）
- EncodeOutput/DecodeOutput 增加 coordinate_scale 字段
- Encode 将其写入 frame header；Decode 从 frame header 读取并返回

相关文件：

- polygon_codec/codec.h
- polygon_codec/codec.cc

### 3) bench-perftest 支持 --scale，并按 scale 生成数据

- GenerateConfig 增加 coordinate_scale
- --scale 解析并透传给 GeneratePolygonSet 与 Codec::Encode
- MaskLike/RandomPoints 中把 um 输入转换为 ticks（point(x,y) 的整数单位）

相关文件：

- polygon_codec/bench-perftest/generate.h
- polygon_codec/bench-perftest/generate.cc
- polygon_codec/bench-perftest/main.cc

### 4) UT 覆盖

- 新增用例验证：
  - v2 header 能携带 scale 且解码可取回
  - v1 数据默认按 1nm 解释

相关文件：

- polygon_codec/tests/test_codec.cc

## 风险点与边界说明

- coordinate_scale 仅作为语义元数据，不会自动对坐标数值做缩放；业务侧如需物理单位换算，应以 DecodeOutput.coordinate_scale 为准进行解释
- v1 的默认行为固定为 1nm，避免将旧数据误解释为更细的 0.1nm
