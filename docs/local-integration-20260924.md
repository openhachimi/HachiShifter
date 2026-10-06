# HachiShifter 本机整合版 0.2.1

制作日期：2026-09-24。此版本是本机整合构建，不是上游正式发行版。

## 来源与合并方式

- 本机基础：`31335bd04329b575018319a42105ae1aa5c51ace`。
- GitHub `new` 固定版本：[24ccd963370029b1d719e7a1b16cc84e13bcbc42](https://github.com/openhachimi/HachiShifter/tree/24ccd963370029b1d719e7a1b16cc84e13bcbc42)，提交于北京时间 2026-09-24 00:28。
- 两边 Git 历史没有共同祖先。用本机 `639f294` 与上游初始导入 `574afb6` 的内容相似性定位移植起点，再对完整上游树做三方内容整合。合并中的 `6b8ae0c` 是明确标记的合成移植提交，不代表真实上游祖先关系。
- 整合源码位于 `E:/和声合成新UI/HachiShifter-integrated`，分支 `integration-20260924`。原 `HachiShifter-next` 和用户原 EXE 未覆盖。
- 原源码备份：工作区根目录 `HachiShifter-before-integration-31335bd-20260924.zip`；原完整运行包也仍在原位置。

## 合入的上游更新

- 原生素材分段 NativeSegment、原生连接 NativeConnection、HJM 标注读写，以及对应编辑行为。
- 原生 NSF-HiFiGAN 音源短语规划、F0 生成、两种处理顺序与混合路径；移除逐片段 RMS 抬升，保留上游响度一致性修复。
- 右侧停靠并可调宽度的资源管理器、素材注册/复制与音源导入流程。
- 深浅主题、Windows 软件绘制启动、JUCE/DXGI 等待修复、原生构建配置与上游文档。
- `--render-project` 命令行工程渲染。
- OTO 导出按采样条目合并，保留其他录音条目，以及 cutoff 和 HJM 写入规则修复。
- Melodyne Provider 检测框架；其真实原生导入/渲染接口仍沿用上游未实现状态。

## 保留本机功能并处理冲突

- 保留单音符 OTO、恢复音源 OTO、STP、原音试听、音源后台索引和写入后缓存失效。
- 保留完整共享音高时间线、相邻音符/单音符选区的音高求值、独立颤音终点、MIDI 指定轨导入与工程 MIDI 导出。
- 保留线性振幅包络、六组包络预设、包络基础值、逐音符渲染波形与实时整形，以及外部 HF 预热。
- 原生 NSF 的采样解析接入单音符 OTO；目标 F0 接入本机完整时间线；音量计算接入本机线性/对数包络。更改原生音符歌词会清除旧的单音符 OTO 和 STP。
- HJPX 同时持久化两边的数据字段；HJM 包络兼容保存本机线性插值标记。切换 NSF/UTAU 后保存再读取的组合测试通过。
- 菜单编号冲突已消除；颤音、包络基础值和原生连接菜单共存。UTAU 包络预设单独占一行，1280/1600 宽度下不再挤压音源/参数控件；浅色主题 BPM/拍号标签同步刷新。
- 汉字转拼音覆盖选中轨道，同时保留本机模型修改与撤销流程。
- 保留本机旧工程的实验性 MLD5 渲染兼容路径；它仍是独立实现，不等同官方 Melodyne。默认算法选择和隐藏未完成算法的界面策略沿用上游。
- flag 曲线/拆分编辑保留本机既有模式限制，避免统一界面改变原 UTAU 协议行为。
- 整合后主入口诊断用例较多，MSVC 栈大小设为 8 MiB，修复启动时默认栈不足的问题。

## 已执行验证

Windows x64、Visual Studio 2022、RelWithDebInfo，ONNX Runtime/DirectML 编译启用，构建成功。版本资源为 0.2.1。

- `python tools/integration-regression.py`：78/78 内置无参数 smoke 通过，包含两边已有回归及新增组合字段测试。
- `--smoke-integrated-ui`：深/浅主题 × 1280/1600 共四张截图；检查控件边界并人工查看 1280 版本。
- `--smoke-oto-hjm-discipline test`：注册不意外写 HJM、OTO 合并保留其他采样与 cutoff 规则通过。
- 在最终完整运行包中再次运行组合数据测试和自动定位真实外部引擎测试，均通过。
- 最终运行包通过 MCP 创建/保存/重开四音符工程，并通过新增 `--render-project` 调用真实 WCSNDM 引擎。输出 2 秒、48 kHz、24-bit、立体声 WAV，峰值 0.49147，后端 `utau-resampler+internal-wavtool`，无渲染警告。
- 原 EXE 的 SHA-256 复核未变：`D3577C1EA91E15468CA3970EF05AA9A392E69BAF4CFA23ED84747A182DC50051`。
- 整合 EXE 的 SHA-256：`C8F5AAA4E07EE0B133EEA91F2E33E2567FAAFC678D15592E7632C222E8FB6FA9`。

`--smoke-note-oto` 与 `--smoke-note-stp` 的合成夹具用原音高区分取样区域，因此通过不带外部引擎的同一 EXE 副本验证内部渲染。真实重采样器会把不同原音高都变为目标音高，不能用此夹具判定它失败。其他用例使用正常捆绑引擎版本，最终包另有真实音源渲染验证。没有放宽原来的频率断言。

日志、结果 JSON、截图与测试 WAV 位于源码目录的 `integration-tests/`。测试使用独立设置目录，不读取/改写用户日常设置；普通启动仍使用原应用设置位置。

## 运行与验证边界

直接运行工作区 `HachiShifter-整合版-0.2.1/HachiShifter Next.exe`。分发 ZIP 解压后应保留 EXE、DLL、engines、models 的目录关系。随包保留原发布包的外部 HF Python 环境、PyTorch checkpoint 和 UTAU 引擎。

本机没有找到上游原生 NSF 所需的 ONNX 模型包。原来的外部 HF PyTorch checkpoint 不能直接充当该 ONNX 模型。因此已验证原生 NSF 的规划、F0、混合和缺模型提示，但未完成原生神经网络推理/成品声音验证；使用此路径前仍需配置兼容模型。外部 HF 依赖被保留，本次没有重新做其完整神经合成性能/音质测试。

真实 Melodyne/ARA 引擎尚未由上游实现；整合不会把检测框架变成可用的官方合成引擎。自动测试也不能替代所有真实工程的试听；新数据结构的工程应保存为新文件，以便需要时继续使用原版。

## 继续构建

当前机器在源码目录执行 `./build-local.ps1` 可使用已配置的 ASCII 路径缓存增量构建，产物位于 `build-integrated/HachiShifterNext_artefacts/RelWithDebInfo/`。

全新环境需要 Visual Studio 2022 C++ 工具链、Windows SDK、CMake >=3.24，以及网络下载 JUCE 8.0.8/ONNX/DirectML 依赖。路径含非 ASCII 字符时先创建指向本源码的 ASCII junction，再把路径传给脚本，例如：

```powershell
New-Item -ItemType Junction -Path C:/hachi-integrated-src -Target $PWD.Path
./build-local.ps1 -SourceLink C:/hachi-integrated-src -Reconfigure
```

不要让此 junction 指向旧仓库。JUCE 依赖会应用上游补丁，应使用整合版自己的副本。当前本机使用 `.deps/juce-src`，ONNX/DirectML 使用旧构建已下载的只读依赖；需要重配时可传 `-DependencyRoot C:/hachi-src/build-onnx/_deps`。源码 ZIP 不含可重新下载的依赖和大型编译中间文件。
