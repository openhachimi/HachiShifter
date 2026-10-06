# DiffSinger 使用完整性检查 · 0.2.3

后续状态：更新 003 已完成旧式 depth 修正、加载预检和模型/声音缓存；更新 009 已补中英文发音转换、读音覆盖、用户词典及模型 retake 局部重生成；GPU、质量档位和完整第三方发音器插件执行层仍属后续事项。详见 [更新记录](updates.md)。以下保留检查时的原始分析。

检查日期：2026-09-26。本文为现状评估和待办，不表示下列补充已经实现，也不改变对外版本号。

结论：现有版本能完成受支持音源的 MIDI/歌词输入、预测音高、手动修音、多参数曲线、播放和 WAV 导出。要作为日常完整的 DS 编辑器使用，应先补兼容性和发音编辑，再完善模型复用、局部重生成和工程交换。

本机 OpenUtau 位于 `F:/OpenUtau/OpenUtau.exe`，文件版本为 `0.1.565+a60ca5830b9064556157245d4bf8f5920d93e5f8`。其用户目录为 `C:/Users/Asuna/Documents/OpenUtau`；检查时 Singers 目录为空，额外音源目录为空。设置中的声学/variance/pitch 步数分别为 20/20/10，tensor cache 已启用，OnnxRunner 为空。此次读取安装信息和设置，并对照官方教程、公开源码；未在 OpenUtau 内进行同曲目声音对比。上游 master 中的新功能不能视为本机 0.1.565 已有功能。

## 已有能力和验证范围

- 音源自动识别、语言词典和默认音色选择；普通 UTAU 与 DS 曲线分开保存。
- 时长预测、音高预测、手动 pitch 合成、撤销和保存；选中音符可以只写回选中部分。
- DYN、GENC、VELC、ENE、BREC、TENC、VOIC、PEXP、SHFC 和动态音色混合的接口已接入，按音源能力启用。控制种类已覆盖[官方 DS 参数说明](https://github.com/openutau/OpenUtau/wiki/DiffSinger-support)的主要项目。
- 现有真实模型验证集中于 Umidaji：DYN、GENC、VELC、PEXP 和五种音色混合。该音源没有对应 variance 表现参数及 SHFC 能力；这些接口的替身测试不能替代其他真实音源验证。

## 优先级 1：影响兼容性或日常编辑

### A. 修正旧采样接口，并建立音源预检

`juce/engines/diffsinger/bridge.py:71` 固定设置 `steps=20`、`speedup=50`，直接把配置的 `max_depth` 转成输入所需数据类型。旧式整数 depth 的量纲应进行换算。

使用真实 `Model.run` 和模拟 ONNX session 检查：当 `max_depth=0.5` 且输入为 int64 时，现实现传入 `depth=0, speedup=50`；按上游 20 步逻辑应为 `depth=500, speedup=25`。这是输入构造问题的复现，未用旧式真实音源测量其听感影响；不能据此断言已验证的 Umidaji 受到影响。[上游采样换算实现](https://github.com/openutau/OpenUtau/blob/master/OpenUtau.Core/DiffSinger/DiffSingerRenderer.cs)

建议：分开处理连续/整数采样接口；加载音源时检查模型文件、输入、词典和声码器配套情况，并给出具体缺失项。优先加入一个旧接口音源及一个支持四种 variance 参数的真实音源验证。

### B. 完善歌词到音素的转换

现在支持词典命中、单音素、纯 `[音素 音素]`、基本 `+/-` 延音及 SP/AP。没有 OpenUtau 发音器插件执行层，中文为逐字拼音表，未知英文词和多语转换没有完整 G2P 支持。

在 Umidaji 词典上检查：`ni` 和 `[zh/n zh/i]` 接受；`ni[zh/n zh/i]`、`+2` 被拒绝。需要补歌词与发音提示分离、读音覆盖、多音节词的音符分配、多语映射及用户词典。`+2` 等记法应按目标发音器语义处理，不能统一当作延长前一个元音。[官方发音器教程](https://github.com/openutau/OpenUtau/wiki/Phonemizers)、[DS 发音器实现](https://github.com/openutau/OpenUtau/blob/master/OpenUtau.Core/DiffSinger/DiffSingerBasePhonemizer.cs)

### C. 增加 DS 音素时长编辑通道

更新 004 已实现：谋•UTAU 内的 DS 专用音素编辑窗口、边界拖动/数值输入、保持元音拍点、恢复预测、工程保存/撤销，以及各预测器与合成共享时长覆盖。以下为实施前评估记录。

当前时长预测结果只留在 Python 内部，编辑器没有用于保存并传回 DS 的独立音素边界覆盖数据；返回给编辑器的声音片段按音符切片，并把前置时间设为 0。用户不能直接调整某个辅音早晚、元音起点或尾辅音位置。VELC 控制声学发音速度，不能替代明确的音素边界修改。

建议：显示实际预测音素及边界，支持拖动和数值修改；将覆盖值随工程保存，并让 pitch、variance、声学预测共享同一时间布局。已有 UTAU OTO/四分区控件不能直接代替此功能。

## 优先级 2：影响流畅度和反复调教

### D. 复用模型，并提供预览/导出质量选项

现有编辑器有音频渲染缓存，并非完全没有缓存。但每个新 DS 请求都会启动临时 Python 进程，进程内会话不能跨请求保留；声码器还会在每个乐句的 `render` 中重新初始化。适配器固定 CPU，编辑器其他算法的 GPU 选项不会自动接入该适配器。

建议先复用一个音源的后台模型、缓存中间预测结果，再接 DirectML 或 CUDA、失败回退 CPU，以及空闲卸载/内存上限。增加快速预览和高质量导出设置，分别控制 pitch、variance、声学采样。减少只修改 DYN、音量等后处理参数时的模型重算。性能收益仍需在本机实测。[上游会话复用](https://github.com/openutau/OpenUtau/blob/master/OpenUtau.Core/DiffSinger/DiffSingerSinger.cs)

### E. 真正的局部 pitch 重生成

当前 `generateDiffSingerPitch` 收集整轨音符，推理时 `retake` 全为 true，结果返回后才筛选选中音符。现有已编辑 pitch 没有作为锁定上下文传给预测器。PEXP 修改后也要手动再次生成；生成时任何工程修改都会令结果过期。

建议：增加“保留手调/重生成所选区域”，传入已有 pitch 和 retake 掩码，显示哪些音符需要重生成；按相关轨道/乐句判断结果是否过期。基础预测与手工修正分层可作为下一步，避免重生成覆盖用户修音。[上游当前局部预测实现](https://github.com/openutau/OpenUtau/blob/master/OpenUtau.Core/DiffSinger/DiffSingerPitch.cs)（此参考来自 master，不代表本机旧版已提供相同界面。）

### F. 乐句连续性与进度反馈

适配器按间隙或约 15 秒分组，长串延音会绕过该长度切分。输入布局检查显示，40 个连续一秒普通音符被分为 16/16/8，而相同时长的 `+` 延音串保持一组。自动长度切分会改变模型上下文；长延音又可能产生更大推理负载。此次没有测量切口的声音，尚不能断言存在爆音。

建议：用合理断句和重叠上下文切块，核对跨片段连音、首辅音及句尾保留；加入乐句进度、已完成片段试听和局部重试。现有取消和后台执行已经实现，应继续保留。

## 优先级 3：完整工作流

- USTX 导入/导出：当前源码没有 USTX 读写路径。分阶段保留音符、歌词、曲速、pitch、DF 表现曲线和音素覆盖；不支持的字段明确提示。普通 MIDI/UST 交换不能等同于保留 OpenUtau 调教。[官方 USTX 格式](https://github.com/openutau/OpenUtau/wiki/USTX-file-format)
- 音源管理：选择根目录后的完整预检、声码器导入入口、最近使用音源与音源预设；可读取现有 OpenUtau 的配置作为迁移便利，不应擅自修改其音源文件。
- 曲线操作：跨音符绘制/连续性、批量复制参数、预设，以及可用时显示模型预测的基础 ENE/BREC/TENC/VOIC 曲线，帮助理解“在预测值上加减”的含义。
- 验证矩阵：补齐多语言、多音节、快辅音、长句、跨片段延音、不同帧率/采样接口、模型缺失、真实 variance 和可控 pitch 声码器、CPU/GPU 输出差异测试。不同帧率子模型目前会被主动拒绝，属于已知兼容范围。

建议实施次序：A 兼容修复与预检 → B/C 发音及音素编辑 → D 模型复用 → E 局部重生成；F 应贯穿真实歌曲验证，最后再扩展工程交换和操作便利。

本次输入检查记录：`integration-tests/diffsinger-audit-20260926/contracts.json`。既有声音验证见 `docs/diffsinger.md` 及对应测试报告；本次没有新增真实音频 A/B 测试。
