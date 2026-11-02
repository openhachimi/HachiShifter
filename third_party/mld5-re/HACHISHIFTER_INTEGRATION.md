# HachiShifter 接入说明

本文说明如何把当前可接受版本的独立 Melodyne 5 调音 DSP 接入 HachiShifter。**本文件只提供接入说明；本次逆向工程没有修改 HachiShifter。**

## 1. 代码位置与构建

项目目录：`melodyne5-re/`

推荐以 CMake 子目录方式接入：

```cmake
add_subdirectory("/absolute/path/to/melodyne5-re"
                 "${CMAKE_BINARY_DIR}/melodyne5-re")
target_link_libraries(hachishifter PRIVATE melodyne_re::dsp)
```

库是纯 C++17 静态库，不依赖 Melodyne DLL、Windows、IDA、Python 或网络。必须保留严格浮点选项：

- GCC/Clang：`-fno-fast-math -ffp-contract=off`
- MSVC：`/fp:strict`

当前生产模块由 [`CMakeLists.txt`](<CMakeLists.txt>) 显式列出；默认便携测试为 **15/15 CTest 通过**。构建细节见 [`BUILDING.md`](<BUILDING.md>)。

## 2. 推荐入口

HachiShifter 应调用：

```cpp
#include "output_gain_control.hpp"

melodyne_re::OutputGainRenderResult result =
    melodyne_re::render_period_component_source_time_gain(
        component_pcm,
        periods,
        playback_curve,
        source_time_curve,
        renderer_parameters,
        caller_context,
        gain_parameters);
```

返回值：

- `result.ok`：成功状态；失败时读取 `result.error`。
- `result.samples`：已应用原版末组件增益退出的单个 principal/component float 音频。
- `result.raw`：同一次渲染产生的源时间控制、周期渲染和事件记录。
- `result.records`：触发输出索引、阈值、fade divisor、每个输出的实际 gain 和最终 gain 状态。

不要把旧 WAV 作为输入再套 gain；该入口会在同一次源时间轨迹中生成 raw 音频，并在触发后冻结 source refresh。

## 3. 必须提供的输入

当前入口不是项目解析器、音高检测器或 source separator。HachiShifter 必须先提供：

1. **已分离的 component PCM**
   - principal component：原始分析采样率，本验证素材为 32 kHz mono。
   - 必须保证 float PCM 的 source ownership、起点和长度正确。
2. **周期数组**
   - 正的 source-period 值及其 control resolution。
   - 本验证素材为 441 Hz 控制数组，共 254 个值。
3. **播放 pitch-ratio 曲线**
   - 明确的 control resolution。
   - 不在库内重新检测 F0。
4. **source-time 曲线**
   - 当前已验证入口要求严格递增、`y=x` 的四个 identity knots：
     - `0`
     - `0.031843750000000004`
     - `0.39259375`
     - `0.5760000000000001`
   - 这不是把曲线简化成两点；原版的 endpoint/count 反馈必须保留。
5. **caller 时间上下文**
   - `begin_time`、`end_time`、`output_block_count`、`element_start`。
   - Fo=48 kHz 时 `source_refresh_budget` 必须为 `min(139, output_block_count)`。
   - `admit_pre_startup_source_refresh=true` 是当前已验证入口要求。
6. **renderer 参数**
   - `output_sample_rate=48000`。
   - mono、D1、global pitch 1、`clip_output=false`。
   - 当前验证路径的 Q 仍由 caller 以 `probe_ratio` 显式提供；不要假定库已恢复一般 Q/formant automation。
7. **末组件状态**
   - `signal_end`：source sample 坐标中的 signal endpoint；本 principal fixture 为 `18432`。
   - `pending_queue_empty`：由 Hachi 的调度器明确提供，不要从音频分数推断。
   - `last_component_zero_remainder=true` 只应在确认这是最后一个 component 且允许尾部清零时使用。

## 4. 关键参数示例

```cpp
melodyne_re::PeriodRendererParameters p;
p.source_sample_rate = 32000.0;
p.output_sample_rate = 48000.0;
p.output_count = 27648;
p.probe_ratio = q_from_hachi;
p.clip_output = false;

melodyne_re::SourceTimeRenderContext caller;
caller.time.begin_time = 0.0;
caller.time.end_time = 0.5760000000000001;
caller.time.output_block_count = 27648;
caller.time.element_start = 0.0;
caller.source_refresh_budget = 139;
caller.rate_external_budget = 27648;
caller.admit_pre_startup_source_refresh = true;

melodyne_re::OutputGainRenderParameters gain;
gain.signal_end = 18432.0;
gain.source_feature = false;
gain.inhibit_322 = false;
gain.pending_queue_empty = true;       // 仅在Hachi已证明队列为空时
// 最后一个component才设为true：
gain.last_component_zero_remainder = true;
```

`SourceTimeRenderContext::after_output_action` 是受限 seam：gain 入口会独占它；不要在 caller 上同时注册另一个 after-output owner。read-only `observe_before_output` 可以继续使用并会被 gain 入口链式调用。

## 5. component 混音顺序

入口返回的是单 component principal float。HachiShifter 侧按自己的已验证 source topology 组合：

1. 调用 principal 的 `render_period_component_source_time_gain`。
2. 单独渲染/保留 sibilant component；不要把 sibilant 当作 principal pitch component。
3. 按现有 HachiShifter channel/pan/mix 规则相加。
4. 最后再执行 Hachi 自己的 WAV/PCM16 写出。

本次验证使用冻结的 principal+sibilant 分量；没有把分析/分离逻辑伪装成 Melodyne 重建。`melodyne5-re` 不自动读取 MPD、自动分离 source、自动解析 project 或自动混音。

## 6. 输出格式

验证后的 PCM16 写出路径是原版的：

```text
float32 sample * 32768.0f
CVTTSS2SI / 向零截断
低16位写入 little-endian int16
```

这是原函数 `181579DF0` 在有限域 `[-1,1)` 中的指令证据，不是通用 saturating writer。超出该域时原生路径会发生低 16 位 wrap，因此 Hachi 接入时应在写出前确保输入满足范围，或自行实现并明确记录自己的 clipping policy。测试用编码器位于 [`tests/pcm16_conversion_format.hpp`](<tests/pcm16_conversion_format.hpp>)，**不是生产 DSP 模块，也不应通过参考 WAV 复制样本码**。

## 7. 已验证结果

五组冻结历史对照（mono 48 kHz、27648 帧、0.576 s）：

| 编辑 | PCM16 完全相同样本 | 最大码差 | float/PCM16 全文件 RMSE |
|---|---:|---:|---:|
| neutral | 27648/27648 | 0 | 0 |
| +5 | 27483/27648 | 1 LSB | 2.358e-6 |
| -5 | 27605/27648 | 1 LSB | 1.204e-6 |
| +12 | 27481/27648 | 1 LSB | 2.372e-6 |
| -12 | 27474/27648 | 1 LSB | 2.421e-6 |

四组移调剩余 549 个样本全部只有 ±1 LSB；没有任何超过 1 LSB 的样本。neutral 生成 WAV 与历史原版 WAV 全文件字节一致。完整 SHA、STFT 和逐点差异见 [`RESULTS.md`](<RESULTS.md>) 与 [`tests/pcm16_conversion_encoded_report.json`](<tests/pcm16_conversion_encoded_report.json>)。

这 549 个 ±1 LSB 差异分布在约 0.01–0.46 秒，并非末尾尾音问题。它们已通过只读分析逐点列出，但当前没有把它们归因给某一个量化策略，也没有用它们拟合参数；不要在 HachiShifter 侧复制或修正这些差异。详见 [`notes/post-gain-residual.md`](<notes/post-gain-residual.md>)。

可直接用于听感检查的五组文件：

- [`pcm16_conversion_encoded_base.wav`](<tests/pcm16_conversion_encoded_base.wav>)
- [`pcm16_conversion_encoded_up5.wav`](<tests/pcm16_conversion_encoded_up5.wav>)
- [`pcm16_conversion_encoded_down5.wav`](<tests/pcm16_conversion_encoded_down5.wav>)
- [`pcm16_conversion_encoded_up12.wav`](<tests/pcm16_conversion_encoded_up12.wav>)
- [`pcm16_conversion_encoded_down12.wav`](<tests/pcm16_conversion_encoded_down12.wav>)

## 8. 接入时不要做的事

- 不要启动或调用 Melodyne 原 DLL。
- 不要再通过 Melodyne GUI 导出或点击操作。
- 不要把旧 WAV 作为输入再后处理 gain。
- 不要用 `S += Fs/Fo` 替代 source-time endpoint/count 反馈。
- 不要把 cached-R-only、旧 scalar 或 neutral-only 结果当成最高精度入口。
- 不要把本入口扩展成任意 632、joins、reference-phase、非单位 modulation/drift、一般 shaper 或完整分析器，除非另有原始 IDA 证据。
- 不要修改 HachiShifter 的历史逆向记录；接入应在 Hachi 侧单独提交。

## 9. 验证命令

在 `melodyne5-re` 目录：

```sh
cmake -S . -B /tmp/melodyne-re-build -DCMAKE_BUILD_TYPE=Release
cmake --build /tmp/melodyne-re-build --parallel 2
ctest --test-dir /tmp/melodyne-re-build --output-on-failure

python3 tests/output_gain_render_run.py --gain
python3 tests/pcm16_conversion_native_run.py
python3 tests/pcm16_conversion_encode_run.py --sanitize
python3 tests/pcm16_conversion_encode_run.py
```

最终边界和未恢复模式见 [`RESULTS.md`](<RESULTS.md>)。