# 标准 UTAU：中文 CVVC 发音器

适用版本：0.2.4，内部更新 112（2026-10-08）。这是根据 OpenUtau 中文 CVVC 规则适配的原生 C++ 发音器；不是加载 OpenUtau 插件 DLL。

## 使用

1. 轨道选择标准 **UTAU**，选择带有 `oto.ini` 和 `presamp.ini` 的中文 CVVC 音源。
2. 右键轨道或区域 → **发音器 → 中文 CVVC**。旧工程与新轨道仍默认 **手动别名**；可随时切回，切换支持撤销。
3. 每个音符填写一个拼音音节，也支持现有拼音表中的单个汉字。选择需要试听的音符后渲染。
4. 输出引擎可以是外部重采样器，也可以是 HiFisampler（PC-NSF-HiFiGAN）。两者使用同一份发音编排。

设置按轨道保存在工程中。当前只在标准 UTAU 生效；切换到界、谋或 DiffSinger 时停用，保存的发音器选择不会丢失。

## 音源要求

音源根目录的 `presamp.ini` 至少提供 `[VOWEL]` 和 `[CONSONANT]`；支持 `[REPLACE]` 的精确替换。示例（必须与实际 OTO 别名相符）：

```ini
[VOWEL]
a=A=a,ba,pa=100
i=I=i,bi,pi=100
[CONSONANT]
b=ba,bi=1
p=pa,pi=1
[REPLACE]
bah=ba
```

支持 UTF-8 及音源的传统文本编码设置；在没有显式编码设置、默认解码失败时，对中文 presamp 增加 GBK 重试。配置文件不会被改写。配置缺失或无法构成有效规则时，渲染保留手动别名并报告原因。

不需要 `oto4.ini` 或 `otomou.ini`。普通 `oto.ini` 的先行发声、重叠和 `prefix.map` 是发音安排的基础。

## 实际行为

- 先尝试“前元音 + 当前音节”的完整连接别名，再尝试 VC + CV；句首的前元音为 `-`。
- 例如相邻歌词 `a → ba`、音源存在对应条目时，内部安排为 `a → a b → ba`。VC 属于后一个歌词音符，位置可以在其名义起点之前。
- VC 时长参考当前 CV 的先行发声、零／负重叠、辅音速度、局部 BPM，并限制在前一个音符的可用时长内。
- 句末存在 `元音 R` 时自动安排收尾；显式 `R` 或 `-` 尝试使用前元音的收尾音。`RR` 保持本编辑器的休止符含义。
- 缺少 VC 时使用 CV；缺少 CV 时报告缺失并保持该段静音，不把任意单素材错误当成 CV。原手动模式的单素材回退仍保留。
- 多音高音源先按 `prefix.map` 查找。VC 使用前音符音高选择录音，CV 使用当前音符音高；已经解析的别名不会再次套用前后缀。
- 空隙和休止符切断相邻发音上下文。含空格的完整别名或 `[别名]` 可以直接指定素材、绕过自动拆分。
- 原歌词、音符 ID、位置和音符数量不改变。生成片段保持父音符的音高／FLAG／包络时间轴，并在渲染后归并成原音符的波形；片段分界可见，悬停显示别名。
- 框选试听保留同一区域内未选音符的上下文和 OTO 衔接参数，但不合成、不播放这些未选音符。发音方式、音符上下文、presamp 和 prefix.map 变更会影响缓存。

## 当前边界

本版针对**一音符一音节、同一区域内的中文 CVVC**。汉字使用编辑器现有的常用单字读音，不具备 OpenUtau 的完整上下文多音字转换；建议多音字直接填写拼音。未实现多字歌词自动分配、`+` 连音组展开、跨区域自动发音衔接，以及 OpenUtau voice color/subbank 元数据。

生成的发音片段目前用于合成、显示和悬停提示；没有另加独立的逐音素时长编辑器。可通过原音符参数、歌词别名与单音 OTO 覆盖调整。本功能不等于日语 CVVC、韩语 CVC 或其他语言发音器。

## 代码与验证入口

- `backend/ChineseCvvcPhonemizer.h`：presamp 解析、别名／时长编排、曲线时钟转换、父音符波形回调聚合。
- `backend/UtauRenderer.*`：严格 OTO 查询、已解析别名通道、传统引擎接入。
- `backend/NsfHifiganRenderer.cpp`：同一发音器接入内置 HiFisampler；未选上下文只保留静音交接。
- `AudioEngine.cpp`：轨道开关、选区上下文、缓存文件戳；`ProjectModel.*`：撤销及 `utauPhonemizer=zh-cvvc/manual` 持久化。
- `MainComponent.cpp` / `I18n.cpp` / `TrackListComponent.cpp` / `PianoRollComponent.cpp`：菜单、翻译、轨道标识及生成片段显示。
- `tests/ChineseCvvcSmoke.h` / `Main.cpp`：`--smoke-chinese-cvvc <输出目录> [NSF模型目录]`。带模型目录时同时验证内置 HiFisampler，并查找该配布下的 `engines/WCSNDM-0.0803.exe` 验证外部合成。

相关来源：[OpenUtau 中文 CVVC 源码](https://github.com/openutau/OpenUtau/blob/master/OpenUtau.Plugin.Builtin/ChineseCVVCPhonemizer.cs)、[发音器说明](https://github.com/openutau/OpenUtau/wiki/Phonemizers)。移植来源与 MIT 许可保存在 `third_party/openutau/`。
