## 背景

polygon_codec 目前将 Point(x, y) 作为 long long 整数进行编解码与传输。历史上，bench-perftest 的 MaskLike 数据集默认将 um 输入转换为 nm 栅格（1um=1000nm），但该“刻度”只存在于生成逻辑与文档口径中，编码数据本身不携带单位语义。

本次需求明确：刻度精度是 point(x, y) 的取值单位，必须编码到数据中，解码方可在不知道外部上下文的情况下恢复刻度语义。

## 需求

- 编码数据需要携带 point(x, y) 的取值单位（刻度精度）
- 可选刻度：0.1nm / 1nm / 10nm / 100nm / 1um
- 默认刻度：0.1nm
- 解码时能够获取该刻度信息
- 需要兼容已有数据：旧版本数据缺少刻度字段时，解码要有明确默认行为

## 设计概览

采用“帧级元数据”的方式，在 polygon_codec 的自描述 frame header 中加入 coordinate_scale 字段：

- 不修改各算法 payload 的 wire format（payload 仍只表达整数坐标值与拓扑结构）
- coordinate_scale 作为 frame header 字段，描述 payload 内整数坐标的单位含义
- header 固定长度仍为 8 字节，升级 frame version 以区分旧格式

## CoordinateScale 枚举

新增枚举 CoordinateScale（存储为 uint8_t）：

- kNm0p1 = 1
- kNm1 = 2
- kNm10 = 3
- kNm100 = 4
- kUm1 = 5

## Frame Header v2

header 长度仍为 8 字节，布局更新如下：

- magic(4): 'P''C''O''D'
- version(1): 2
- algorithm(1): Algorithm
- coordinate_scale(1): CoordinateScale
- reserved(1): 0

## 兼容策略（v1 → v2）

为兼容历史数据：

- ParseHeader 支持 version=1 与 version=2
- 对 version=1：header 不包含 coordinate_scale 字段，解码时强制使用 CoordinateScale::kNm1 作为默认值

说明：v1 的默认选择为 1nm，是为了与历史“nm 栅格”口径更接近，并避免把 v1 数据误解为更细的 0.1nm。

## Codec API 变更

新增与透传字段：

- Options.coordinate_scale：编码时写入 frame header（默认 0.1nm）
- EncodeOutput.coordinate_scale：返回编码所使用的刻度
- DecodeOutput.coordinate_scale：返回 frame header 解析出来的刻度

行为：

- Encode：仅写入 header，不对 payload 数值做缩放
- Decode：仅解析并返回 scale，不对 payload 数值做缩放

业务侧如果需要将整数坐标转成物理单位，应按 DecodeOutput.coordinate_scale 做单位解释/换算。

## bench-perftest 变更

- GenerateConfig 增加 coordinate_scale
- main 增加命令行参数：--scale 0.1nm/1nm/10nm/100nm/1um
- RandomPoints/MaskLike 生成时，将 um 输入转换为“ticks/um”，由 coordinate_scale 决定：
  - 0.1nm: 10000 ticks/um
  - 1nm: 1000 ticks/um
  - 10nm: 100 ticks/um
  - 100nm: 10 ticks/um
  - 1um: 1 ticks/um
- bench 输出标题中附带 scale 便于对比

## 测试

新增 UT 覆盖：

- v2：编码后 ParseHeader 可读出 coordinate_scale；DecodeOutput 携带同值
- v1：将已编码字节的 version 字节强制改为 1 后，ParseHeader/DecodeOutput 均默认返回 1nm

## 后续可扩展项

- 如果未来需要让 scale 参与压缩（例如利用已知刻度对坐标做归一化/量化），可以在算法层增加“按 scale 选择更紧凑编码”的新算法，但应与当前算法集合解耦，并按需新增算法枚举与工厂注册。
