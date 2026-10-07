# UTAU 输出引擎

调音模式选择 UTAU、界•UTAU 或谋•UTAU。三种模式分别读取普通 OTO、界 OTO 和谋 OTO，输出引擎独立选择，不改变音符、分段或调音数据。

在上方区域、轨道空白位置或左侧轨道标题处右键 → 选择输出引擎：

- 跟随设置：撤销本轨引擎及工具路径覆盖，使用设置 → 算法中的 UTAU 默认输出引擎与工具路径。
- HiFisampler（PC-NSF-HiFiGAN）：直接使用内置 ONNX 模型及所选模式的 OTO 分段。
- 重采样器…：打开配置窗口，选择本轨合成器／wavtool 与重采样器，点应用生效。留空路径使用全局设置；全局 wavtool 留空使用内置合成，全局重采样器留空使用随包 WCSNDM。

本轨选择优先于全局默认，支持工程保存和撤销／重做。移动工程时，也尝试解析工程相对路径；显式工具缺失或执行失败会显示错误。075 的原生 NSF 音源轨道自动保留为显式 PC-NSF-HiFiGAN 输出。普通音频素材仍可以选择 nsf-hifigan 调音算法。DiffSinger 和伴奏不提供这个 UTAU 输出引擎菜单。

PC-NSF-HiFiGAN 支持 OTO 时序、界／谋多段、手动分区、音高、响度、音头／尾包络及波形显示。已移植 hifisampler 的 g、Hb、Hv、Ht、HG、P、t、A、G、He，并支持全局、音符、分区和参数曲线。详见 [HiFisampler FLAG](hifisampler-flags.md)。WCSNDM 特有参数不属于该引擎的 FLAG。

外置 wavtool 处理已应用编辑器包络和交接的完整句段，负责最终封装；输入为兼容传统 wavtool 的单声道 PCM16，不重复套传统包络。全局默认也对无界面 MCP 生效。

MCP `track_update` 属性：`output_engine` 为 `inherit`、`resampler`、`pc-nsf-hifigan`；`output_resampler` 与 `output_wavtool` 为本轨工具路径。DiffSinger 使用自己的合成后端。
