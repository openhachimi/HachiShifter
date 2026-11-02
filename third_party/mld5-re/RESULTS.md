# 既有 Melodyne 5 原版素材：最终复现结果

## 结论

在提供的历史 `a` 素材、neutral/±5/±12 半音五组对照及下述明确条件中，独立 C++17 调音 DSP 已完成频谱级近一致复现：

- **neutral：新 WAV 与原版 WAV 全文件 SHA-256 相同。**
- **四组移调：99.37%–99.84%的 PCM16 样本码完全相同；全部剩余误差严格只有±1LSB，没有任何>1LSB样本。**
- 全文件 STFT 幅度 log-MAE 为0–0.00573dB；未做延迟对齐、增益拟合、EQ、归一化、参考重采样或验收裁剪。

**这是既有五组参考条件下的调音 DSP 复现，不是整个 Melodyne 5、任意素材、所有参数或原始分析/分离流程的等价声明。** 输入仍是已核对的原版分量 PCM、周期/音高缓存与明确的调用上下文；没有用近似检测器补齐未恢复环节。

## 同格式全文件结果

每组均为 mono48kHz、27648帧、0.576秒，PCM16LE。STFT：Hann，N=256/1024/4096，hop=N/4，参考峰值相对−80dB floor。完整逐点残差、输入/输出 SHA 和全部15组 STFT 指标在[最终编码报告](<tests/pcm16_conversion_encoded_report.json>)中。

| 移调 | 完全相同样本 /27648 | 最大误差 | 全文件 RMSE | 三分辨率幅度 log-MAE |
|---|---:|---:|---:|---:|
| neutral |27648（100%）|0|0|0dB|
|+5 |27483（99.403%）|1LSB|2.358e−6|0.00163–0.00236dB|
|−5 |27605（99.844%）|1LSB|1.204e−6|0.000638–0.00142dB|
|+12 |27481（99.396%）|1LSB|2.372e−6|0.00258–0.00573dB|
|−12 |27474（99.371%）|1LSB|2.421e−6|0.00270–0.00488dB|

四组移调 SNR 为90.10–97.60dB。它们合计549个不同样本全部逐项公开，均为±1码；没有隐藏小样本例外。neutral 的[复现 WAV](<tests/pcm16_conversion_encoded_base.wav>) SHA 为`abe4a5c75374005002511d9d61860c6cadec1427d81e6854800d27128b7c9c47`，与冻结原版完全相同。

## 不是通过后处理拟合得到

1. **整帧 OLA：** 原指令要求最终两半窗口、ADD L+H、跳过尾部 COPY，而非“左ADD/右COPY”。[720组分支差分](<notes/processed-ola-correction.md>)支持该修正。
2. **音高缓存：** 保留 float 音分常量、两遍平滑及逐点舍入，不用理想 double 幂常量替代。[缓存证据](<notes/pitch-cache.md>)及[控制接口](<notes/control-clock.md>)保留算法和有限域。
3. **源时间反馈：** 原版四结点恒等曲线仍以139样本预算计算端点/计数并反馈 double 步长，不能替换成 `S += Fs/Fo`。约1.54e−8的源位置差会改变整数取整及快速滑音 grain 选择。[源时间证明](<notes/source-time-control.md>)解释了原−12大残差的修复。
4. **末组件退出：** 通过原版父级调度、分组、单次入队与pop逻辑，证明该principal待处理队列为0→1→0；源阈值为18432−80=18352。触发后按原 float 增益递推退出，同时冻结源刷新。不是从音频拟合尾部增益，也不是对旧 WAV 直接套包络。[实际条件证明](<notes/endpoint-predicate.md>)、[输出增益实现](<src/output_gain_control.hpp>)及[重新生成轨迹报告](<tests/output_gain_render_report.json>)均公开。
5. **PCM16格式：** 完整原转换函数181579DF0的 float→word 路径为float乘32768、向零截断、存16bit。它不是nearest-round或32767归一化。该转换由原指令确认，再用于新测试文件；编码器不接收参考文件参数，无法复制参考样本。[转换证明](<notes/pcm16-conversion.md>)明确其有限域及非saturating行为。

Float32 DSP 输出与PCM16参考的直接比较仍单独保留在[增益报告](<tests/output_gain_render_report.json>)，RMSE约1.56–1.77e−5、最大误差<2LSB。最终表是在**匹配原版输出格式之后**的结果，不混淆这两个比较层次。

## 独立验证及构建

- Lead 干净构建：14个生产模块；加入格式单元后 **15/15 CTest 通过**。
- Lead 独立重生五组 gain/WAV、NULL及keep回归；旧raw/scalar输出及事件 SHA不变。
- ASan/UBSan通过。
- [原rolling读出](<notes/rolling-readout.md>)：25257120个float比较，0bit差异。
- [真实帧轨迹](<notes/native-frame-trajectory.md>)：1580次原9970调用，最大支持区差异2.38e−7，标志0失配。
- [原driver数值组合](<notes/native-driver-numeric.md>)：3240事件及实际负向重点事件，状态/几何/读入参数0失配。
- [完整PCM16转换](<tests/pcm16_conversion_native_result.json>)：3103623样本比较、5172753 guard检查，0失配，原转换路径无数学/对象替代桥。

见[构建与调用](<BUILDING.md>)和[CMake工程](<CMakeLists.txt>)。最高精度principal入口是 `render_period_component_source_time_gain`。生产库不调用原DLL；原DLL只用于独立的完整性校验原指令测试。未修改或集成进HachiShifter；没有再操作Melodyne GUI，全部复用历史测试过程与文件。

## 仍未扩大声明的边界

当前组合入口限定Fo48k/D1/mono、恒等源时间结点、一个显式调用区间、globalPitch1、显式恒定Q、已声明的bootstrap与末组件策略。采样分离/检测、任意非中性shaper、reference-phase、joins、632/其它模式、非单位modulation/drift及任意工程上下文未在本结果中全部重建。两种有来源的时间上下文此前给出相同音频，但没有声称观察了历史exporter的实时堆。剩余549个±1LSB样本也不被说成逐bit完整等价。
