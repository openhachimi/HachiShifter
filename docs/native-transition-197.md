# 197：衔接音独立登记、Melodyne 自动识别、自然音高走势与 MIDI

本轮仍是本地开发修改，公开版本 0.25、WCSNDM 0.0803；未上传 GitHub、未修改 `r`。东京泰迪熊没有渲染或检查。

## 衔接音

`NativeTransitionShape` 是独立的源元数据：只有严格位于完整源覆盖内部的中间分界，不使用 C、V、P 或先行。左右边缘继续使用已有 `NativePronunciationEdges`，由同一 CVVC fade／overlap 权威渲染；衔接音不会进入同源非完全切割链、物理 overlap 连接或 source-edge 连续所有权。

OTTO 精确编辑器选择“过渡发音”后：

- 隐藏辅音、先行、简化辅音和元音分段控件；
- 显示中间分界（源毫秒），可数字输入或拖动已有原生分界；
- 仍可编辑左淡入终点和右淡出起点；
- 保存／冷重开保留分界、边缘和完整物理源；
- 不凭别名、文件名或音高连接自动拼接。

## Melodyne 导入

设置→文件导入新增默认开启的“自动注册衔接音（Melodyne 导入）”。识别器只使用实际源片段：短、完全切割、两端能量较高且中间有明显能量谷；存在物理连接、部分 pitch join、稳定有声短元音或长度不明确时保守不注册。检测可以是长文件中的源切片，也可以是独立文件；文件名不参与判断。

注册只写身份类型和中间分界。导入项目原有 source clock、contour、F0、gain、fade、Melodyne join 和音频边界不被自动类型标签改写。注册后的 OTTO 行再生成独立衔接音边缘默认，但不会在原始导入快照上偷偷激活新边缘增益。

## 原生换素材音高

新 score-material 绑定不再把 UTAU 导入目标线当成新素材的音高目标。精确源票据完成后，沿现有 owner clock 读取 GAME/FCPE 已测完整源事实：源中心移到目标乐谱音高，保留相对 pitch detail、颤音和非直线走势，并经过相同 source-time map。真实无声仍无声，不造默认 C4。已有用户明确绘制的原生 pitch controls 会保留；该策略有持久标志，旧工程默认不改变。

## MIDI

MIDI 全文件和单轨入口共享同轨精确 onset 的歌词解析。仅唯一的 Lyric meta 事件保留文本；无歌词、空文本、普通 Text meta、和弦、重复歌词、跨轨或前后 tick 不确定时写 `-`。MIDI 仍建立原生 Hachi 轨并保留 `.mid/.midi` source identity，随后复用既有素材选择／替换流程。

## 针对性验证

- `--smoke-native-transition-shape`：分界、两侧 elastic、OTTO/HJPX 冷重开、非连续链拒绝。
- `--smoke-native-transition-detection`：控制 WAV 能量谷、完全切割／部分连接／稳定有声拒绝、长录音切片、开关前后 request fingerprint、一致的精确编辑器控件和默认导入选项。
- `--smoke-native-source-trend`：控制源事实下中心移调、走势保留、冷重开和后续用户编辑保护。
- `--smoke-native-midi-material`：歌词与缺省 `-`、MIDI 时钟／undo／全文件和单轨一致性。

控制 WAV 只用于逻辑测试，不宣称 GAME/FCPE 识别准确率。冻结的 update196 teio 基线和模型哈希保存在 `validation/native-transition-197`；最终新版本仍需只对 teio 做真实 CPU PCM 对照后再安装，未验收不发布。
